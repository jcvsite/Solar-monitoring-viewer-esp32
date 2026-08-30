#include "device_web.h"
#include <WebServer.h>
#include <WiFi.h>

DeviceWeb deviceWeb;
static WebServer s_server(80);

void DeviceWeb::applySettings(const HostSettings& s) { settings_ = s; }

String DeviceWeb::htmlPage() const {
  String ip = WiFi.localIP().toString();
  String host = settings_.hostIp.length() ? settings_.hostIp : "(not set)";
  String html = R"raw(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Display</title>
<style>body{font-family:sans-serif;margin:1rem;background:#111;color:#eee}
label{display:block;margin:.5rem 0}input,select{width:100%;padding:.4rem}
button{margin-top:1rem;padding:.6rem 1.2rem;background:#0a84ff;color:#fff;border:0;border-radius:6px}
.card{background:#1c1c1e;padding:1rem;border-radius:8px;margin-bottom:1rem}</style></head><body>
<h1>Solar Monitoring Viewer</h1>
<div class="card"><p>Device IP: )raw";
  html += ip;
  html += R"raw(</p><p>Host: )raw";
  html += host;
  html += ":" + String(settings_.hostPort);
  html += R"raw(</p></div>
<form method="POST" action="/save">
<div class="card">
<label>Host IP<input name="host" value=")raw";
  html += settings_.hostIp;
  html += R"raw("></label>
<label>Port<input name="port" type="number" value=")raw";
  html += String(settings_.hostPort);
  html += R"raw("></label>
<label>Layout<select name="layout">)raw";
  const char* layouts[] = {"Classic", "Compact", "Ring", "Bars", "Flow"};
  for (int i = 0; i < 5; i++) {
    html += "<option value='" + String(i) + "'";
    if ((int)settings_.glanceLayout == i) html += " selected";
    html += ">" + String(layouts[i]) + "</option>";
  }
  html += R"raw(</select></label>
<label>Theme<select name="theme">)raw";
  const char* themes[] = {"Dark", "Light", "Solar", "Ocean", "Forest"};
  for (int i = 0; i < 5; i++) {
    html += "<option value='" + String(i) + "'";
    if ((int)settings_.themeId == i) html += " selected";
    html += ">" + String(themes[i]) + "</option>";
  }
  html += R"raw(</select></label>
<label>Rotation<select name="rot">)raw";
  const char* rots[] = {"Portrait", "Landscape", "Portrait flip", "Landscape flip"};
  for (int i = 0; i < 4; i++) {
    html += "<option value='" + String(i) + "'";
    if ((int)settings_.screenRotation == i) html += " selected";
    html += ">" + String(rots[i]) + "</option>";
  }
  html += R"raw(</select></label>
<label><input type="checkbox" name="gridAlert" )raw";
  if (settings_.gridOfflineAlert) html += "checked";
  html += R"raw(> Grid offline alert</label>
<label><input type="checkbox" name="useHost" )raw";
  if (settings_.useHostConfig) html += "checked";
  html += R"raw(> Sync from host</label>
</div>
<button type="submit">Save</button>
</form></body></html>)raw";
  return html;
}

void DeviceWeb::handleRoot() {
  s_server.send(200, "text/html", htmlPage());
}

void DeviceWeb::handleSave() {
  if (s_server.hasArg("host")) settings_.hostIp = s_server.arg("host");
  if (s_server.hasArg("port")) settings_.hostPort = (uint16_t)constrain(s_server.arg("port").toInt(), 1, 65535);
  if (s_server.hasArg("layout")) settings_.glanceLayout = (uint8_t)constrain(s_server.arg("layout").toInt(), 0, 4);
  if (s_server.hasArg("theme")) settings_.themeId = (uint8_t)constrain(s_server.arg("theme").toInt(), 0, 4);
  if (s_server.hasArg("rot")) settings_.screenRotation = (uint8_t)constrain(s_server.arg("rot").toInt(), 0, 3);
  settings_.gridOfflineAlert = s_server.hasArg("gridAlert");
  settings_.useHostConfig = s_server.hasArg("useHost");
  s_server.send(200, "text/html", "<html><body><p>Saved.</p><a href='/'>Back</a></body></html>");
  // Caller should persist via store.save in main if needed — flag via extern
  extern bool gDeviceWebSaved;
  gDeviceWebSaved = true;
}

void DeviceWeb::begin() {
  if (started_) return;
  s_server.on("/", [this]() { handleRoot(); });
  s_server.on("/save", HTTP_POST, [this]() { handleSave(); });
  s_server.begin();
  started_ = true;
}

void DeviceWeb::loop() {
  if (started_) s_server.handleClient();
}

bool gDeviceWebSaved = false;
