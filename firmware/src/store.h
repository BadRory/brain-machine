// store.h — LittleFS persistence for user sessions and session history.
#pragma once
#include <Arduino.h>
#include <vector>
#include "session.h"

namespace Store {
  void begin();

  // User-built sessions (built-ins live in code; these are saved by the UI).
  std::vector<Session> userSessions();
  bool saveSession(const Session& s);
  bool deleteSession(const String& id);

  // Find a session by id across built-ins + user sessions.
  bool find(const String& id, Session& out);

  // Append a finished session's summary to history.
  // hrSamples: optional BPM samples captured during the run.
  void appendHistory(const String& sessionId, float durationRun_s,
                     const std::vector<uint8_t>& bpmSamples);
  String historyJson();   // raw JSON array for the /api/history endpoint
}
