// net.cpp — WiFi provisioning with hotspot fallback.
#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
  NetMode g_mode = NetMode::AP;
  JoinState g_join = JoinState::Idle;
  String g_joinSsid, g_joinIp;

  // Pending join request (written by the web task, consumed by Net::loop()).
  volatile bool g_pending = false;
  String g_pSsid, g_pPass;
  uint32_t g_joinStart = 0;
  uint32_t g_okAt = 0;
  bool g_mdnsUp = false;

  String apSsid() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
    return String(buf);
  }

  bool loadCreds(String& ssid, String& pass) {
    if (!LittleFS.exists(WIFI_CFG_FILE)) return false;
    File f = LittleFS.open(WIFI_CFG_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    ssid = doc["ssid"] | "";
    pass = doc["pass"] | "";
    return !ssid.isEmpty();
  }

  void saveCreds(const String& ssid, const String& pass) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    File f = LittleFS.open(WIFI_CFG_FILE, "w");
    if (f) { serializeJson(doc, f); f.close(); }
  }

  void startAP() {
    // AP+STA so the setup page can scan for networks while the hotspot is up.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid().c_str(), AP_PASSWORD);
    g_mode = NetMode::AP;
    Serial.printf("[net] AP up: %s  pass: %s  %s\n", apSsid().c_str(), AP_PASSWORD,
                  WiFi.softAPIP().toString().c_str());
  }

  void startMDNS() {
    if (g_mdnsUp) MDNS.end();
    g_mdnsUp = MDNS.begin(MDNS_HOSTNAME);
    if (g_mdnsUp) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[net] mDNS: http://%s.local\n", MDNS_HOSTNAME);
    }
  }

  // Boot-time join: blocking is fine here, nothing else is running yet.
  bool bootJoin(const String& ssid, const String& pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (millis() - start < WIFI_JOIN_TIMEOUT_MS) {
      if (WiFi.status() == WL_CONNECTED) {
        g_mode = NetMode::STA;
        Serial.printf("[net] joined %s  %s\n", ssid.c_str(),
                      WiFi.localIP().toString().c_str());
        return true;
      }
      delay(200);
    }
    Serial.printf("[net] join failed for %s\n", ssid.c_str());
    return false;
  }
}

void Net::begin() {
  WiFi.persistent(false);
  String ssid, pass;
  if (loadCreds(ssid, pass) && bootJoin(ssid, pass)) {
    startMDNS();
    return;
  }
  startAP();     // no creds, or join failed -> hotspot fallback
}

void Net::loop() {
  // 1) start a queued join; hotspot stays up so the UI can read the result.
  if (g_pending) {
    String ssid = g_pSsid, pass = g_pPass;
    g_pending = false;
    WiFi.mode(WIFI_AP_STA);
    if (g_mode == NetMode::STA) WiFi.softAP(apSsid().c_str(), AP_PASSWORD);
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), pass.c_str());
    g_joinSsid = ssid; g_joinIp = "";
    g_join = JoinState::Joining;
    g_joinStart = millis();
    return;
  }

  // 2) watch the join.
  if (g_join == JoinState::Joining) {
    if (WiFi.status() == WL_CONNECTED) {
      saveCreds(g_joinSsid, g_pPass);
      g_joinIp = WiFi.localIP().toString();
      g_join = JoinState::Ok;
      g_okAt = millis();
      startMDNS();
      Serial.printf("[net] joined %s  %s\n", g_joinSsid.c_str(), g_joinIp.c_str());
    } else if (millis() - g_joinStart > WIFI_SETUP_JOIN_TIMEOUT_MS) {
      WiFi.disconnect();
      g_join = JoinState::Failed;
      Serial.printf("[net] join failed for %s\n", g_joinSsid.c_str());
    }
    return;
  }

  // 3) joined: keep the hotspot a little longer so the phone can read the
  //    new address, then drop it and run on home WiFi only.
  if (g_join == JoinState::Ok && g_mode == NetMode::AP &&
      millis() - g_okAt > AP_LINGER_MS) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    g_mode = NetMode::STA;
    Serial.println("[net] hotspot closed; running on home WiFi");
  }
}

NetMode Net::mode() { return g_mode; }
String Net::ssid()  { return g_mode == NetMode::STA ? WiFi.SSID() : apSsid(); }
String Net::ip() {
  return (g_mode == NetMode::STA ? WiFi.localIP() : WiFi.softAPIP()).toString();
}

void Net::requestJoin(const String& ssid, const String& pass) {
  g_pSsid = ssid; g_pPass = pass;
  g_join = JoinState::Joining;      // report "joining" right away
  g_joinSsid = ssid; g_joinIp = "";
  g_pending = true;
}
JoinState Net::joinState() { return g_join; }
String Net::joinIp()   { return g_joinIp; }
String Net::joinSsid() { return g_joinSsid; }

void Net::forgetHomeWifi() {
  if (LittleFS.exists(WIFI_CFG_FILE)) LittleFS.remove(WIFI_CFG_FILE);
  g_join = JoinState::Idle;
  WiFi.disconnect();
  startAP();
}

String Net::scanJson() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {            // none started yet
    WiFi.scanNetworks(true);              // async
    return "{\"scanning\":true}";
  }
  if (n == WIFI_SCAN_RUNNING) return "{\"scanning\":true}";

  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    String s = WiFi.SSID(i);
    if (s.isEmpty()) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = s;
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  return out;
}
