#include "git_ota.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static const char* kGithubRepo = "jcvsite/Solar-monitoring-viewer-esp32";
static const char* kAssetPrefix = "solar-viewer-cyd_esp32";

GitOta gitOta;

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

void GitOta::begin() { status_ = "Ready"; }

void GitOta::configure(const String& /*host*/, uint16_t /*port*/, const String& /*token*/, bool check,
                       bool autoInstall) {
  check_ = check;
  autoInstall_ = autoInstall;
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
    return false;
  }
  busy_ = true;
  status_ = "Downloading...";
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  status_ = "Installing...";
  t_httpUpdate_return ret = httpUpdate.update(client, url, "");
  busy_ = false;
  switch (ret) {
    case HTTP_UPDATE_OK:
      statusOut = "Updated to " + (pendingTag_.length() ? pendingTag_ : String(FW_VERSION));
      status_ = statusOut;
      return true;
    case HTTP_UPDATE_NO_UPDATES:
      statusOut = "Already current";
      status_ = statusOut;
      return false;
    case HTTP_UPDATE_FAILED:
    default:
      statusOut = String("Update failed: ") + httpUpdate.getLastErrorString();
      status_ = statusOut;
      return false;
  }
}

bool GitOta::installLatest(String& statusOut, bool force) {
  if (WiFi.status() != WL_CONNECTED) {
    statusOut = "No WiFi";
    status_ = statusOut;
    return false;
  }

  UpdateInfo info;
  const String wantTag = pendingTag_;
  if (!fetchGithubRelease(wantTag, info) || !info.tag.length() || !info.assetUrl.length()) {
    statusOut = info.error.length() ? info.error : "No release";
    status_ = statusOut;
    return false;
  }
  pendingTag_ = info.tag;
  pendingUrl_ = info.assetUrl;

  if (!force && !remoteIsNewer(info.tag)) {
    if (cmpSemver(info.tag, String(FW_VERSION)) == 0) {
      statusOut = "Already current (" FW_VERSION ")";
    } else {
      statusOut = "Remote older (" + info.tag + "), keeping " FW_VERSION;
    }
    status_ = statusOut;
    return false;
  }

  return installFromUrl(pendingUrl_, statusOut);
}

void GitOta::loop() {
  if (!check_ || busy_ || WiFi.status() != WL_CONNECTED) return;
  uint32_t interval = autoInstall_ ? 3600000UL : 86400000UL;
  if (millis() - lastCheckMs_ < interval && lastCheckMs_ != 0) return;
  lastCheckMs_ = millis();
  UpdateInfo info;
  if (!checkUpdateInfo(info) || !info.tag.length()) {
    status_ = info.error.length() ? info.error : "Check failed";
    return;
  }
  if (!remoteIsNewer(info.tag)) {
    status_ = "Up to date (" FW_VERSION ")";
    return;
  }
  status_ = "Update: " + info.tag;
  pendingTag_ = info.tag;
  pendingUrl_ = info.assetUrl;
  if (autoInstall_) {
    String st;
    installLatest(st, false);
  }
}
