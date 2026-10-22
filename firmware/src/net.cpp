// net.cpp — WiFi provisioning with hotspot fallback.
#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
  NetMode g_mode = NetMode::AP;

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
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid().c_str());
    g_mode = NetMode::AP;
    Serial.printf("[net] AP up: %s  %s\n", apSsid().c_str(),
                  WiFi.softAPIP().toString().c_str());
  }

  bool tryJoin(const String& ssid, const String& pass) {
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

  void startMDNS() {
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[net] mDNS: http://%s.local\n", MDNS_HOSTNAME);
    }
  }
}

void Net::begin() {
  WiFi.persistent(false);
  String ssid, pass;
  if (loadCreds(ssid, pass) && tryJoin(ssid, pass)) {
    startMDNS();
    return;
  }
  startAP();     // no creds, or join failed -> hotspot fallback
}

NetMode Net::mode() { return g_mode; }
String Net::ssid()  { return g_mode == NetMode::STA ? WiFi.SSID() : apSsid(); }
String Net::ip() {
  return (g_mode == NetMode::STA ? WiFi.localIP() : WiFi.softAPIP()).toString();
}

bool Net::setHomeWifi(const String& ssid, const String& pass) {
  saveCreds(ssid, pass);
  if (tryJoin(ssid, pass)) { startMDNS(); return true; }
  startAP();
  return false;
}

void Net::forgetHomeWifi() {
  if (LittleFS.exists(WIFI_CFG_FILE)) LittleFS.remove(WIFI_CFG_FILE);
  startAP();
}
