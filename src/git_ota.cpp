#include "git_ota.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static const char* kGithubRepo = "jcvsite/Solar-monitoring-viewer-esp32";
static const char* kAssetPrefix = "solar-viewer-cyd_esp32";

GitOta gitOta;

static SemaphoreHandle_t s_otaMutex = nullptr;
static TaskHandle_t s_otaTask = nullptr;

static void parseSemver(const String& s, int& maj, int& mino, int& pat) {
  String t = s;
  t.trim();
  if (t.startsWith("v") || t.startsWith("V")) t.remove(0, 1);
  maj = mino = pat = 0;
  int d1 = t.indexOf('.');
  if (d1 < 0) {
    maj = t.toInt();
    return;
  }
  maj = t.substring(0, d1).toInt();
  int d2 = t.indexOf('.', d1 + 1);
  if (d2 < 0) {
    mino = t.substring(d1 + 1).toInt();
    return;
  }
  mino = t.substring(d1 + 1, d2).toInt();
  String patch = t.substring(d2 + 1);
  int cut = patch.length();
  for (int i = 0; i < patch.length(); i++) {
    char c = patch.charAt(i);
    if (c < '0' || c > '9') {
      cut = i;
      break;
    }
  }
  pat = patch.substring(0, cut).toInt();
}

static int cmpSemver(const String& a, const String& b) {
  int am, an, ap, bm, bn, bp;
  parseSemver(a, am, an, ap);
  parseSemver(b, bm, bn, bp);
  if (am != bm) return am < bm ? -1 : 1;
  if (an != bn) return an < bn ? -1 : 1;
  if (ap != bp) return ap < bp ? -1 : 1;
  return 0;
}

static String normalizeGithubTag(const String& tag) {
  String t = tag;
  t.trim();
  if (!t.length()) return t;
  if (!(t.startsWith("v") || t.startsWith("V"))) t = "v" + t;
  return t;
}

void gitOtaTask(void* arg) {
  GitOta* self = static_cast<GitOta*>(arg);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (self) self->runQueuedWork();
  }
}

void GitOta::begin() {
  status_ = "Ready";
  if (!s_otaMutex) s_otaMutex = xSemaphoreCreateMutex();
  if (!s_otaTask) {
    xTaskCreatePinnedToCore(gitOtaTask, "gitOta", 16384, this, 1, &s_otaTask, 0);
  }
}

void GitOta::configure(const String& /*host*/, uint16_t /*port*/, const String& /*token*/, bool check,
                       bool autoInstall) {
  check_ = check;
  autoInstall_ = autoInstall;
}

void GitOta::setStatus(const String& s) {
  if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    status_ = s;
    xSemaphoreGive(s_otaMutex);
  } else {
    status_ = s;
  }
}

String GitOta::status() const {
  if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    String out = status_;
    xSemaphoreGive(s_otaMutex);
    return out;
  }
  return status_;
}

bool GitOta::busy() const { return busy_; }

void GitOta::setPendingTag(const String& tag) {
  if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    pendingTag_ = tag;
    pendingUrl_ = "";
    xSemaphoreGive(s_otaMutex);
  } else {
    pendingTag_ = tag;
    pendingUrl_ = "";
  }
}

void GitOta::requestCheckNow(bool force) {
  if (busy_ || pending_) {
    setStatus("Busy...");
    return;
  }
  pendingForce_ = force;
  pendingPeriodic_ = false;
  pending_ = true;
  busy_ = true;
  setStatus("Checking...");
  if (s_otaTask) xTaskNotifyGive(s_otaTask);
}

bool GitOta::fetchGithubRelease(const String& tag, UpdateInfo& out) {
  out = UpdateInfo();
  if (WiFi.status() != WL_CONNECTED) {
    out.error = "No WiFi";
    return false;
  }

  String url = String("https://api.github.com/repos/") + kGithubRepo + "/releases/";
  if (tag.length()) {
    url += "tags/" + normalizeGithubTag(tag);
  } else {
    url += "latest";
  }

  WiFiClientSecure client;
  client.setInsecure();  // public release assets; avoids bundling CA store on CYD
  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    out.error = "HTTPS begin failed";
    return false;
  }
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("User-Agent", "solar-viewer-esp32");
  int code = http.GET();
  if (code != 200) {
    out.error = "GitHub HTTP " + String(code);
    http.end();
    return false;
  }

  // Filter keeps RAM low on the release JSON payload.
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;

  JsonDocument doc;
  DeserializationError jerr =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jerr) {
    out.error = "JSON parse";
    return false;
  }

  out.tag = doc["tag_name"] | "";
  out.name = doc["name"] | out.tag;
  JsonArrayConst assets = doc["assets"].as<JsonArrayConst>();
  for (JsonObjectConst asset : assets) {
    String name = asset["name"] | "";
    if (name.startsWith(kAssetPrefix) && name.endsWith(".bin")) {
      out.assetName = name;
      out.assetUrl = asset["browser_download_url"] | "";
      break;
    }
  }
  if (!out.assetUrl.length()) {
    for (JsonObjectConst asset : assets) {
      String name = asset["name"] | "";
      if (name.endsWith(".bin")) {
        out.assetName = name;
        out.assetUrl = asset["browser_download_url"] | "";
        break;
      }
    }
  }
  if (!out.tag.length() || !out.assetUrl.length()) {
    out.error = "No firmware asset";
    return false;
  }
  out.ok = true;
  return true;
}

bool GitOta::checkUpdateInfo(UpdateInfo& out) { return fetchGithubRelease("", out); }

bool GitOta::remoteIsNewer(const String& remoteTag) const {
  if (!remoteTag.length()) return false;
  return cmpSemver(remoteTag, String(FW_VERSION)) > 0;
}

bool GitOta::installFromUrl(const String& url, String& statusOut) {
  if (!url.length()) {
    statusOut = "No asset URL";
    setStatus(statusOut);
    return false;
  }
  setStatus("Downloading...");
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  setStatus("Installing...");
  t_httpUpdate_return ret = httpUpdate.update(client, url, "");
  switch (ret) {
    case HTTP_UPDATE_OK:
      statusOut = "Updated to " + (pendingTag_.length() ? pendingTag_ : String(FW_VERSION));
      setStatus(statusOut);
      return true;
    case HTTP_UPDATE_NO_UPDATES:
      statusOut = "Already current";
      setStatus(statusOut);
      return false;
    case HTTP_UPDATE_FAILED:
    default:
      statusOut = String("Update failed: ") + httpUpdate.getLastErrorString();
      setStatus(statusOut);
      return false;
  }
}

bool GitOta::installLatest(String& statusOut, bool force) {
  if (WiFi.status() != WL_CONNECTED) {
    statusOut = "No WiFi";
    setStatus(statusOut);
    return false;
  }

  UpdateInfo info;
  String wantTag;
  if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    wantTag = pendingTag_;
    xSemaphoreGive(s_otaMutex);
  } else {
    wantTag = pendingTag_;
  }

  setStatus("Checking...");
  if (!fetchGithubRelease(wantTag, info) || !info.tag.length() || !info.assetUrl.length()) {
    statusOut = info.error.length() ? info.error : "No release";
    setStatus(statusOut);
    return false;
  }

  if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    pendingTag_ = info.tag;
    pendingUrl_ = info.assetUrl;
    xSemaphoreGive(s_otaMutex);
  } else {
    pendingTag_ = info.tag;
    pendingUrl_ = info.assetUrl;
  }

  if (!force && !remoteIsNewer(info.tag)) {
    if (cmpSemver(info.tag, String(FW_VERSION)) == 0) {
      statusOut = "Already current (" FW_VERSION ")";
    } else {
      statusOut = "Remote older (" + info.tag + "), keeping " FW_VERSION;
    }
    setStatus(statusOut);
    return false;
  }

  return installFromUrl(info.assetUrl, statusOut);
}

void GitOta::runQueuedWork() {
  const bool periodic = pendingPeriodic_;
  const bool force = pendingForce_;
  pending_ = false;
  pendingPeriodic_ = false;
  busy_ = true;

  String st;
  if (periodic) {
    setStatus("Checking...");
    UpdateInfo info;
    if (!checkUpdateInfo(info) || !info.tag.length()) {
      setStatus(info.error.length() ? info.error : "Check failed");
      busy_ = false;
      return;
    }
    if (!remoteIsNewer(info.tag)) {
      setStatus("Up to date (" FW_VERSION ")");
      busy_ = false;
      return;
    }
    setStatus("Update: " + info.tag);
    if (s_otaMutex && xSemaphoreTake(s_otaMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      pendingTag_ = info.tag;
      pendingUrl_ = info.assetUrl;
      xSemaphoreGive(s_otaMutex);
    } else {
      pendingTag_ = info.tag;
      pendingUrl_ = info.assetUrl;
    }
    if (autoInstall_) {
      installLatest(st, false);
    }
  } else {
    installLatest(st, force);
  }

  busy_ = false;
}

void GitOta::loop() {
  if (pending_ || busy_ || !check_ || WiFi.status() != WL_CONNECTED) return;
  uint32_t interval = autoInstall_ ? 3600000UL : 86400000UL;
  if (millis() - lastCheckMs_ < interval && lastCheckMs_ != 0) return;
  lastCheckMs_ = millis();
  pendingForce_ = false;
  pendingPeriodic_ = true;
  pending_ = true;
  busy_ = true;
  setStatus("Checking...");
  if (s_otaTask) xTaskNotifyGive(s_otaTask);
}
