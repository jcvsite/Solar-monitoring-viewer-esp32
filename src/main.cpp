#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <vector>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <esp_wifi.h>
#include <stdio.h>

#include "config.h"
#include "secrets.h"
#include "settings_store.h"
#include "api_client.h"
#include "discovery.h"
#include "touch_input.h"
#include "lvgl_port.h"
#include "host_poll.h"
#include "ui.h"
#include "ui_lv.h"
#include "ui_lv/ui_actions.h"
#include "ui_lv/ui_shell.h"
#include "theme.h"
#include "layout.h"
#include "git_ota.h"
#include "device_web.h"
#include "wifi_setup.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

TFT_eSPI tft;

SettingsStore store;
ApiClient api;
Discovery discovery;
UiLv ui;

HostSettings settings;
GlanceData glance;
BmsData bms;
HistoryData history;

UiPage page = UiPage::Glance;
UiSettingsTab settingsTab = UiSettingsTab::Connection;
uint32_t lastPoll = 0;
uint32_t lastConfigPoll = 0;
uint32_t lastGood = 0;
uint32_t lastAnim = 0;
String statusMsg = "Boot";
String otaStatus = "Ready";
bool needRedraw = true;
static bool s_pollAfterUi = false;
std::vector<DiscoveredHost> found;
uint8_t manualOctets[4] = {192, 168, 1, 240};
uint8_t manualSel = 0;

std::vector<WifiNetwork> wifiNets;
int wifiSelected = 0;
String wifiPickSsid;
String wifiPassword;
bool wifiShowPass = false;
String wifiStatus;

bool settingsPinUnlocked = false;
String pinEntry;
String pinStatus;
String pinSetFirst;
uint8_t pinSetPhase = 0;
UiPage pinReturnPage = UiPage::Glance;

static void pollIfDue(bool force);
static void pollDisplayConfig(bool force);
static bool glanceGridAlert(const GlanceData& g);

static void refreshCurrentPage();
static void handleUiAction(UiActionId id, const UiActionCtx& ctx);
static void openSettingsPage();
static void applyScreenRotation();
static uint32_t s_lastLocalRotateMs = 0;
static void startHostDiscovery();
static void openManualHost();
static void startWifiScan();
static bool ensureWifi(bool forcePortal);
static bool connectPickedWifi(const String& ssid, const String& pass);
static void flushUi();
static void showHostChoice();
static String manualOctetsToIp();
static void handlePinPadCommon(bool forSet);
static void applyEffectiveBrightness();
static uint8_t cycleBrightnessStep(uint8_t cur, const uint8_t* steps, size_t n);

static void refreshCurrentPage() {
  const bool stale = (lastGood == 0) || (millis() - lastGood > STALE_MS);
  switch (page) {
    case UiPage::Glance:
      ui.updateGlance(glance, stale, glanceGridAlert(glance), settings.glanceLayout);
      break;
    case UiPage::Bms:
      ui.updateBms(bms);
      break;
    case UiPage::History:
      ui.updateHistory(history);
      break;
    case UiPage::Settings:
    case UiPage::SettingsConn:
    case UiPage::SettingsUpdates:
      ui.updateSettings(settings, WiFi.status() == WL_CONNECTED, WiFi.SSID(),
                        settingsTab == UiSettingsTab::Updates ? otaStatus : statusMsg, otaStatus, settingsTab,
                        FW_VERSION);
      break;
    case UiPage::PickLayout:
      ui.showPickList("Layout", layoutNames(), 5, settings.glanceLayout, false);
      break;
    case UiPage::PickTheme:
      ui.showPickList("Theme", themeNames(), themeCount(), settings.themeId, true);
      break;
    case UiPage::FindingHost:
      ui.showFindingHost(statusMsg);
      break;
    case UiPage::HostChoice:
      ui.showHostChoice(WiFi.SSID());
      break;
    case UiPage::ManualHost:
      ui.showManualHost(manualOctets, manualSel, settings.hostPort, statusMsg);
      break;
    case UiPage::WifiPick:
      ui.showWifiNetworks(wifiNets, wifiSelected, wifiStatus);
      break;
    case UiPage::WifiPassword:
      ui.showWifiPassword(wifiPickSsid, wifiPassword, wifiShowPass, wifiStatus);
      break;
    case UiPage::PinUnlock:
      ui.showPinPad("Settings locked", pinEntry, "Enter 4-digit PIN", pinStatus);
      break;
    case UiPage::PinSet:
      ui.showPinPad("Settings PIN", pinEntry, pinSetPhase ? "Confirm PIN" : "New PIN (4 digits)", pinStatus);
      break;
    default:
      break;
  }
}

static void handleUiAction(UiActionId id, const UiActionCtx& ctx) {
  switch (id) {
    case UiActionId::NavPage:
      settingsPinUnlocked = false;
      page = ctx.page;
      settingsTab = UiSettingsTab::Connection;
      // Paint first — never block the tap handler on HTTP (was up to 4s freeze).
      needRedraw = true;
      s_pollAfterUi = true;
      break;
    case UiActionId::OpenSettings:
      openSettingsPage();
      break;
    case UiActionId::SettingsTab:
      settingsTab = ctx.settingsTab;
      // LVGL already switched the tab — no full rebuild.
      break;
    case UiActionId::CycleBrightness: {
      static const uint8_t kDayBri[] = {80, 140, 200, 255};
      settings.brightness = cycleBrightnessStep(settings.brightness, kDayBri, sizeof(kDayBri) / sizeof(kDayBri[0]));
      store.save(settings);
      applyEffectiveBrightness();
      needRedraw = true;
      break;
    }
    case UiActionId::ToggleNightMode:
      settings.nightMode = !settings.nightMode;
      store.save(settings);
      applyEffectiveBrightness();
      needRedraw = true;
      break;
    case UiActionId::CycleNightBrightness: {
      static const uint8_t kNightBri[] = {10, 25, 40, 80};
      settings.nightBrightness = cycleBrightnessStep(settings.nightBrightness, kNightBri, sizeof(kNightBri) / sizeof(kNightBri[0]));
      store.save(settings);
      applyEffectiveBrightness();
      needRedraw = true;
      break;
    }
    case UiActionId::CycleNightStart: {
      uint8_t h = (uint8_t)((settings.nightStartMin / 60 + 1) % 24);
      settings.nightStartMin = (uint16_t)(h * 60);
      store.save(settings);
      applyEffectiveBrightness();
      needRedraw = true;
      break;
    }
    case UiActionId::CycleNightEnd: {
      uint8_t h = (uint8_t)((settings.nightEndMin / 60 + 1) % 24);
      settings.nightEndMin = (uint16_t)(h * 60);
      store.save(settings);
      applyEffectiveBrightness();
      needRedraw = true;
      break;
    }
    case UiActionId::OpenPickLayout:
      page = UiPage::PickLayout;
      needRedraw = true;
      break;
    case UiActionId::OpenPickTheme:
      page = UiPage::PickTheme;
      needRedraw = true;
      break;
    case UiActionId::PickLayout:
      if (ctx.index >= 0 && ctx.index <= 4) {
        settings.glanceLayout = (uint8_t)ctx.index;
        store.save(settings);
      }
      page = UiPage::Settings;
      needRedraw = true;
      break;
    case UiActionId::PickTheme:
      if (ctx.index >= 0 && ctx.index <= 4) {
        settings.themeId = (uint8_t)ctx.index;
        ui.setTheme(settings.themeId);
        store.save(settings);
      }
      page = UiPage::Settings;
      needRedraw = true;
      break;
    case UiActionId::RotateScreen: {
      static uint32_t lastRotBtn = 0;
      if (millis() - lastRotBtn < 750) break;
      lastRotBtn = millis();
      s_lastLocalRotateMs = millis();
      settings.screenRotation = (settings.screenRotation + 1) & 3;
      store.save(settings);
      applyScreenRotation();
      statusMsg = rotationLabel(settings.screenRotation);
      needRedraw = true;
      break;
    }
    case UiActionId::OpenPinSet:
      page = UiPage::PinSet;
      pinSetPhase = 0;
      pinSetFirst = "";
      pinEntry = "";
      pinStatus = "New PIN (4 digits)";
      needRedraw = true;
      break;
    case UiActionId::StartHostDiscovery:
      startHostDiscovery();
      break;
    case UiActionId::OpenManualHost:
      openManualHost();
      break;
    case UiActionId::StartWifiScan:
      page = UiPage::WifiPick;
      startWifiScan();
      break;
    case UiActionId::WifiRescan:
      startWifiScan();
      break;
    case UiActionId::WifiPhonePortal:
      statusMsg = "Opening phone portal...";
      needRedraw = true;
      if (ensureWifi(true)) {
        statusMsg = "WiFi OK " + WiFi.localIP().toString();
        page = UiPage::Settings;
      } else {
        statusMsg = "Portal cancelled";
      }
      needRedraw = true;
      break;
    case UiActionId::WifiPickNetwork:
      if (ctx.index >= 0 && ctx.index < (int)wifiNets.size()) {
        wifiSelected = ctx.index;
        wifiPickSsid = wifiNets[ctx.index].ssid;
        if (!wifiNets[ctx.index].secure) {
          if (connectPickedWifi(wifiPickSsid, "")) {
            statusMsg = wifiStatus;
            page = settings.hostIp.length() ? UiPage::Glance : UiPage::HostChoice;
          } else {
            page = UiPage::WifiPick;
          }
        } else {
          wifiPassword = "";
          wifiShowPass = false;
          wifiStatus = "Enter password";
          page = UiPage::WifiPassword;
        }
        needRedraw = true;
      }
      break;
    case UiActionId::WifiConnect: {
      wifiPassword = ui.getWifiPasswordInput();
      if (connectPickedWifi(wifiPickSsid, wifiPassword)) {
        statusMsg = wifiStatus;
        page = settings.hostIp.length() ? UiPage::Glance : UiPage::HostChoice;
      } else {
        page = UiPage::WifiPassword;
      }
      needRedraw = true;
      break;
    }
    case UiActionId::WifiToggleShowPass:
      wifiPassword = ui.getWifiPasswordInput();
      wifiShowPass = !wifiShowPass;
      needRedraw = true;
      break;
    case UiActionId::WifiBack:
      if (page == UiPage::WifiPassword) {
        page = UiPage::WifiPick;
        wifiStatus = "Tap your network";
      } else {
        page = UiPage::Settings;
        statusMsg = wifiStatus;
      }
      needRedraw = true;
      break;
    case UiActionId::HostChoiceDiscover:
      startHostDiscovery();
      break;
    case UiActionId::HostChoiceManual:
      openManualHost();
      break;
    case UiActionId::HostChoiceSkip:
      page = UiPage::Glance;
      needRedraw = true;
      break;
    case UiActionId::FindingHostCancel:
      showHostChoice();
      break;
    case UiActionId::ManualOctetSelect:
      manualSel = (uint8_t)ctx.index;
      needRedraw = true;
      break;
    case UiActionId::ManualOctetDec:
      manualOctets[manualSel] = (manualOctets[manualSel] == 0) ? 255 : manualOctets[manualSel] - 1;
      needRedraw = true;
      break;
    case UiActionId::ManualOctetInc:
      manualOctets[manualSel] = (manualOctets[manualSel] == 255) ? 0 : manualOctets[manualSel] + 1;
      needRedraw = true;
      break;
    case UiActionId::ManualPortDec:
      if (settings.hostPort > 1) settings.hostPort--;
      needRedraw = true;
      break;
    case UiActionId::ManualPortInc:
      if (settings.hostPort < 65535) settings.hostPort++;
      needRedraw = true;
      break;
    case UiActionId::ManualTest: {
      const String ip = manualOctetsToIp();
      statusMsg = "Testing...";
      needRedraw = true;
      refreshCurrentPage();
      flushUi();
      String title;
      if (api.probeHost(ip, settings.hostPort, settings.token, title)) {
        statusMsg = title.length() ? title : "Host OK";
      } else {
        statusMsg = "Not reachable";
      }
      needRedraw = true;
      break;
    }
    case UiActionId::ManualSave:
      settings.hostIp = manualOctetsToIp();
      store.save(settings);
      deviceWeb.applySettings(settings);
      statusMsg = "Saved";
      page = UiPage::Glance;
      pollIfDue(true);
      needRedraw = true;
      break;
    case UiActionId::ManualBack:
      page = settings.hostIp.length() ? UiPage::Settings : UiPage::HostChoice;
      statusMsg = "";
      needRedraw = true;
      break;
    case UiActionId::ToggleCheckUpdate:
      settings.checkForUpdate = !settings.checkForUpdate;
      if (!settings.checkForUpdate) settings.autoInstallUpdate = false;
      store.save(settings);
      gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                       settings.autoInstallUpdate);
      needRedraw = true;
      break;
    case UiActionId::ToggleAutoUpdate:
      settings.autoInstallUpdate = !settings.autoInstallUpdate;
      if (settings.autoInstallUpdate) settings.checkForUpdate = true;
      store.save(settings);
      gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                       settings.autoInstallUpdate);
      needRedraw = true;
      break;
    case UiActionId::ToggleGridAlert:
      settings.gridOfflineAlert = !settings.gridOfflineAlert;
      store.save(settings);
      needRedraw = true;
      break;
    case UiActionId::ToggleUseHostConfig:
      settings.useHostConfig = !settings.useHostConfig;
      store.save(settings);
      if (settings.useHostConfig) pollDisplayConfig(true);
      needRedraw = true;
      break;
    case UiActionId::OtaCheckNow: {
      otaStatus = "Checking...";
      needRedraw = true;
      refreshCurrentPage();
      flushUi();
      String st;
      gitOta.installLatest(st, false);
      otaStatus = st;
      needRedraw = true;
      break;
    }
    case UiActionId::PinDigit:
      if (pinEntry.length() < 4) {
        pinEntry += String(ctx.digit);
        if (page == UiPage::PinUnlock || page == UiPage::PinSet) ui.updatePinPad(pinEntry, pinStatus);
        if (pinEntry.length() == 4 && page == UiPage::PinUnlock) handlePinPadCommon(false);
      }
      break;
    case UiActionId::PinDelete:
      if (pinEntry.length()) pinEntry.remove(pinEntry.length() - 1);
      ui.updatePinPad(pinEntry, pinStatus);
      break;
    case UiActionId::PinOk:
      handlePinPadCommon(page == UiPage::PinSet);
      needRedraw = true;
      break;
    case UiActionId::PinBack:
      pinEntry = "";
      if (page == UiPage::PinSet || page == UiPage::PickLayout || page == UiPage::PickTheme)
        page = UiPage::Settings;
      else
        page = UiPage::Glance;
      needRedraw = true;
      break;
    default:
      break;
  }
}

static void startWifiScan() {
  wifiStatus = "Scanning...";
  needRedraw = true;
  ui.showWifiNetworks(wifiNets, wifiSelected, wifiStatus);
  flushUi();
  if (wifiScanNetworks(wifiNets)) {
    wifiSelected = 0;
    wifiStatus = "Tap your network";
  } else {
    wifiNets.clear();
    wifiStatus = "No networks — tap Rescan";
  }
  ui.showWifiNetworks(wifiNets, wifiSelected, wifiStatus);
  flushUi();
  needRedraw = false;
}

static bool connectPickedWifi(const String& ssid, const String& pass) {
  wifiStatus = "Connecting...";
  needRedraw = true;
  if (page == UiPage::WifiPassword) {
    ui.showWifiPassword(ssid, pass, wifiShowPass, wifiStatus);
  } else {
    ui.showWifiNetworks(wifiNets, wifiSelected, wifiStatus);
  }
  ui.tick();
  if (!wifiConnectAndSave(ssid.c_str(), pass.c_str())) {
    wifiStatus = "Failed — check password";
    return false;
  }
  wifiStatus = "Connected " + WiFi.localIP().toString();
  deviceWeb.applySettings(settings);
  deviceWeb.begin();
  ui.ensureClock();
  return true;
}

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
  page = UiPage::HostChoice;
  statusMsg = "";
  needRedraw = true;
}

static void openManualHost() {
  discovery.cancelSearch();
  loadManualOctetsFromSettings();
  manualSel = 0;
  page = UiPage::ManualHost;
  statusMsg = "Tap octet, use +/-";
  needRedraw = true;
}

static void startHostDiscovery() {
  found.clear();
  discovery.cancelSearch();
  discovery.beginSearch(settings.hostPort);
  page = UiPage::FindingHost;
  statusMsg = "Searching...";
  needRedraw = true;
}

static void finishHostDiscovery() {
  if (!found.empty()) {
    settings.hostIp = found[0].ip;
    settings.hostPort = found[0].port;
    store.save(settings);
    statusMsg = "Found " + settings.hostIp;
    page = UiPage::Glance;
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

static bool s_briPwmReady = false;

static void setBrightness(uint8_t v) {
  if (!s_briPwmReady) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(TFT_BL, 5000, 8);
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
#endif
    s_briPwmReady = true;
  }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(TFT_BL, v);
#else
  ledcWrite(0, v);
#endif
}

static bool minuteInNightWindow(uint16_t nowMin, uint16_t startMin, uint16_t endMin) {
  if (startMin == endMin) return false;
  if (startMin < endMin) return nowMin >= startMin && nowMin < endMin;
  return nowMin >= startMin || nowMin < endMin;
}

static uint8_t effectiveBrightness() {
  if (!settings.nightMode) return settings.brightness;
  struct tm ti;
  if (!getLocalTime(&ti, 0)) return settings.brightness;
  const uint16_t nowMin = (uint16_t)(ti.tm_hour * 60 + ti.tm_min);
  if (minuteInNightWindow(nowMin, settings.nightStartMin, settings.nightEndMin)) {
    return settings.nightBrightness;
  }
  return settings.brightness;
}

static void applyEffectiveBrightness() { setBrightness(effectiveBrightness()); }

static uint8_t cycleBrightnessStep(uint8_t cur, const uint8_t* steps, size_t n) {
  size_t i = 0;
  for (; i < n; i++) {
    if (cur == steps[i]) {
      return steps[(i + 1) % n];
    }
  }
  // Snap to nearest then advance
  size_t best = 0;
  int bestDiff = 256;
  for (size_t j = 0; j < n; j++) {
    const int d = abs((int)cur - (int)steps[j]);
    if (d < bestDiff) {
      bestDiff = d;
      best = j;
    }
  }
  return steps[(best + 1) % n];
}

static void applyScreenRotation() {
  settings.screenRotation = (uint8_t)constrain(settings.screenRotation, (int)0, (int)3);
  tft.setRotation(settings.screenRotation);
  touchInputSetRotation(settings.screenRotation);
  ui.setRotation(settings.screenRotation);
}

static String gPortalApName;

static void flushUi() {
  for (int i = 0; i < 4; i++) {
    ui.tick();
    delay(5);
  }
}

static void configModeCallback(WiFiManager* wm) {
  (void)wm;
  ui.showWifiPortal(gPortalApName.c_str());
  flushUi();
}

static void configureWifiManager(WiFiManager& wm, const String& hostname) {
  wm.setDebugOutput(false);
  wm.setConfigPortalTimeout(300);
  wm.setConnectTimeout(20);
  wm.setAPCallback(configModeCallback);
  wm.setTitle("Solar Display WiFi");
  wm.setHostname(hostname.c_str());
  wm.setCustomHeadElement(
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>"
      "body{font-family:system-ui,sans-serif;background:#111;color:#eee;padding:12px;max-width:420px;margin:auto}"
      "input,select{font-size:18px;padding:12px;width:100%;box-sizing:border-box;margin:8px 0;"
      "border-radius:8px;border:1px solid #444;background:#1c1c1e;color:#fff}"
      "button,.btn{font-size:18px;padding:12px 16px;border-radius:8px;border:0;background:#0a84ff;color:#fff;"
      "width:100%;margin-top:12px;cursor:pointer}"
      "h1{font-size:1.35rem} label{font-weight:600;display:block;margin-top:10px}"
      ".msg{padding:10px;border-radius:8px;background:#1c1c1e;margin:10px 0}"
      "</style>");
}

static bool waitStaConnected(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    lvglPortTick();
    delay(20);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool runPhoneConfigPortal() {
  String hostname = uniqueHostname();
  WiFiManager wm;
  configureWifiManager(wm, hostname);
  wm.setConfigPortalBlocking(false);

  ui.showWifiPortal(gPortalApName.c_str());
  flushUi();

  if (!wm.startConfigPortal(gPortalApName.c_str())) {
    return WiFi.status() == WL_CONNECTED;
  }

  uint32_t t0 = millis();
  while (wm.getConfigPortalActive() && millis() - t0 < 300000UL) {
    wm.process();
    ui.tick();
    if (WiFi.status() == WL_CONNECTED) break;
    delay(5);
  }
  wm.stopConfigPortal();
  return WiFi.status() == WL_CONNECTED;
}

static bool ensureWifi(bool forcePortal) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  gPortalApName = uniqueApName();
  String hostname = uniqueHostname();
  WiFi.setHostname(hostname.c_str());

  if (forcePortal) {
    return runPhoneConfigPortal();
  }

  // Boot: brief STA retry only. SoftAP is Phone fallback — fail fast so the
  // on-device WiFi list can open (README Option A).
  ui.setSplashMsg("WiFi...");
  flushUi();

  String seedSsid = WIFI_SSID;
  String seedPass = WIFI_PASSWORD;
  const bool seedOk = seedSsid.length() > 0 && seedSsid != "YourWiFiSSID";

  if (seedOk) {
    WiFi.begin(seedSsid.c_str(), seedPass.c_str());
    if (waitStaConnected(8000)) return true;
    return false;
  }

  wifi_config_t cfg = {};
  const bool hasSaved =
      esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0] != 0;
  if (hasSaved) {
    WiFi.begin();
    if (waitStaConnected(8000)) return true;
  }

  return false;
}

static bool pinIsSet() { return settings.settingsPin.length() == 4; }

static void openSettingsPage() {
  if (pinIsSet() && !settingsPinUnlocked) {
    page = UiPage::PinUnlock;
    pinEntry = "";
    pinStatus = "Enter PIN";
    pinReturnPage = UiPage::Settings;
    needRedraw = true;
    return;
  }
  page = UiPage::Settings;
  settingsTab = UiSettingsTab::Connection;
  settingsPinUnlocked = true;
  needRedraw = true;
  s_pollAfterUi = true;
}

static void handlePinPadCommon(bool forSet) {
  if (forSet) {
    if (pinEntry.length() == 0) {
      settings.settingsPin = "";
      store.save(settings);
      deviceWeb.applySettings(settings);
      pinSetPhase = 0;
      pinSetFirst = "";
      pinStatus = "PIN disabled";
      page = UiPage::Settings;
      needRedraw = true;
      return;
    }
    if (pinEntry.length() != 4) return;
    if (pinSetPhase == 0) {
      pinSetFirst = pinEntry;
      pinEntry = "";
      pinSetPhase = 1;
      pinStatus = "Confirm PIN";
      needRedraw = true;
      return;
    }
    if (pinEntry != pinSetFirst) {
      pinEntry = "";
      pinSetPhase = 0;
      pinSetFirst = "";
      pinStatus = "No match — retry";
      needRedraw = true;
      return;
    }
    settings.settingsPin = pinEntry;
    store.save(settings);
    deviceWeb.applySettings(settings);
    pinEntry = "";
    pinSetPhase = 0;
    pinSetFirst = "";
    pinStatus = "PIN saved";
    page = UiPage::Settings;
    needRedraw = true;
    return;
  }
  if (pinEntry.length() != 4) return;
  if (pinEntry == settings.settingsPin) {
    settingsPinUnlocked = true;
    pinEntry = "";
    page = pinReturnPage;
    pinStatus = "";
    needRedraw = true;
  } else {
    pinEntry = "";
    pinStatus = "Wrong PIN";
    needRedraw = true;
  }
}

static void applyHostSettingsFromConfig(const DisplayConfig& cfg) {
  bool changed = false;
  const bool revBump = cfg.config_rev > settings.configRev;
  if (revBump) {
    settings.configRev = cfg.config_rev;
    changed = true;
  }
  // Visual prefs only follow the host when it bumps config_rev.
  if (revBump) {
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
      if (s_lastLocalRotateMs != 0 && millis() - s_lastLocalRotateMs < 120000) {
        // keep local
      } else {
        settings.screenRotation = cfg.rotation;
        applyScreenRotation();
        changed = true;
      }
    }
    if (settings.brightness != cfg.brightness) {
      settings.brightness = cfg.brightness;
      applyEffectiveBrightness();
      changed = true;
    }
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
  if (settings.useHostConfig && cfg.settings_pin != settings.settingsPin) {
    settings.settingsPin = cfg.settings_pin;
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
    if (cfg.force_update_version.length()) {
      gitOta.setPendingTag(cfg.force_update_version);
    }
    gitOta.installLatest(st, true);
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

static void applyHostPollResults() {
  GlanceData g;
  if (hostPollTakeGlance(g)) {
    if (g.tz_offset_sec >= -43200 && g.tz_offset_sec <= 50400) {
      ui.syncClockTimezone(g.tz_offset_sec);
    }
    bool headerChanged = (g.clock != glance.clock) || (g.weather_enabled != glance.weather_enabled) ||
                         (g.weather_code != glance.weather_code) || (g.weather_temp != glance.weather_temp);
    if (!glanceVisualEqual(glance, g)) {
      glance = g;
      // Don't rebuild Settings/other pages on every power tick — kills tab taps.
      if (page == UiPage::Glance) needRedraw = true;
    } else {
      glance = g;
      if (page == UiPage::Glance && headerChanged) ui.refreshHeaderTime(glance);
    }
    lastGood = millis();
  }
  BmsData b;
  if (hostPollTakeBms(b)) {
    bms = b;
    lastGood = millis();
    if (page == UiPage::Bms) needRedraw = true;
  }
  HistoryData h;
  if (hostPollTakeHistory(h)) {
    history = h;
    lastGood = millis();
    if (page == UiPage::History) needRedraw = true;
  }
}

static void pollIfDue(bool force) {
  if (!force && millis() - lastPoll < settings.pollMs) return;
  if (!force && hostPollBusy()) return;
  lastPoll = millis();
  if (settings.hostIp.length() == 0 || WiFi.status() != WL_CONNECTED) return;
  // Non-blocking: FreeRTOS worker does HTTP; UI just applies results later.
  hostPollRequest(page, force);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  store.begin();
  settings = store.load();
  hostPollBegin(&api, &settings);

  themeSetActive(settings.themeId);
  touchInputBegin();

  tft.init();
  settings.screenRotation = (uint8_t)constrain(settings.screenRotation, (int)0, (int)3);
  tft.setRotation(settings.screenRotation);
  touchInputSetRotation(settings.screenRotation);
  applyEffectiveBrightness();
  ui.begin(tft, settings.screenRotation);
  ui.setTheme(settings.themeId);
  uiActionsSetHandler(handleUiAction);
  ui.showSplash("Starting...");
  ui.tick();

  gitOta.begin();
  gitOta.configure(settings.hostIp, settings.hostPort, settings.token, settings.checkForUpdate,
                   settings.autoInstallUpdate);

  if (ensureWifi(false)) {
    ui.setSplashMsg(WiFi.localIP().toString().c_str());
    ui.tick();
    deviceWeb.applySettings(settings);
    deviceWeb.begin();
    ui.ensureClock();
    {
      struct tm ti;
      uint32_t t0 = millis();
      while (millis() - t0 < 3000) {
        lvglPortTick();
        if (getLocalTime(&ti)) break;
        delay(20);
      }
    }
    delay(400);
  } else {
    page = UiPage::WifiPick;
    startWifiScan();
    delay(400);
  }

  if (page != UiPage::WifiPick && page != UiPage::WifiPassword) {
    page = UiPage::Glance;
    statusMsg = settings.hostIp;
  }

  pollDisplayConfig(true);
  pollIfDue(true);
  refreshCurrentPage();
  needRedraw = false;
}

void loop() {
  ui.tick();

  // Settings tab bar sits under the 24px header — don't treat horizontal drags there as page swipes.
  lvglPortSetSwipeExcludeTop(page == UiPage::Settings ? 64 : 0);

  static uint32_t s_wifiDownSince = 0;
  static uint32_t s_lastWifiReconnect = 0;
  if (WiFi.status() == WL_CONNECTED) {
    s_wifiDownSince = 0;
  } else if (page != UiPage::WifiPick && page != UiPage::WifiPassword) {
    if (s_wifiDownSince == 0) s_wifiDownSince = millis();
    if (millis() - s_wifiDownSince > 30000 && millis() - s_lastWifiReconnect > 15000) {
      s_lastWifiReconnect = millis();
      WiFi.reconnect();
    }
  }

  const int swipe = lvglPortConsumeSwipe();
  if (swipe != 0) {
    uiShellSwipePage(swipe);
  }

  if (gDeviceWebSaved) {
    gDeviceWebSaved = false;
    const uint8_t prevRot = settings.screenRotation;
    settings = deviceWeb.settings();
    store.save(settings);
    if (settings.screenRotation != prevRot) {
      s_lastLocalRotateMs = millis();
      applyScreenRotation();
    }
    ui.setTheme(settings.themeId);
    applyEffectiveBrightness();
    needRedraw = true;
  }

  deviceWeb.loop();
  gitOta.loop();
  otaStatus = gitOta.status();

  if (page == UiPage::FindingHost && discovery.isSearching()) {
    if (discovery.tickSearch(api, settings.token, found)) {
      finishHostDiscovery();
    }
  }

  static bool wasStale = false;
  const bool stale = (lastGood == 0) || (millis() - lastGood > STALE_MS);
  if (stale != wasStale) {
    wasStale = stale;
    needRedraw = true;
  }

  pollIfDue(false);
  pollDisplayConfig(false);

  static uint32_t lastClock = 0;
  if (page == UiPage::Glance && WiFi.status() == WL_CONNECTED && millis() - lastClock >= 30000) {
    lastClock = millis();
    ui.ensureClock();
    ui.refreshHeaderTime(glance);
  }

  static uint32_t lastBriApply = 0;
  static int lastBriMin = -1;
  if (millis() - lastBriApply >= 15000) {
    lastBriApply = millis();
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
      const int nowMin = ti.tm_hour * 60 + ti.tm_min;
      if (nowMin != lastBriMin) {
        lastBriMin = nowMin;
        applyEffectiveBrightness();
      }
    } else {
      applyEffectiveBrightness();
    }
  }

  if (page == UiPage::Glance && glanceGridAlert(glance) && millis() - lastAnim >= 5000) {
    lastAnim = millis();
    ui.pulseGlanceGrid(true, millis());
  }

  static uint32_t lastBattAnim = 0;
  if (page == UiPage::Glance && millis() - lastBattAnim >= 120) {
    lastBattAnim = millis();
    ui.animateGlanceBattery(glance, millis());
  }

  applyHostPollResults();

  if (needRedraw) {
    needRedraw = false;
    refreshCurrentPage();
    lvglPortResetInput();
  }
  // Kick HTTP only after the new page painted (worker task; never blocks UI).
  if (s_pollAfterUi) {
    s_pollAfterUi = false;
    pollIfDue(true);
  }

  delay(5);
}
