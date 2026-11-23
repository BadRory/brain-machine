// heart.cpp — PPG beat detection using the SparkFun MAX3010x library.
#include "heart.h"
#include "config.h"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

namespace {
  MAX30105 sensor;
  volatile bool g_present = false;
  volatile bool g_contact = false;
  volatile uint8_t g_bpm = 0;
  volatile uint32_t g_ir = 0;

  constexpr int kAvg = 4;          // beats averaged for the shown BPM
  uint8_t rates[kAvg] = {0};
  int rateIdx = 0, rateCount = 0;
  uint32_t lastBeat = 0;

  std::vector<uint8_t> samples;
  uint32_t lastLog = 0;

  void resetBeats() {
    rateIdx = rateCount = 0;
    g_bpm = 0;
  }

  void task(void*) {
    uint32_t lastContact = 0;
    for (;;) {
      uint32_t irv = sensor.getIR();             // waits for the next sample
      g_ir = irv;
      uint32_t now = millis();

      if (irv >= HR_CONTACT_IR) lastContact = now;
      bool contact = now - lastContact < 1000;
      if (contact != g_contact) {
        g_contact = contact;
        Serial.printf("[heart] %s (ir=%u)\n", contact ? "skin contact" : "no contact", irv);
        if (!contact) resetBeats();
      }

      if (contact && checkForBeat(irv)) {
        uint32_t delta = now - lastBeat;
        lastBeat = now;
        float rate = 60000.0f / delta;
        if (rate >= HR_BPM_MIN && rate <= HR_BPM_MAX) {
          rates[rateIdx] = (uint8_t)rate;
          rateIdx = (rateIdx + 1) % kAvg;
          if (rateCount < kAvg) rateCount++;
          uint16_t sum = 0;
          for (int i = 0; i < rateCount; ++i) sum += rates[i];
          g_bpm = sum / rateCount;
        }
      }
      if (g_bpm && now - lastBeat > 3000) resetBeats();   // lost the pulse
      vTaskDelay(1);
    }
  }
}

void Heart::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("[heart] no sensor found on I2C (check SDA/SCL/3V3)");
    return;
  }
  g_present = true;
  Serial.printf("[heart] sensor found, part id 0x%02X rev %u\n",
                sensor.readPartID(), sensor.getRevisionID());
  // brightness, sampleAverage, ledMode (2 = red+IR), sampleRate, pulseWidth, adcRange
  sensor.setup(HR_LED_POWER, 4, 2, 400, 411, 4096);
  sensor.setPulseAmplitudeRed(0x0A);   // red only as a dim "on" hint; IR does the work
  xTaskCreatePinnedToCore(task, "heart", 4096, nullptr, 1, nullptr, 0);
}

bool Heart::present()  { return g_present; }
bool Heart::contact()  { return g_contact; }
uint8_t Heart::bpm()   { return g_bpm; }
uint32_t Heart::ir()   { return g_ir; }

void Heart::sessionStart() { samples.clear(); lastLog = millis(); }

void Heart::sessionTick() {
  if (millis() - lastLog < HR_LOG_EVERY_MS) return;
  lastLog = millis();
  uint8_t b = g_bpm;
  if (b && samples.size() < 720) samples.push_back(b);   // cap: 1 h at 5 s
}

std::vector<uint8_t> Heart::sessionSamples() { return samples; }
