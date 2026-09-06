#include "git_ota.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

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
  // Ignore build metadata / prerelease suffix after patch digits.
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

// Returns -1 if a<b, 0 if equal, 1 if a>b.
static int cmpSemver(const String& a, const String& b) {
  int am, an, ap, bm, bn, bp;
  parseSemver(a, am, an, ap);
  parseSemver(b, bm, bn, bp);
  if (am != bm) return am < bm ? -1 : 1;
  if (an != bn) return an < bn ? -1 : 1;
  if (ap != bp) return ap < bp ? -1 : 1;
  return 0;
}

void GitOta::begin() { status_ = "Ready"; }

void GitOta::configure(const String& host, uint16_t port, const String& token, bool check, bool autoInstall) {
  host_ = host;
  port_ = port;
  token_ = token;
  check_ = check;
  autoInstall_ = autoInstall;
}

bool GitOta::checkUpdateInfo(UpdateInfo& out) {
  out = UpdateInfo();
  if (WiFi.status() != WL_CONNECTED || host_.length() == 0) return false;
  String url = "http://" + host_ + ":" + String(port_) + "/api/display/update-info";
  if (token_.length()) url += "?token=" + token_;
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  http.addHeader("Accept", "application/json");
  if (token_.length()) http.addHeader("Authorization", "Bearer " + token_);
  int code = http.GET();
  if (code != 200) {
    out.error = "HTTP " + String(code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    out.error = "JSON parse";
    return false;
  }
  out.ok = doc["ok"] | false;
  JsonObjectConst rel = doc["release"];
  if (!rel.isNull()) {
    out.tag = rel["tag"] | "";
    out.name = rel["name"] | out.tag;
    out.assetName = rel["asset_name"] | "";
  }
  if (!out.ok) out.error = doc["error"] | "No release";
  return out.ok;
}

bool GitOta::remoteIsNewer(const String& remoteTag) const {
  if (!remoteTag.length()) return false;
  return cmpSemver(remoteTag, String(FW_VERSION)) > 0;
}

bool GitOta::installLatest(String& statusOut, bool force) {
  if (WiFi.status() != WL_CONNECTED || host_.length() == 0) {
    statusOut = "No host";
    return false;
  }

  // Resolve target tag first so we never silently downgrade on Check now / auto.
  if (!pendingTag_.length() || !force) {
    UpdateInfo info;
    if (!checkUpdateInfo(info) || !info.tag.length()) {
      statusOut = info.error.length() ? info.error : "No release";
      status_ = statusOut;
      return false;
    }
    pendingTag_ = info.tag;
    if (!force && !remoteIsNewer(info.tag)) {
      if (cmpSemver(info.tag, String(FW_VERSION)) == 0) {
        statusOut = "Already current (" FW_VERSION ")";
      } else {
        statusOut = "Remote older (" + info.tag + "), keeping " FW_VERSION;
      }
      status_ = statusOut;
      return false;
    }
  }

  busy_ = true;
  status_ = "Downloading...";
  String url = "http://" + host_ + ":" + String(port_) + "/api/display/firmware/latest.bin";
  if (token_.length()) url += "?token=" + token_;
  if (pendingTag_.length()) {
    url += (url.indexOf('?') >= 0 ? "&" : "?") + String("ver=") + pendingTag_;
  }
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  WiFiClient client;
  status_ = "Installing...";
  // Pass empty current version so ESP HTTPUpdate does not short-circuit on 304;
  // we already gated on semver above (unless force).
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

void GitOta::loop() {
  if (!check_ || busy_ || WiFi.status() != WL_CONNECTED || host_.length() == 0) return;
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
  if (autoInstall_) {
    String st;
    installLatest(st, false);
  }
}
