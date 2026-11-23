// net.h — WiFi: try saved home creds, else fall back to self-hosted hotspot.
// Standard IoT captive-provisioning pattern (see CONTROL_WEBAPP.md §2).
//
// Joining is non-blocking: the web UI posts creds, Net::loop() does the join
// while the hotspot stays up, and the UI polls joinStatus() for the result.
#pragma once
#include <Arduino.h>

enum class NetMode { AP, STA };
enum class JoinState { Idle, Joining, Ok, Failed };

namespace Net {
  // Reads saved creds from LittleFS; tries to join; falls back to AP.
  void begin();
  void loop();                       // call from loop(); drives join + AP shutdown
  NetMode mode();
  String ssid();      // the SSID we are serving as (AP) or joined to (STA)
  String ip();        // current IP as string

  // Queue a join to a home network. Returns immediately.
  void requestJoin(const String& ssid, const String& pass);
  JoinState joinState();
  String joinIp();                   // IP on the home network once Ok
  String joinSsid();
  void forgetHomeWifi();

  // Network scan for the setup page. The scan runs on the main loop (not the
  // web task). Result JSON: {"networks":[{"ssid":"..","rssi":-50,"secure":true}]}
  void scanStart();                  // ask the main loop to run a fresh scan
  String scanJson();                 // {"scanning":true} until the scan is done
}
