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
  // Prefer requestCheckNow() from the UI thread — this call blocks.
  bool installLatest(String& statusOut, bool force = false);
  bool remoteIsNewer(const String& remoteTag) const;
  void setPendingTag(const String& tag);
  /** Queue a GitHub check/install on the OTA worker (non-blocking). */
  void requestCheckNow(bool force = false);
  void loop();
  String status() const;
  bool busy() const;

 private:
  friend void gitOtaTask(void* arg);
  bool fetchGithubRelease(const String& tag, UpdateInfo& out);
  bool installFromUrl(const String& url, String& statusOut);
  void setStatus(const String& s);
  void runQueuedWork();

  bool check_ = false;
  bool autoInstall_ = false;
  String status_ = "Ready";
  volatile bool busy_ = false;
  volatile bool pending_ = false;
  volatile bool pendingForce_ = false;
  volatile bool pendingPeriodic_ = false;
  uint32_t lastCheckMs_ = 0;
  String pendingTag_;
  String pendingUrl_;
};

extern GitOta gitOta;
