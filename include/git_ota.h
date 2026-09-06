#pragma once
#include <Arduino.h>

struct UpdateInfo {
  bool ok = false;
  String tag;
  String name;
  String assetName;
  String assetUrl;
  String error;
};

class GitOta {
 public:
  void begin();
  // host/port/token kept for call-site compatibility; OTA talks to GitHub directly.
  void configure(const String& host, uint16_t port, const String& token, bool check, bool autoInstall);
  bool checkUpdateInfo(UpdateInfo& out);
  // Install latest (or pending tag). force=true skips "remote newer" gate.
  bool installLatest(String& statusOut, bool force = false);
  bool remoteIsNewer(const String& remoteTag) const;
  void setPendingTag(const String& tag) { pendingTag_ = tag; pendingUrl_ = ""; }
  void loop();
  const String& status() const { return status_; }
  bool busy() const { return busy_; }

 private:
  bool fetchGithubRelease(const String& tag, UpdateInfo& out);
  bool installFromUrl(const String& url, String& statusOut);

  bool check_ = false;
  bool autoInstall_ = false;
  String status_ = "Ready";
  bool busy_ = false;
  uint32_t lastCheckMs_ = 0;
  String pendingTag_;
  String pendingUrl_;
};

extern GitOta gitOta;
