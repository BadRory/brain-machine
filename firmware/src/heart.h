// heart.h — forehead PPG heart rate (MAX30101 / MAX30102 on I2C).
//
// Runs on its own low-priority task on core 0, so I2C reads can never stall
// the audio task (core 1) that keeps light and sound locked.
#pragma once
#include <Arduino.h>
#include <vector>

namespace Heart {
  void begin();          // start I2C + sensor task; safe if no sensor is fitted
  bool present();        // sensor answered on I2C
  bool contact();        // enough reflected light: sensor is against skin
  uint8_t bpm();         // smoothed beats per minute, 0 = no reading
  uint32_t ir();         // raw IR level, for checking placement

  // Session logging: call sessionStart() when a run begins, sessionTick()
  // from loop() while it runs (stores one sample every HR_LOG_EVERY_MS).
  void sessionStart();
  void sessionTick();
  std::vector<uint8_t> sessionSamples();
}
