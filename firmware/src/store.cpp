// store.cpp — LittleFS persistence.
#include "store.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
  String sessionPath(const String& id) {
    return String(SESSIONS_DIR) + "/" + id + ".json";
  }
}

void Store::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[store] LittleFS mount failed");
    return;
  }
  if (!LittleFS.exists(SESSIONS_DIR)) LittleFS.mkdir(SESSIONS_DIR);
}

std::vector<Session> Store::userSessions() {
  std::vector<Session> out;
  File dir = LittleFS.open(SESSIONS_DIR);
  if (!dir || !dir.isDirectory()) return out;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    JsonDocument doc;
    if (!deserializeJson(doc, f)) {
      Session s;
      if (s.fromJson(doc.as<JsonObjectConst>())) out.push_back(s);
    }
    f.close();
  }
  return out;
}

bool Store::saveSession(const Session& s) {
  if (s.id.isEmpty()) return false;
  JsonDocument doc;
  s.toJson(doc.to<JsonObject>());
  File f = LittleFS.open(sessionPath(s.id), "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool Store::deleteSession(const String& id) {
  String p = sessionPath(id);
  return LittleFS.exists(p) ? LittleFS.remove(p) : false;
}

bool Store::find(const String& id, Session& out) {
  for (const auto& s : builtinSessions())
    if (s.id == id) { out = s; return true; }
  String p = sessionPath(id);
  if (!LittleFS.exists(p)) return false;
  File f = LittleFS.open(p, "r");
  if (!f) return false;
  JsonDocument doc;
  bool ok = !deserializeJson(doc, f) && out.fromJson(doc.as<JsonObjectConst>());
  f.close();
  return ok;
}

void Store::appendHistory(const String& sessionId, float durationRun_s,
                          const std::vector<uint8_t>& bpm) {
  JsonDocument doc;
  if (LittleFS.exists(HISTORY_FILE)) {
    File f = LittleFS.open(HISTORY_FILE, "r");
    deserializeJson(doc, f);
    f.close();
  }
  JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();

  JsonObject e = arr.add<JsonObject>();
  e["ts"] = (uint32_t)(millis() / 1000); // relative; RTC/NTP can refine later
  e["session"] = sessionId;
  e["duration_s"] = durationRun_s;
  uint16_t sum = 0, mn = 255, mx = 0;
  for (uint8_t b : bpm) { sum += b; mn = min<uint8_t>(mn, b); mx = max<uint8_t>(mx, b); }
  if (!bpm.empty()) {
    e["bpm_avg"] = sum / bpm.size();
    e["bpm_min"] = mn;
    e["bpm_max"] = mx;
  }
  JsonArray samples = e["bpm"].to<JsonArray>();
  for (uint8_t b : bpm) samples.add(b);

  File f = LittleFS.open(HISTORY_FILE, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}

String Store::historyJson() {
  if (!LittleFS.exists(HISTORY_FILE)) return "[]";
  File f = LittleFS.open(HISTORY_FILE, "r");
  String s = f.readString();
  f.close();
  return s.isEmpty() ? "[]" : s;
}
