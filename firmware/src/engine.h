// engine.h — the entrainment core. Light and binaural audio are driven from
// ONE timebase so they can never drift out of phase (the whole machine
// depends on this lock; see hardware/STAGE1_HARDWARE.md §5 and the BT-latency
// discussion — wired audio + shared clock is the entire point).
#pragma once
#include <Arduino.h>
#include "session.h"

enum class EngineState { Idle, Running, Stopping };

struct EngineStatus {
  EngineState state = EngineState::Idle;
  String sessionId;
  float elapsed_s   = 0;
  float total_s     = 0;
  float current_hz  = 0;   // live entrainment freq (post-fade)
  size_t step_index = 0;
};

class EntrainmentEngine {
 public:
  void begin();                      // init I2S, LEDC, spawn audio task
  bool start(const Session& s);      // begin a session (copies it)
  void stop();                       // graceful stop (soft fade-out)
  EngineStatus status() const;

  // The single source of truth for "what hz right now", applying the fade
  // between the previous step and the current step. Also enforces the
  // photosensitive dwell limiter. Pure function of elapsed time.
  float freqAt(float elapsed_s, size_t* stepOut = nullptr) const;

 private:
  void audioTask();                  // runs on its own FreeRTOS task
  static void audioTaskTrampoline(void* self);

  Session   session_;
  volatile EngineState state_ = EngineState::Idle;
  volatile float elapsed_s_   = 0;   // advanced by the audio task (authoritative)

  // Shared phase accumulator: one per L/R carrier for binaural beat, plus the
  // entrainment-rate phase that BOTH the beat envelope and the LED read.
  double phaseL_ = 0, phaseR_ = 0;   // carrier phases (radians)
  double entrainPhase_ = 0;          // 0..2pi at the entrainment rate

  TaskHandle_t audioTaskH_ = nullptr;
};

extern EntrainmentEngine Engine;
