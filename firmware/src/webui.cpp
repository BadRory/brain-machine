// webui.cpp — AsyncWebServer control API + static UI + captive portal + WS.
// Endpoints mirror CONTROL_WEBAPP.md §4.
#include "webui.h"
#include "config.h"
#include "net.h"
#include "engine.h"
#include "store.h"
#include "session.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
  AsyncWebServer server(80);
  AsyncWebSocket ws("/ws");
  DNSServer dns;
  uint32_t lastPush = 0;

  void sendJson(AsyncWebServerRequest* req, int code, JsonDocument& doc) {
    String out; serializeJson(doc, out);
    req->send(code, "application/json", out);
  }

  // GET /api/sessions -> built-ins + user sessions (metadata).
  void handleSessionsList(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    auto emit = [&](const Session& s, bool builtin) {
      JsonObject o = arr.add<JsonObject>();
      o["id"] = s.id; o["name"] = s.name;
      o["builtin"] = builtin; o["total_s"] = s.totalSeconds();
    };
    for (const auto& s : builtinSessions()) emit(s, true);
    for (const auto& s : Store::userSessions()) emit(s, false);
    sendJson(req, 200, doc);
  }

  // GET /api/status -> live engine + net state.
  void handleStatus(AsyncWebServerRequest* req) {
    EngineStatus st = Engine.status();
    JsonDocument doc;
    doc["state"] = st.state == EngineState::Running ? "running"
                 : st.state == EngineState::Stopping ? "stopping" : "idle";
    doc["session"] = st.sessionId;
    doc["elapsed_s"] = st.elapsed_s;
    doc["total_s"] = st.total_s;
    doc["current_hz"] = st.current_hz;
    doc["step"] = (uint32_t)st.step_index;
    doc["net_mode"] = Net::mode() == NetMode::STA ? "sta" : "ap";
    doc["ssid"] = Net::ssid();
    doc["ip"] = Net::ip();
    sendJson(req, 200, doc);
  }

  void handleRun(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument body;
    if (deserializeJson(body, data, len)) { req->send(400, "text/plain", "bad json"); return; }
    String id = body["id"] | "";
    Session s;
    if (!Store::find(id, s)) { req->send(404, "text/plain", "no such session"); return; }
    bool ok = Engine.start(s);
    req->send(ok ? 200 : 409, "text/plain", ok ? "started" : "busy");
  }

  void handleSave(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument body;
    if (deserializeJson(body, data, len)) { req->send(400, "text/plain", "bad json"); return; }
    Session s;
    if (!s.fromJson(body.as<JsonObjectConst>())) { req->send(400, "text/plain", "invalid session"); return; }
    req->send(Store::saveSession(s) ? 200 : 500, "text/plain", "saved");
  }

  void handleWifiGet(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["mode"] = Net::mode() == NetMode::STA ? "sta" : "ap";
    doc["ssid"] = Net::ssid();
    doc["ip"] = Net::ip();
    sendJson(req, 200, doc);
  }

  void handleWifiSet(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument body;
    if (deserializeJson(body, data, len)) { req->send(400, "text/plain", "bad json"); return; }
    String ssid = body["ssid"] | "";
    String pass = body["pass"] | "";
    if (ssid.isEmpty()) { req->send(400, "text/plain", "ssid required"); return; }
    // Respond BEFORE reconnecting (the join drops this AP client).
    req->send(200, "text/plain", "saved; joining — reconnect to the device");
    Net::setHomeWifi(ssid, pass);
  }

  void pushStatus() {
    if (ws.count() == 0) return;
    EngineStatus st = Engine.status();
    JsonDocument doc;
    doc["state"] = st.state == EngineState::Running ? "running"
                 : st.state == EngineState::Stopping ? "stopping" : "idle";
    doc["elapsed_s"] = st.elapsed_s;
    doc["total_s"] = st.total_s;
    doc["current_hz"] = st.current_hz;
    doc["step"] = (uint32_t)st.step_index;
    String out; serializeJson(doc, out);
    ws.textAll(out);
  }
}

void WebUI::begin() {
  // Static single-page UI from LittleFS (/ui/index.html). Captive-portal
  // fallback serves a tiny inline page if the asset isn't uploaded yet.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (LittleFS.exists("/ui/index.html"))
      req->send(LittleFS, "/ui/index.html", "text/html");
    else
      req->send(200, "text/html",
        "<h1>Brain Machine</h1><p>Control UI asset not uploaded yet. "
        "API is live at <code>/api/status</code>.</p>"
        "<p>Fork of Mitch Altman's Brain Machine (CC BY-SA).</p>");
  });
  server.serveStatic("/ui", LittleFS, "/ui");

  server.on("/api/sessions", HTTP_GET, handleSessionsList);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", Store::historyJson());
  });
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest* req) {
    Engine.stop(); req->send(200, "text/plain", "stopping");
  });
  server.on("/api/wifi", HTTP_GET, handleWifiGet);

  // Body-bearing POSTs.
  server.on("/api/run", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr, handleRun);
  server.on("/api/sessions", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr, handleSave);
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr, handleWifiSet);

  // Captive-portal: unknown host -> our page (only meaningful in AP mode).
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->redirect("/");
  });

  ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t) {});
  server.addHandler(&ws);

  if (Net::mode() == NetMode::AP) dns.start(53, "*", WiFi.softAPIP());
  server.begin();
}

void WebUI::loop() {
  if (Net::mode() == NetMode::AP) dns.processNextRequest();
  ws.cleanupClients();
  uint32_t now = millis();
  if (now - lastPush > 500) { lastPush = now; pushStatus(); }
}
