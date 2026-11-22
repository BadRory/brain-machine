// engine.h — the entrainment core. Light and audio are driven from ONE
// timebase so they can never drift out of phase (the whole machine depends on
// this lock; see hardware/STAGE1_HARDWARE.md §5 — wired audio + shared clock is
// the entire point).
//
// Threading: everything the audio task touches (session_, phases, noise) is
// owned by that task. Other tasks only call start()/stop()/status().
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
  float carrier_hz  = 0;   // live tone
  size_t step_index = 0;
};

class EntrainmentEngine {
 public:
  void begin();                      // init I2S, LEDC, spawn audio task
  bool start(const Session& s);      // begin a session (false if one is running)
  void stop();                       // graceful stop (soft fade-out)
  EngineStatus status() const;

 private:
  void audioTask();                  // runs on its own FreeRTOS task
  static void audioTaskTrampoline(void* self);
  void adoptPending();
  // Pure function of elapsed time: entrainment hz, step index, carrier target.
  void evalAt(float t, float& hz, size_t& idx, float& carrier) const;

  // ---- owned by the audio task ----
  Session session_;
  double phaseL_ = 0, phaseR_ = 0, entrainPhase_ = 0;
  float  carrierCur_ = 200.0f;
  float  gain_ = 0.0f;
  float  stopT_ = 0.0f;

  // ---- hand-off + status (cross-task) ----
  Session pending_;
  SemaphoreHandle_t mux_ = nullptr;
  volatile bool hasPending_ = false;
  volatile EngineState state_ = EngineState::Idle;
  volatile float elapsed_s_ = 0, total_s_ = 0, curHz_ = 0, curCarrier_ = 0;
  volatile uint32_t curStep_ = 0;
  char id_[48] = {0};

  TaskHandle_t audioTaskH_ = nullptr;
};

extern EntrainmentEngine Engine;
