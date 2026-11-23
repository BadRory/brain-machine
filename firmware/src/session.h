// session.h — the session/sequence data model.
// Mirrors firmware/CONTROL_WEBAPP.md §5. A session is an ordered list of
// steps; each step holds an entrainment target (hz) and a dwell + fade.
#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

struct Step {
  float hz       = 10.0f; // entrainment target: drives BOTH LED flash + beat
  float minutes  = 2.0f;  // dwell at this hz
  float fade_s   = 30.0f; // fade FROM previous step's hz INTO this one
  float carrier_hz = 0.0f; // 0 = use the session carrier; else glide to this tone
};

struct LightCfg {
  float brightness = 0.5f;        // 0..1 (hard-capped by SAFE_BRIGHTNESS_MAX)
  String pattern   = "both";      // both | alternate
};

struct AudioCfg {
  float carrier_hz = 200.0f;      // base tone; beat = carrier .. carrier+hz
  String tone       = "binaural"; // binaural | isochronic | chakra
  String soundscape = "none";     // none | rain | ocean | pink | brown
  float volume      = 0.6f;       // 0..1 (tone)
  float noise_vol   = 0.35f;      // 0..1 (soundscape bed)
};

struct Session {
  String id;
  String name;
  String category = "custom";     // classic | chakra | relax | sleep | focus | custom
  String blurb;                   // one line shown in the UI
  std::vector<Step> steps;
  LightCfg light;
  AudioCfg audio;

  float totalSeconds() const {
    float s = 0;
    for (const auto& st : steps) s += st.minutes * 60.0f;
    return s;
  }

  // ---- JSON (de)serialization for the web API + LittleFS ----
  bool fromJson(const JsonObjectConst& o);
  void toJson(JsonObject o) const;
};

// Built-in presets, incl. the classic original (beta -> theta -> back).
// Defined in session.cpp.
const std::vector<Session>& builtinSessions();
