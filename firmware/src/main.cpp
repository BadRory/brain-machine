// main.cpp — Brain Machine (Stage 1), XIAO ESP32-S3.
// Fork of Mitch Altman's Brain Machine / Sound & Light Machine (CC BY-SA).
//
// Boot order: storage -> network (join or hotspot) -> web app -> engine.
// Control is the WiFi web app; the one hardware control is the stop button,
// usable blind with eyes shut mid-session.
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "store.h"
#include "net.h"
#include "webui.h"
#include "engine.h"

namespace {
  // Debounced active-low stop button.
  bool lastBtn = true;
  uint32_t lastEdge = 0;

  void pollStopButton() {
    bool now = digitalRead(PIN_STOP_BTN);     // HIGH = released (pull-up)
    uint32_t t = millis();
    if (now != lastBtn && t - lastEdge > 40) {
      lastEdge = t;
      lastBtn = now;
      if (now == LOW) {                        // pressed
        if (Engine.status().state == EngineState::Running) {
          Serial.println("[btn] stop pressed");
          Engine.stop();
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] Brain Machine (Stage 1)");

  pinMode(PIN_STOP_BTN, INPUT_PULLUP);

  Store::begin();   // LittleFS: sessions + history + wifi creds
  Net::begin();     // try saved home WiFi, else hotspot fallback
  Engine.begin();   // I2S + LEDC + audio task (idle until start())
  WebUI::begin();   // control API + static UI + captive portal

  Serial.printf("[boot] ready. %s  http://%s\n",
                Net::mode() == NetMode::STA ? "joined home WiFi" : "hotspot",
                Net::ip().c_str());
}

void loop() {
  pollStopButton();
  WebUI::loop();
  delay(2);
}
