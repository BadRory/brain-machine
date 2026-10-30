// session.cpp — JSON (de)serialization + built-in presets.
#include "session.h"

bool Session::fromJson(const JsonObjectConst& o) {
  if (o.isNull()) return false;
  id   = o["id"]   | "";
  name = o["name"] | "";
  if (id.isEmpty()) return false;

  steps.clear();
  for (JsonObjectConst s : o["steps"].as<JsonArrayConst>()) {
    Step st;
    st.hz      = s["hz"]      | 10.0f;
    st.minutes = s["minutes"] | 2.0f;
    st.fade_s  = s["fade_s"]  | 30.0f;
    steps.push_back(st);
  }
  if (steps.empty()) return false;

  JsonObjectConst l = o["light"];
  if (!l.isNull()) {
    light.brightness = l["brightness"] | light.brightness;
    light.pattern    = l["pattern"]    | light.pattern;
  }
  JsonObjectConst a = o["audio"];
  if (!a.isNull()) {
    audio.carrier_hz = a["carrier_hz"] | audio.carrier_hz;
    audio.soundscape = a["soundscape"] | audio.soundscape;
    audio.volume     = a["volume"]     | audio.volume;
  }
  return true;
}

void Session::toJson(JsonObject o) const {
  o["id"]   = id;
  o["name"] = name;
  JsonArray arr = o["steps"].to<JsonArray>();
  for (const auto& st : steps) {
    JsonObject s = arr.add<JsonObject>();
    s["hz"]      = st.hz;
    s["minutes"] = st.minutes;
    s["fade_s"]  = st.fade_s;
  }
  JsonObject l = o["light"].to<JsonObject>();
  l["brightness"] = light.brightness;
  l["pattern"]    = light.pattern;
  JsonObject a = o["audio"].to<JsonObject>();
  a["carrier_hz"] = audio.carrier_hz;
  a["soundscape"] = audio.soundscape;
  a["volume"]     = audio.volume;
}

static Session makeClassic() {
  // Mitch Altman's original 14-min arc: beta -> alpha -> theta -> back up.
  // Credit: Mitch Altman, Brain Machine / Sound & Light Machine (CC BY-SA).
  Session s;
  s.id = "classic-14";
  s.name = "Classic (Mitch Altman's original)";
  s.steps = {
    { 20.0f, 1.0f, 10.0f }, // beta, settle in
    { 15.0f, 2.0f, 30.0f },
    { 10.0f, 3.0f, 60.0f }, // alpha, relaxed
    {  5.0f, 4.0f, 60.0f }, // theta, meditative (the heart of it)
    { 10.0f, 2.0f, 60.0f }, // back up
    { 15.0f, 1.0f, 30.0f },
    { 20.0f, 1.0f, 30.0f }, // gently return to alert
  };
  s.audio.carrier_hz = 200.0f;
  s.audio.soundscape = "none";
  return s;
}

static Session makeWinddown() {
  Session s;
  s.id = "evening-winddown";
  s.name = "Evening wind-down";
  s.steps = {
    { 15.0f, 2.0f, 30.0f },
    { 10.0f, 4.0f, 60.0f },
    {  6.0f, 6.0f, 60.0f },
    {  4.0f, 4.0f, 60.0f }, // delta edge, toward sleep
  };
  s.audio.carrier_hz = 200.0f;
  s.audio.soundscape = "soft-rain";
  s.light.brightness = 0.4f;
  return s;
}

static Session makeFocus() {
  Session s;
  s.id = "morning-focus";
  s.name = "Morning focus";
  s.steps = {
    { 10.0f, 2.0f, 20.0f },
    { 14.0f, 4.0f, 45.0f },
    { 12.0f, 4.0f, 45.0f }, // hold low-beta/alpha for calm alertness
  };
  s.audio.carrier_hz = 220.0f;
  s.audio.soundscape = "pink-noise";
  return s;
}

const std::vector<Session>& builtinSessions() {
  static std::vector<Session> presets = { makeClassic(), makeWinddown(), makeFocus() };
  return presets;
}
