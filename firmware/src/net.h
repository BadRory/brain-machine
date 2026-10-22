// net.h — WiFi: try saved home creds, else fall back to self-hosted hotspot.
// Standard IoT captive-provisioning pattern (see CONTROL_WEBAPP.md §2).
#pragma once
#include <Arduino.h>

enum class NetMode { AP, STA };

namespace Net {
  // Reads saved creds from LittleFS; tries to join; falls back to AP.
  void begin();
  NetMode mode();
  String ssid();      // the SSID we are serving as (AP) or joined to (STA)
  String ip();        // current IP as string
  // Save new home-WiFi creds and reconnect. Returns true if it then joined.
  bool setHomeWifi(const String& ssid, const String& pass);
  void forgetHomeWifi();
}
