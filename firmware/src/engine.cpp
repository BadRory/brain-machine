// engine.cpp — phase-locked light + binaural audio.
//
// Design: the audio task is the clock. Every I2S block it advances elapsed
// time by (AUDIO_FRAMES / sample_rate). It computes current_hz from the
// session fade curve, generates a stereo block whose L/R carrier gap IS that
// hz (the binaural beat), and sets the LED duty from the SAME entrainment
// phase. So the LED flash and the audio beat share one phase accumulator —
// they are the same oscillator, read twice. Nothing can drift.
#include "engine.h"
#include "config.h"
#include <driver/i2s.h>
#include <math.h>
#include <algorithm>

EntrainmentEngine Engine;

static constexpr double TWO_PI = 6.283185307179586;
static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

// --- clamp helpers ---
static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void EntrainmentEngine::begin() {
  // LEDC for the two eye LEDs (high-freq PWM carrier, we modulate duty).
  ledcAttach(PIN_LED_LEFT,  LED_PWM_FREQ_HZ, LED_PWM_BITS);
  ledcAttach(PIN_LED_RIGHT, LED_PWM_FREQ_HZ, LED_PWM_BITS);
  ledcWrite(PIN_LED_LEFT, 0);
  ledcWrite(PIN_LED_RIGHT, 0);

  // I2S master TX to the PCM5102A.
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = (i2s_bits_per_sample_t)AUDIO_BITS;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = AUDIO_FRAMES;
  cfg.use_apll = true;              // low-jitter clock for clean beats
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCLK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);

  xTaskCreatePinnedToCore(audioTaskTrampoline, "entrain-audio", 4096, this,
                          configMAX_PRIORITIES - 2, &audioTaskH_, 1);
}

float EntrainmentEngine::freqAt(float t, size_t* stepOut) const {
  // Walk the step timeline; apply a linear fade of fade_s INTO each step
  // from the previous step's hz. Clamp to the safe flash band.
  if (session_.steps.empty()) { if (stepOut) *stepOut = 0; return 0; }
  float acc = 0;
  float prevHz = session_.steps.front().hz;
  for (size_t i = 0; i < session_.steps.size(); ++i) {
    const Step& st = session_.steps[i];
    float dur = st.minutes * 60.0f;
    if (t < acc + dur || i == session_.steps.size() - 1) {
      float into = t - acc;                 // seconds into this step
      float hz;
      if (into < st.fade_s && st.fade_s > 0) {
        float k = into / st.fade_s;          // 0..1
        hz = prevHz + (st.hz - prevHz) * k;
      } else {
        hz = st.hz;
      }
      if (stepOut) *stepOut = i;
      return clampf(hz, SAFE_FLASH_HZ_MIN, SAFE_FLASH_HZ_MAX);
    }
    acc += dur;
    prevHz = st.hz;
  }
  if (stepOut) *stepOut = session_.steps.size() - 1;
  return clampf(session_.steps.back().hz, SAFE_FLASH_HZ_MIN, SAFE_FLASH_HZ_MAX);
}

bool EntrainmentEngine::start(const Session& s) {
  if (s.steps.empty()) return false;
  session_ = s;
  elapsed_s_ = 0;
  phaseL_ = phaseR_ = entrainPhase_ = 0;
  state_ = EngineState::Running;
  return true;
}

void EntrainmentEngine::stop() {
  if (state_ == EngineState::Running) state_ = EngineState::Stopping;
}

EngineStatus EntrainmentEngine::status() const {
  EngineStatus st;
  st.state = state_;
  st.sessionId = session_.id;
  st.elapsed_s = elapsed_s_;
  st.total_s = session_.totalSeconds();
  size_t idx = 0;
  st.current_hz = freqAt(elapsed_s_, &idx);
  st.step_index = idx;
  return st;
}

void EntrainmentEngine::audioTaskTrampoline(void* self) {
  static_cast<EntrainmentEngine*>(self)->audioTask();
}

void EntrainmentEngine::audioTask() {
  static int16_t block[AUDIO_FRAMES * 2]; // interleaved L,R
  const double dt = 1.0 / AUDIO_SAMPLE_RATE;

  for (;;) {
    if (state_ == EngineState::Idle) {
      // Keep the DMA fed with silence so the DAC stays settled.
      memset(block, 0, sizeof(block));
      size_t wrote;
      i2s_write(I2S_PORT, block, sizeof(block), &wrote, portMAX_DELAY);
      vTaskDelay(1);
      continue;
    }

    float t = elapsed_s_;
    float hz = freqAt(t);                  // entrainment rate this block
    float carrier = clampf(session_.audio.carrier_hz, 50.0f, 1000.0f);
    double wL = TWO_PI * carrier;          // left carrier
    double wR = TWO_PI * (carrier + hz);   // right carrier -> beat = hz
    double wE = TWO_PI * hz;               // entrainment envelope + LED

    // Soft-start + stop ramp for volume/brightness.
    float env = 1.0f;
    if (t < SAFE_SOFTSTART_S) env = t / SAFE_SOFTSTART_S;
    if (state_ == EngineState::Stopping) env = 0.0f; // snap toward silence

    float vol = clampf(session_.audio.volume, 0.0f, 1.0f) * env;
    float brightCap = std::min(session_.light.brightness, SAFE_BRIGHTNESS_MAX);
    float bright = brightCap * env;

    for (int n = 0; n < AUDIO_FRAMES; ++n) {
      float l = (float)sin(phaseL_);
      float r = (float)sin(phaseR_);
      int16_t sl = (int16_t)(l * vol * 30000.0f);
      int16_t sr = (int16_t)(r * vol * 30000.0f);
      block[n * 2 + 0] = sl;
      block[n * 2 + 1] = sr;

      phaseL_ += wL * dt; if (phaseL_ > TWO_PI) phaseL_ -= TWO_PI;
      phaseR_ += wR * dt; if (phaseR_ > TWO_PI) phaseR_ -= TWO_PI;
      entrainPhase_ += wE * dt; if (entrainPhase_ > TWO_PI) entrainPhase_ -= TWO_PI;
    }

    // LED duty from the SAME entrainment phase the beat uses -> locked.
    // Raised-cosine pulse (0..1), gamma-ish via square for nicer perceived fade.
    float pulse = 0.5f * (1.0f - (float)cos(entrainPhase_)); // 0..1
    float duty = pulse * pulse * bright;
    uint32_t dutyVal = (uint32_t)(duty * ((1 << LED_PWM_BITS) - 1));

    if (session_.light.pattern == "alternate") {
      // Left on first half-cycle, right on second — same lock, split in phase.
      bool firstHalf = entrainPhase_ < M_PI;
      ledcWrite(PIN_LED_LEFT,  firstHalf ? dutyVal : 0);
      ledcWrite(PIN_LED_RIGHT, firstHalf ? 0 : dutyVal);
    } else {
      ledcWrite(PIN_LED_LEFT, dutyVal);
      ledcWrite(PIN_LED_RIGHT, dutyVal);
    }

    size_t wrote;
    i2s_write(I2S_PORT, block, sizeof(block), &wrote, portMAX_DELAY);

    // Advance the authoritative clock by exactly one block of audio time.
    elapsed_s_ = t + (float)AUDIO_FRAMES * dt;

    // End conditions: finished all steps, hit the hard cap, or stop requested.
    bool done = elapsed_s_ >= session_.totalSeconds();
    bool overCap = elapsed_s_ >= SAFE_SESSION_MAX_MIN * 60.0f;
    if (state_ == EngineState::Stopping || done || overCap) {
      ledcWrite(PIN_LED_LEFT, 0);
      ledcWrite(PIN_LED_RIGHT, 0);
      i2s_zero_dma_buffer(I2S_PORT);
      state_ = EngineState::Idle;
    }
  }
}
