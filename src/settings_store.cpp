#include "settings_store.h"
#include "config.h"

static uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static bool settingsEqual(const HostSettings& a, const HostSettings& b) {
  return a.hostIp == b.hostIp && a.hostPort == b.hostPort && a.token == b.token && a.pollMs == b.pollMs &&
         a.brightness == b.brightness && a.screenRotation == b.screenRotation &&
         a.glanceLayout == b.glanceLayout && a.themeId == b.themeId && a.useHostConfig == b.useHostConfig &&
         a.gridOfflineAlert == b.gridOfflineAlert && a.checkForUpdate == b.checkForUpdate &&
         a.autoInstallUpdate == b.autoInstallUpdate && a.nightMode == b.nightMode &&
         a.nightBrightness == b.nightBrightness && a.nightStartMin == b.nightStartMin &&
         a.nightEndMin == b.nightEndMin && a.configRev == b.configRev && a.settingsPin == b.settingsPin;
}

static HostSettings s_lastSaved;
static bool s_haveLastSaved = false;

void SettingsStore::begin() { prefs_.begin("solar_disp", false); }

HostSettings SettingsStore::load() {
  HostSettings s;
  s.hostIp = prefs_.getString("host", "");
  if (s.hostIp.length() == 0) s.hostIp = DEFAULT_HOST_IP;
  s.hostPort = prefs_.getUShort("port", DEFAULT_HOST_PORT);
  s.token = prefs_.getString("token", DEFAULT_API_TOKEN);
  s.pollMs = prefs_.getUInt("poll", DEFAULT_POLL_MS);
  s.brightness = clampU8(prefs_.getUChar("bri", 200), 0, 255);
  s.screenRotation = clampU8(prefs_.getUChar("rot", 0), 0, 3);
  s.glanceLayout = clampU8(prefs_.getUChar("glLayout", 0), 0, 4);
  s.themeId = clampU8(prefs_.getUChar("theme", 0), 0, 4);
  s.useHostConfig = prefs_.getBool("useHostCfg", true);
  s.gridOfflineAlert = prefs_.getBool("gridAlert", true);
  s.checkForUpdate = prefs_.getBool("chkUpd", false);
  s.autoInstallUpdate = prefs_.getBool("autoUpd", false);
  s.nightMode = prefs_.getBool("nightMode", false);
  s.nightBrightness = clampU8(prefs_.getUChar("nightBri", 40), 1, 255);
  s.nightStartMin = prefs_.getUShort("nightStart", 22 * 60);
  s.nightEndMin = prefs_.getUShort("nightEnd", 6 * 60);
  if (s.nightStartMin >= 24 * 60) s.nightStartMin = 22 * 60;
  if (s.nightEndMin >= 24 * 60) s.nightEndMin = 6 * 60;
  s.configRev = prefs_.getUInt("cfgRev", 0);
  s.settingsPin = prefs_.getString("setPin", "");
  if (s.settingsPin.length() != 4) s.settingsPin = "";
  if (s.hostPort == 0) s.hostPort = DEFAULT_HOST_PORT;
  if (s.pollMs < 2000) s.pollMs = 2000;
  if (s.autoInstallUpdate) s.checkForUpdate = true;
  s_lastSaved = s;
  s_haveLastSaved = true;
  return s;
}

void SettingsStore::save(const HostSettings& s) {
  if (s_haveLastSaved && settingsEqual(s, s_lastSaved)) return;
  prefs_.putString("host", s.hostIp);
  prefs_.putUShort("port", s.hostPort);
  prefs_.putString("token", s.token);
  prefs_.putUInt("poll", s.pollMs);
  prefs_.putUChar("bri", s.brightness);
  prefs_.putUChar("rot", s.screenRotation);
  prefs_.putUChar("glLayout", s.glanceLayout);
  prefs_.putUChar("theme", s.themeId);
  prefs_.putBool("useHostCfg", s.useHostConfig);
  prefs_.putBool("gridAlert", s.gridOfflineAlert);
  prefs_.putBool("chkUpd", s.checkForUpdate);
  prefs_.putBool("autoUpd", s.autoInstallUpdate);
  prefs_.putBool("nightMode", s.nightMode);
  prefs_.putUChar("nightBri", s.nightBrightness);
  prefs_.putUShort("nightStart", s.nightStartMin);
  prefs_.putUShort("nightEnd", s.nightEndMin);
  prefs_.putUInt("cfgRev", s.configRev);
  prefs_.putString("setPin", s.settingsPin.length() == 4 ? s.settingsPin : "");
  s_lastSaved = s;
  s_haveLastSaved = true;
}
