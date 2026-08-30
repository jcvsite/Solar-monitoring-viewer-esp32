#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <SPI.h>

#include "config.h"
#include "secrets.h"
#include "settings_store.h"
#include "api_client.h"
#include "discovery.h"
#include "touch_input.h"
#include "ui.h"
#include "theme.h"
#include "layout.h"
#include "git_ota.h"
#include "device_web.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

TFT_eSPI tft;

SettingsStore store;
ApiClient api;
Discovery discovery;
Ui ui;

HostSettings settings;
GlanceData glance;
BmsData bms;
HistoryData history;

Page page = Page::Glance;
SettingsTab settingsTab = SettingsTab::Connection;
uint32_t lastPoll = 0;
uint32_t lastConfigPoll = 0;
uint32_t lastGood = 0;
uint32_t lastAnim = 0;
String statusMsg = "Boot";
String otaStatus = "Ready";
bool needRedraw = true;
std::vector<DiscoveredHost> found;
int foundIndex = 0;
uint8_t manualOctets[4] = {192, 168, 1, 240};
uint8_t manualSel = 0;

static const char* kLayoutNames[] = {"Classic", "Compact", "Ring", "Bars", "Flow"};
static const char* kThemeNames[] = {"Dark", "Light", "Solar", "Ocean", "Forest"};

static void pollIfDue(bool force);
static void pollDisplayConfig(bool force);

static const char* rotationLabel(uint8_t r) {
  switch (r & 3) {
    case 1:
      return "Landscape";
    case 2:
      return "Portrait flip";
    case 3:
      return "Landscape flip";
    default:
      return "Portrait";
  }
}

static void applyHostSettingsFromConfig(const DisplayConfig& cfg);

static bool nearEq(float a, float b) {
  if (isnan(a) && isnan(b)) return true;
  if (isnan(a) || isnan(b)) return false;
  return fabsf(a - b) < 1.0f;
}

static bool glanceVisualEqual(const GlanceData& a, const GlanceData& b) {
  return a.title == b.title && a.grid_state == b.grid_state && a.batt_status == b.batt_status &&
         a.batt_time == b.batt_time && a.status == b.status && nearEq(a.soc, b.soc) &&
         nearEq(a.batt_w, b.batt_w) && nearEq(a.pv_w, b.pv_w) && nearEq(a.load_w, b.load_w) &&
         nearEq(a.grid_w, b.grid_w) && nearEq(a.pv_today_kwh, b.pv_today_kwh) &&
         nearEq(a.load_today_kwh, b.load_today_kwh);
}

static bool glanceGridAlert(const GlanceData& g) {
  if (!settings.gridOfflineAlert) return false;
  return g.grid_state == "offline" || g.grid_state == "brownout";
}

static void loadManualOctetsFromSettings() {
  if (settings.hostIp.length()) {
    int start = 0;
    for (int i = 0; i < 4; i++) {
      int dot = settings.hostIp.indexOf('.', start);
      String part = (dot >= 0) ? settings.hostIp.substring(start, dot) : settings.hostIp.substring(start);
      manualOctets[i] = (uint8_t)constrain(part.toInt(), 0, 255);
      if (dot < 0) break;
      start = dot + 1;
    }
  } else {
    IPAddress lip = WiFi.localIP();
    manualOctets[0] = 192;
    manualOctets[1] = 168;
    manualOctets[2] = 1;
    manualOctets[3] = 240;
    if (lip != IPAddress((uint32_t)0)) {
      manualOctets[0] = lip[0];
      manualOctets[1] = lip[1];
      manualOctets[2] = lip[2];
    }
  }
}

static String manualOctetsToIp() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", manualOctets[0], manualOctets[1], manualOctets[2], manualOctets[3]);
  return String(buf);
}

static void showHostChoice() {
  discovery.cancelSearch();
  page = Page::HostChoice;
  statusMsg = "";
  needRedraw = true;
}

static void openManualHost() {
  discovery.cancelSearch();
  loadManualOctetsFromSettings();
  manualSel = 0;
  page = Page::ManualHost;
  statusMsg = "Tap octet, use +/-";
  needRedraw = true;
}

static void startHostDiscovery() {
  found.clear();
  discovery.cancelSearch();
  discovery.beginSearch(settings.hostPort);
  page = Page::FindingHost;
  statusMsg = "Searching...";
  needRedraw = true;
}

static void finishHostDiscovery() {
  if (!found.empty()) {
    foundIndex = 0;
    settings.hostIp = found[0].ip;
    settings.hostPort = found[0].port;
    store.save(settings);
    statusMsg = "Found " + settings.hostIp;
    page = Page::Glance;
    pollIfDue(true);
  } else {
    statusMsg = "Not found";
    showHostChoice();
  }
  needRedraw = true;
}

static const char* kApNameBase = WIFI_SETUP_AP_NAME;

static String uniqueApName() {
  uint32_t id = (uint32_t)ESP.getEfuseMac();
  char buf[40];
  snprintf(buf, sizeof(buf), "%s-%04X", kApNameBase, (unsigned)(id & 0xFFFF));
  return String(buf);
}

static String uniqueHostname() {
  uint32_t id = (uint32_t)ESP.getEfuseMac();
  char buf[32];
  snprintf(buf, sizeof(buf), "esp32-solar-%06X", (unsigned)(id & 0xFFFFFF));
  return String(buf);
}

static void setBrightness(uint8_t v) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, v);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, v);
#endif
}

static void applyScreenRotation() {
  settings.screenRotation = (uint8_t)constrain(settings.screenRotation, (int)0, (int)3);
  tft.setRotation(settings.screenRotation);
  touchInputSetRotation(settings.screenRotation);
  ui.setRotation(settings.screenRotation);
}

static TouchSample lastTouch;
static uint32_t lastTouchAt = 0;
static String gPortalApName;

static void configModeCallback(WiFiManager* wm) {
  (void)wm;
  ui.drawWifiPortal(tft, gPortalApName.c_str());
}

static bool ensureWifi(bool forcePortal) {
  WiFi.mode(WIFI_STA);
  gPortalApName = uniqueApName();
  String hostname = uniqueHostname();

  String seedSsid = WIFI_SSID;
  String seedPass = WIFI_PASSWORD;
  bool seedOk = seedSsid.length() > 0 && seedSsid != "YourWiFiSSID";

  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setConfigPortalTimeout(300);
  wm.setAPCallback(configModeCallback);
  wm.setTitle("Solar Display WiFi");
  wm.setHostname(hostname.c_str());

  if (forcePortal) {
    ui.drawWifiPortal(tft, gPortalApName.c_str());
    bool ok = wm.startConfigPortal(gPortalApName.c_str());
    return ok && WiFi.status() == WL_CONNECTED;
  }

  ui.drawSplash(tft, "WiFi...");
  bool ok = false;
  if (seedOk) {
    ok = wm.autoConnect(gPortalApName.c_str(), NULL);
    if (!ok) {
      WiFi.begin(seedSsid.c_str(), seedPass.c_str());
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(200);
      ok = WiFi.status() == WL_CONNECTED;
      if (!ok) {
        ui.drawWifiPortal(tft, gPortalApName.c_str());
        ok = wm.startConfigPortal(gPortalApName.c_str());
      }
    }
  } else {
    ok = wm.autoConnect(gPortalApName.c_str());
  }
  return ok && WiFi.status() == WL_CONNECTED;
}

static void applyHostSettingsFromConfig(const DisplayConfig& cfg) {
  bool changed = false;
  if (cfg.config_rev > settings.configRev) {
    settings.configRev = cfg.config_rev;
    changed = true;
  }
  if (settings.glanceLayout != cfg.glance_layout) {
    settings.glanceLayout = cfg.glance_layout;
    changed = true;
  }
  if (settings.themeId != cfg.theme) {
    settings.themeId = cfg.theme;
    ui.setTheme(settings.themeId);
    changed = true;
  }
  if (settings.screenRotation != cfg.rotation) {
    settings.screenRotation = cfg.rotation;
    applyScreenRotation();
    changed = true;
  }
  if (settings.brightness != cfg.brightness) {
    settings.brightness = cfg.brightness;
    setBrightness(settings.brightness);
    changed = true;
  }
  if (settings.pollMs != cfg.poll_ms) {
    settings.pollMs = cfg.poll_ms;
    changed = true;
  }
  if (settings.checkForUpdate != cfg.check_for_update) {
    settings.checkForUpdate = cfg.check_for_update;
    changed = true;
  }
  if (settings.autoInstallUpdate != cfg.auto_install_update) {
    settings.autoInstallUpdate = cfg.auto_install_update;
    changed = true;
  }
  if (settings.gridOfflineAlert != cfg.grid_offline_alert) {
    settings.gridOfflineAlert = cfg.grid_offline_alert;
    changed = true;
  }
  gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                   settings.autoInstallUpdate);
  if (changed) {
    store.save(settings);
    deviceWeb.applySettings(settings);
    needRedraw = true;
  }
  if (cfg.force_update) {
    String st;
    gitOta.installLatest(st);
    otaStatus = st;
  }
}

static void pollDisplayConfig(bool force) {
  if (!settings.useHostConfig) return;
  if (!force && millis() - lastConfigPoll < 30000) return;
  lastConfigPoll = millis();
  if (settings.hostIp.length() == 0 || WiFi.status() != WL_CONNECTED) return;
  DisplayConfig cfg;
  if (api.fetchDisplayConfig(settings.hostIp, settings.hostPort, settings.token, cfg)) {
    applyHostSettingsFromConfig(cfg);
  }
}

static void pollIfDue(bool force) {
  if (!force && millis() - lastPoll < settings.pollMs) return;
  lastPoll = millis();
  if (settings.hostIp.length() == 0 || WiFi.status() != WL_CONNECTED) return;

  if (page == Page::Glance || force) {
    GlanceData g;
    if (api.fetchGlance(settings.hostIp, settings.hostPort, settings.token, g)) {
      if (g.tz_offset_sec >= -43200 && g.tz_offset_sec <= 50400) {
        ui.syncClockTimezone(g.tz_offset_sec);
      }
      bool headerChanged = (g.clock != glance.clock) || (g.weather_enabled != glance.weather_enabled) ||
                           (g.weather_code != glance.weather_code) || (g.weather_temp != glance.weather_temp);
      if (force || !glanceVisualEqual(glance, g)) {
        glance = g;
        needRedraw = true;
      } else {
        glance = g;
        if (page == Page::Glance && headerChanged) {
          ui.refreshHeaderTime(tft, glance);
        }
      }
      lastGood = millis();
    }
  }
  if (page == Page::Bms) {
    BmsData b;
    if (api.fetchBms(settings.hostIp, settings.hostPort, settings.token, b)) {
      bms = b;
      lastGood = millis();
      needRedraw = true;
    }
  }
  if (page == Page::History) {
    HistoryData h;
    if (api.fetchHistory(settings.hostIp, settings.hostPort, settings.token, 24, h)) {
      history = h;
      lastGood = millis();
      needRedraw = true;
    }
  }
}

static bool pollTouch(TouchSample& s) {
  static uint32_t lastScan = 0;
  if (millis() - lastScan < 25) return false;
  lastScan = millis();
  if (!touchInputSample(s)) return false;
  lastTouch = s;
  lastTouchAt = millis();
  return true;
}

static void handleTouch() {
  TouchSample s;
  static uint32_t lastTap = 0;
  if (!pollTouch(s)) return;
  if (millis() - lastTap < 300) return;
  lastTap = millis();

  int16_t x = s.x;
  int16_t y = s.y;
  uint8_t rot = settings.screenRotation;
  int sh = scrH(rot);

  Page navPage;
  if (ui.navPageAt(x, y, navPage, rot) && (uint8_t)page <= (uint8_t)Page::Settings) {
    page = navPage;
    settingsTab = SettingsTab::Connection;
    pollIfDue(true);
    needRedraw = true;
    return;
  }

  if (page == Page::HostChoice) {
    if (y >= 120 && y <= 192) {
      startHostDiscovery();
      return;
    }
    if (y >= 210 && y <= 282) {
      openManualHost();
      return;
    }
    return;
  }

  if (page == Page::FindingHost) {
    if (y >= 210 && y <= 260) {
      showHostChoice();
      return;
    }
    return;
  }

  if (page == Page::ManualHost) {
    int w = scrW(rot);
    const int octX[4] = {8, 66, 124, 182};
    for (int i = 0; i < 4; i++) {
      if (y >= 52 && y <= 92 && x >= octX[i] && x <= octX[i] + 48) {
        manualSel = (uint8_t)i;
        needRedraw = true;
        return;
      }
    }
    if (y >= 110 && y <= 150 && x >= 15 && x <= 75) {
      manualOctets[manualSel] = (manualOctets[manualSel] == 0) ? 255 : manualOctets[manualSel] - 1;
      needRedraw = true;
      return;
    }
    if (y >= 110 && y <= 150 && x >= w - 75 && x <= w - 15) {
      manualOctets[manualSel] = (manualOctets[manualSel] == 255) ? 0 : manualOctets[manualSel] + 1;
      needRedraw = true;
      return;
    }
    if (y >= 165 && y <= 195 && x >= 15 && x <= 75) {
      if (settings.hostPort > 1) settings.hostPort--;
      needRedraw = true;
      return;
    }
    if (y >= 165 && y <= 195 && x >= w - 75 && x <= w - 15) {
      if (settings.hostPort < 65535) settings.hostPort++;
      needRedraw = true;
      return;
    }
    if (y >= 205 && y <= 239 && x >= 10 && x <= w / 2 - 5) {
      String ip = manualOctetsToIp();
      statusMsg = "Testing...";
      needRedraw = true;
      ui.drawManualHost(tft, manualOctets, manualSel, settings.hostPort, statusMsg);
      String title;
      if (api.probeHost(ip, settings.hostPort, settings.token, title)) {
        statusMsg = title.length() ? title : "Host OK";
      } else {
        statusMsg = "Not reachable";
      }
      needRedraw = true;
      return;
    }
    if (y >= 205 && y <= 239 && x >= w / 2 + 5 && x <= w - 10) {
      settings.hostIp = manualOctetsToIp();
      store.save(settings);
      deviceWeb.applySettings(settings);
      statusMsg = "Saved";
      page = Page::Glance;
      pollIfDue(true);
      needRedraw = true;
      return;
    }
    if (y >= 248 && y <= 282 && x >= 10 && x <= w - 10) {
      page = settings.hostIp.length() ? Page::Settings : Page::HostChoice;
      statusMsg = "";
      needRedraw = true;
      return;
    }
    return;
  }

  if (page == Page::PickLayout) {
    int idx = 0;
    if (ui.pickListAt(x, y, 36, 5, idx, rot)) {
      settings.glanceLayout = (uint8_t)idx;
      store.save(settings);
      page = Page::Settings;
      needRedraw = true;
      return;
    }
    if (y >= sh - 40) {
      page = Page::Settings;
      needRedraw = true;
    }
    return;
  }

  if (page == Page::PickTheme) {
    int idx = 0;
    if (ui.pickListAt(x, y, 36, 5, idx, rot)) {
      settings.themeId = (uint8_t)idx;
      ui.setTheme(settings.themeId);
      store.save(settings);
      page = Page::Settings;
      needRedraw = true;
      return;
    }
    if (y >= sh - 40) {
      page = Page::Settings;
      needRedraw = true;
    }
    return;
  }

  if (page == Page::Settings || page == Page::SettingsConn || page == Page::SettingsUpdates) {
    SettingsTab tab;
    if (ui.settingsTabAt(x, y, tab, rot)) {
      settingsTab = tab;
      needRedraw = true;
      return;
    }

    if (settingsTab == SettingsTab::Connection) {
      if (y >= 122 && y <= 150 && x >= 8 && x <= scrW(rot) - 8) {
        settings.screenRotation = (settings.screenRotation + 1) & 3;
        store.save(settings);
        applyScreenRotation();
        statusMsg = rotationLabel(settings.screenRotation);
        needRedraw = true;
        return;
      }
      if (y >= 154 && y <= 182) {
        page = Page::PickLayout;
        needRedraw = true;
        return;
      }
      if (y >= 186 && y <= 214) {
        page = Page::PickTheme;
        needRedraw = true;
        return;
      }
      int w = scrW(rot);
      int cardW = w - 16;
      if (y >= 220 && y <= 248 && x >= 8 && x <= 8 + cardW / 2 - 4) {
        startHostDiscovery();
        return;
      }
      if (y >= 220 && y <= 248 && x >= 8 + cardW / 2 + 4) {
        openManualHost();
        return;
      }
      if (y >= 254 && y <= 282) {
        statusMsg = "Opening portal...";
        needRedraw = true;
        if (ensureWifi(true)) {
          statusMsg = "WiFi OK " + WiFi.localIP().toString();
        } else {
          statusMsg = "WiFi setup cancelled";
        }
        needRedraw = true;
        return;
      }
    } else {
      if (y >= 140 && y <= 168) {
        settings.checkForUpdate = !settings.checkForUpdate;
        if (!settings.checkForUpdate) settings.autoInstallUpdate = false;
        store.save(settings);
        gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                         settings.autoInstallUpdate);
        needRedraw = true;
        return;
      }
      if (y >= 172 && y <= 200) {
        settings.autoInstallUpdate = !settings.autoInstallUpdate;
        if (settings.autoInstallUpdate) settings.checkForUpdate = true;
        store.save(settings);
        gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                         settings.autoInstallUpdate);
        needRedraw = true;
        return;
      }
      if (y >= 204 && y <= 232) {
        settings.gridOfflineAlert = !settings.gridOfflineAlert;
        store.save(settings);
        needRedraw = true;
        return;
      }
      if (y >= 236 && y <= 264) {
        settings.useHostConfig = !settings.useHostConfig;
        store.save(settings);
        if (settings.useHostConfig) pollDisplayConfig(true);
        needRedraw = true;
        return;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  store.begin();
  settings = store.load();

  themeSetActive(settings.themeId);
  touchInputBegin();

  tft.init();
  applyScreenRotation();
  ui.setTheme(settings.themeId);
  setBrightness(settings.brightness);
  ui.begin(tft);
  ui.drawSplash(tft, "Starting...");

  gitOta.begin();
  gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                   settings.autoInstallUpdate);

  if (ensureWifi(false)) {
    ui.drawSplash(tft, WiFi.localIP().toString().c_str());
    deviceWeb.applySettings(settings);
    deviceWeb.begin();
    ui.ensureClock();
    {
      struct tm ti;
      uint32_t t0 = millis();
      while (millis() - t0 < 3000) {
        if (getLocalTime(&ti)) break;
        delay(200);
      }
    }
    delay(400);
  } else {
    ui.drawSplash(tft, "No WiFi — use Settings");
    page = Page::Settings;
    statusMsg = "Tap WIFI SETUP";
    delay(1000);
  }

  page = Page::Glance;
  statusMsg = settings.hostIp;

  pollDisplayConfig(true);
  pollIfDue(true);
  needRedraw = true;
}

void loop() {
  handleTouch();

  if (gDeviceWebSaved) {
    gDeviceWebSaved = false;
    settings = deviceWeb.settings();
    store.save(settings);
    applyScreenRotation();
    ui.setTheme(settings.themeId);
    setBrightness(settings.brightness);
    needRedraw = true;
  }

  deviceWeb.loop();
  gitOta.loop();
  otaStatus = gitOta.status();

  if (page == Page::FindingHost && discovery.isSearching()) {
    if (discovery.tickSearch(api, settings.token, found)) {
      finishHostDiscovery();
    }
  }

  static bool wasStale = false;
  bool stale = (lastGood == 0) || (millis() - lastGood > STALE_MS);
  if (stale != wasStale) {
    wasStale = stale;
    needRedraw = true;
  }

  pollIfDue(false);
  pollDisplayConfig(false);

  static uint32_t lastClock = 0;
  if (page == Page::Glance && WiFi.status() == WL_CONNECTED && millis() - lastClock >= 30000) {
    lastClock = millis();
    ui.ensureClock();
    ui.refreshHeaderTime(tft, glance);
  }

  if (page == Page::Glance && glanceGridAlert(glance) && millis() - lastAnim >= 500) {
    lastAnim = millis();
    ui.drawGlanceGridPulse(tft, glance, millis(), settings.gridOfflineAlert);
  }

  if (needRedraw) {
    needRedraw = false;
    switch (page) {
      case Page::Glance:
        ui.drawGlance(tft, glance, stale, millis(), settings.glanceLayout, settings.gridOfflineAlert);
        break;
      case Page::Bms:
        ui.drawBms(tft, bms);
        break;
      case Page::History:
        ui.drawHistory(tft, history);
        break;
      case Page::Settings:
      case Page::SettingsConn:
      case Page::SettingsUpdates:
        ui.drawSettings(tft, settings, WiFi.status() == WL_CONNECTED, WiFi.SSID(),
                        settingsTab == SettingsTab::Updates ? otaStatus : statusMsg, settingsTab);
        break;
      case Page::PickLayout:
        ui.drawPickList(tft, "Layout", kLayoutNames, 5, settings.glanceLayout);
        break;
      case Page::PickTheme:
        ui.drawPickList(tft, "Theme", kThemeNames, 5, settings.themeId);
        break;
      case Page::FindingHost:
        ui.drawFindingHost(tft, statusMsg);
        break;
      case Page::HostChoice:
        ui.drawHostChoice(tft, WiFi.SSID());
        break;
      case Page::ManualHost:
        ui.drawManualHost(tft, manualOctets, manualSel, settings.hostPort, statusMsg);
        break;
    }
  }

#if TOUCH_SHOW_DEBUG
  if (lastTouch.active && millis() - lastTouchAt < 3000) {
    ui.drawTouchDebug(tft, lastTouch);
  }
#endif

  delay(20);
}
