// engine.cpp — phase-locked light + audio.
//
// The audio task is the clock. Every I2S block advances elapsed time by
// (AUDIO_FRAMES / sample_rate), works out the entrainment rate from the
// session curve, and generates the tone. The LED duty comes from the SAME
// entrainment phase that sets the binaural beat (or the isochronic gate), so
// light and sound are one oscillator read twice. Nothing can drift.
#include "engine.h"
#include "config.h"
#include <driver/i2s.h>
#include <math.h>
#include <string.h>
#include <algorithm>

EntrainmentEngine Engine;

static constexpr double kTwoPi = 6.283185307179586;
static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
static constexpr float kStopFadeS = 1.5f;   // fade-out when stopped
static constexpr float kCarrierTauS = 2.0f; // tone glide time between steps

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// ---------------------------------------------------------------- noise beds
namespace {
  struct Noise {
    uint32_t rng = 0x1234567u;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0; // pink
    float brown = 0;                                                // brown
    float lp1 = 0, lp2 = 0;                                         // rain

    inline float white() {                       // xorshift32 -> -1..1
      rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
      return (int32_t)rng * (1.0f / 2147483648.0f);
    }
    inline float pink() {                        // Paul Kellet's filter
      float w = white();
      b0 = 0.99886f * b0 + w * 0.0555179f;
      b1 = 0.99332f * b1 + w * 0.0750759f;
      b2 = 0.96900f * b2 + w * 0.1538520f;
      b3 = 0.86650f * b3 + w * 0.3104856f;
      b4 = 0.55000f * b4 + w * 0.5329522f;
      b5 = -0.7616f * b5 - w * 0.0168980f;
      float out = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f;
      b6 = w * 0.115926f;
      return out;
    }
    inline float brownN() {
      brown = (brown + 0.02f * white()) / 1.02f;
      return brown * 3.5f;
    }
    inline float rain() {                        // band-limited hiss
      float w = white();
      lp1 += 0.07f * (w - lp1);                  // low-pass
      lp2 += 0.004f * (lp1 - lp2);               // slow part to subtract
      return (lp1 - lp2) * 1.6f;
    }
  };
  Noise noiseL, noiseR;
  double oceanPhase = 0;

  enum Bed { BED_NONE, BED_PINK, BED_BROWN, BED_OCEAN, BED_RAIN };
  Bed bedFrom(const String& s) {
    if (s == "pink" || s == "pink-noise") return BED_PINK;
    if (s == "brown") return BED_BROWN;
    if (s == "ocean") return BED_OCEAN;
    if (s == "rain" || s == "soft-rain") return BED_RAIN;
    return BED_NONE;
  }
  enum Tone { TONE_BINAURAL, TONE_ISO, TONE_CHAKRA };
  Tone toneFrom(const String& s) {
    if (s == "isochronic") return TONE_ISO;
    if (s == "chakra") return TONE_CHAKRA;
    return TONE_BINAURAL;
  }
}

// -------------------------------------------------------------------- setup
void EntrainmentEngine::begin() {
  mux_ = xSemaphoreCreateMutex();

  // LEDC for the two eye LEDs (high-freq PWM carrier, we modulate duty).
  ledcAttach(PIN_LED_LEFT,  LED_PWM_FREQ_HZ, LED_PWM_BITS);
  ledcAttach(PIN_LED_RIGHT, LED_PWM_FREQ_HZ, LED_PWM_BITS);
  ledcWrite(PIN_LED_LEFT, 0);
  ledcWrite(PIN_LED_RIGHT, 0);

  // I2S master TX to the PCM5102A. The legacy driver's dma_buf_* fields are
  // marked deprecated by the newer core (built with -Werror), so scope that off.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = (i2s_bits_per_sample_t)AUDIO_BITS;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = AUDIO_FRAMES;
  cfg.use_apll = false;             // APLL is not available on the ESP32-S3
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
#pragma GCC diagnostic pop

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;   // PCM5102 makes its own clock (SCK to GND)
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCLK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);

  xTaskCreatePinnedToCore(audioTaskTrampoline, "entrain-audio", 6144, this,
                          configMAX_PRIORITIES - 2, &audioTaskH_, 1);
}

// ---------------------------------------------------------- session timeline
void EntrainmentEngine::evalAt(float t, float& hz, size_t& idx, float& carrier) const {
  const float base = session_.audio.carrier_hz;
  hz = 0; idx = 0; carrier = base;
  if (session_.steps.empty()) return;
  float acc = 0;
  float prevHz = session_.steps.front().hz;
  const size_t n = session_.steps.size();
  for (size_t i = 0; i < n; ++i) {
    const Step& st = session_.steps[i];
    float dur = st.minutes * 60.0f;
    if (t < acc + dur || i == n - 1) {
      float into = t - acc;
      float h = st.hz;
      if (into < st.fade_s && st.fade_s > 0) {
        float k = into / st.fade_s;          // linear glide from previous step
        h = prevHz + (st.hz - prevHz) * k;
      }
      hz = clampf(h, SAFE_FLASH_HZ_MIN, SAFE_FLASH_HZ_MAX);
      idx = i;
      carrier = st.carrier_hz > 0 ? st.carrier_hz : base;
      return;
    }
    acc += dur;
    prevHz = st.hz;
  }
}

// ----------------------------------------------------------- cross-task API
bool EntrainmentEngine::start(const Session& s) {
  if (s.steps.empty()) return false;
  if (state_ != EngineState::Idle || hasPending_) return false;   // busy
  xSemaphoreTake(mux_, portMAX_DELAY);
  pending_ = s;
  hasPending_ = true;
  xSemaphoreGive(mux_);
  return true;
}

void EntrainmentEngine::stop() {
  if (state_ == EngineState::Running) state_ = EngineState::Stopping;
}

EngineStatus EntrainmentEngine::status() const {
  EngineStatus st;
  st.state = (state_ == EngineState::Idle && hasPending_) ? EngineState::Running : state_;
  st.sessionId = String(id_);
  st.elapsed_s = elapsed_s_;
  st.total_s = total_s_;
  st.current_hz = curHz_;
  st.carrier_hz = curCarrier_;
  st.step_index = curStep_;
  return st;
}

// -------------------------------------------------------------- audio task
void EntrainmentEngine::audioTaskTrampoline(void* self) {
  static_cast<EntrainmentEngine*>(self)->audioTask();
}

void EntrainmentEngine::adoptPending() {
  xSemaphoreTake(mux_, portMAX_DELAY);
  session_ = pending_;
  hasPending_ = false;
  xSemaphoreGive(mux_);

  strncpy(id_, session_.id.c_str(), sizeof(id_) - 1);
  total_s_ = session_.totalSeconds();
  elapsed_s_ = 0;
  phaseL_ = phaseR_ = entrainPhase_ = 0;
  gain_ = 0; stopT_ = 0; oceanPhase = 0;
  float hz; size_t idx; float car;
  evalAt(0, hz, idx, car);
  carrierCur_ = clampf(car, 50.0f, 1000.0f);
  state_ = EngineState::Running;
}

void EntrainmentEngine::audioTask() {
  static int16_t block[AUDIO_FRAMES * 2]; // interleaved L,R
  const double dt = 1.0 / AUDIO_SAMPLE_RATE;
  const float blockDt = AUDIO_FRAMES * (float)dt;
  const float slew = 1.0f - expf(-blockDt / kCarrierTauS);

  for (;;) {
    if (hasPending_ && state_ == EngineState::Idle) adoptPending();

    if (state_ == EngineState::Idle) {
      memset(block, 0, sizeof(block));
      size_t wrote;
      i2s_write(I2S_PORT, block, sizeof(block), &wrote, portMAX_DELAY);
      continue;
    }

    const float t = elapsed_s_;
    float hz, targetCarrier; size_t idx;
    evalAt(t, hz, idx, targetCarrier);
    carrierCur_ += (clampf(targetCarrier, 50.0f, 1000.0f) - carrierCur_) * slew;
    curHz_ = hz; curCarrier_ = carrierCur_; curStep_ = (uint32_t)idx;

    const Tone tone = toneFrom(session_.audio.tone);
    const Bed bed = bedFrom(session_.audio.soundscape);
    const double wL = kTwoPi * carrierCur_;
    const double wR = kTwoPi * (carrierCur_ + hz);
    const double wE = kTwoPi * hz;

    // Target gain at the end of this block: soft start, and a fade on stop.
    float gEnd = clampf((t + blockDt) / SAFE_SOFTSTART_S, 0.0f, 1.0f);
    if (state_ == EngineState::Stopping) {
      stopT_ += blockDt;
      gEnd = std::min(gEnd, clampf(1.0f - stopT_ / kStopFadeS, 0.0f, 1.0f));
    }
    const float gStart = gain_;
    const float toneVol = clampf(session_.audio.volume, 0.0f, 1.0f) * 0.7f;
    const float bedVol  = clampf(session_.audio.noise_vol, 0.0f, 1.0f) * 0.5f;

    for (int n = 0; n < AUDIO_FRAMES; ++n) {
      const float g = gStart + (gEnd - gStart) * ((float)n / AUDIO_FRAMES);
      const float pL = (float)phaseL_, pR = (float)phaseR_;
      float l, r;
      if (tone == TONE_ISO) {
        // Same tone in both ears, gated by the entrainment oscillator.
        float gate = 0.5f * (1.0f - cosf((float)entrainPhase_));
        gate *= gate;
        l = r = sinf(pL) * gate;
      } else if (tone == TONE_CHAKRA) {
        // Fundamental plus a soft octave: reads warmer, like a singing bowl.
        l = (sinf(pL) + 0.25f * sinf(2.0f * pL)) * 0.8f;
        r = (sinf(pR) + 0.25f * sinf(2.0f * pR)) * 0.8f;
      } else {
        l = sinf(pL);
        r = sinf(pR);
      }
      l *= toneVol; r *= toneVol;

      if (bed != BED_NONE) {
        float nl, nr;
        switch (bed) {
          case BED_PINK:  nl = noiseL.pink();   nr = noiseR.pink();   break;
          case BED_BROWN: nl = noiseL.brownN(); nr = noiseR.brownN(); break;
          case BED_RAIN:  nl = noiseL.rain();   nr = noiseR.rain();   break;
          default: {      // ocean: brown noise with a slow ~10 s swell
            float swell = 0.55f + 0.45f * sinf((float)oceanPhase);
            nl = noiseL.brownN() * swell; nr = noiseR.brownN() * swell;
          }
        }
        l += nl * bedVol; r += nr * bedVol;
      }

      l *= g; r *= g;
      block[n * 2 + 0] = (int16_t)clampf(l * 32000.0f, -32767.0f, 32767.0f);
      block[n * 2 + 1] = (int16_t)clampf(r * 32000.0f, -32767.0f, 32767.0f);

      phaseL_ += wL * dt; if (phaseL_ > kTwoPi) phaseL_ -= kTwoPi;
      phaseR_ += wR * dt; if (phaseR_ > kTwoPi) phaseR_ -= kTwoPi;
      entrainPhase_ += wE * dt; if (entrainPhase_ > kTwoPi) entrainPhase_ -= kTwoPi;
      oceanPhase += kTwoPi * 0.1 * dt; if (oceanPhase > kTwoPi) oceanPhase -= kTwoPi;
    }
    gain_ = gEnd;

    // LED duty from the SAME entrainment phase as the beat/gate -> locked.
    float pulse = 0.5f * (1.0f - cosf((float)entrainPhase_));   // 0..1
    float brightCap = std::min(clampf(session_.light.brightness, 0.0f, 1.0f), SAFE_BRIGHTNESS_MAX);
    float duty = pulse * pulse * brightCap * gEnd;
    uint32_t dutyVal = (uint32_t)(duty * ((1 << LED_PWM_BITS) - 1));
    if (session_.light.pattern == "alternate") {
      bool firstHalf = entrainPhase_ < M_PI;
      ledcWrite(PIN_LED_LEFT,  firstHalf ? dutyVal : 0);
      ledcWrite(PIN_LED_RIGHT, firstHalf ? 0 : dutyVal);
    } else {
      ledcWrite(PIN_LED_LEFT, dutyVal);
      ledcWrite(PIN_LED_RIGHT, dutyVal);
    }

    size_t wrote;
    i2s_write(I2S_PORT, block, sizeof(block), &wrote, portMAX_DELAY);
    elapsed_s_ = t + blockDt;

    bool done = elapsed_s_ >= total_s_;
    bool overCap = elapsed_s_ >= SAFE_SESSION_MAX_MIN * 60.0f;
    bool stopped = state_ == EngineState::Stopping && gEnd <= 0.0f;
    if (done || overCap || stopped) {
      ledcWrite(PIN_LED_LEFT, 0);
      ledcWrite(PIN_LED_RIGHT, 0);
      i2s_zero_dma_buffer(I2S_PORT);
      state_ = EngineState::Idle;
    }
  }
}
