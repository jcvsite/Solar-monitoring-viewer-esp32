#include "ui_settings.h"
#include "ui.h"
#include "ui_actions.h"
#include "ui_theme.h"
#include "ui_util.h"
#include "theme.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static const char* kLayoutNames[] = {"Classic", "Compact", "Ring", "Bars", "Flow"};

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

static void settingsBtnClicked(lv_event_t* e) {
  uiActionsFire((UiActionId)(intptr_t)lv_event_get_user_data(e), UiActionCtx());
}

static lv_obj_t* addBtn(lv_obj_t* parent, const char* label, UiActionId action, lv_coord_t w) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &uiStyleCard, 0);
  lv_obj_set_size(btn, w, 28);
  lv_obj_add_event_cb(btn, settingsBtnClicked, LV_EVENT_CLICKED, (void*)(intptr_t)action);
  lv_obj_t* lbl = uiMakeLabel(btn, label, uiFontBody(), uiColor565(themeActive().text));
  lv_obj_center(lbl);
  return btn;
}

static void buildConnPanel(lv_obj_t* panel, const HostSettings& cfg, bool wifiOk, const String& wifiSsid,
                           const String& statusMsg) {
  const lv_coord_t sw = lv_obj_get_width(panel);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 6, 0);

  lv_obj_t* card = uiMakeCard(panel, sw, 72);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  uiMakeLabel(card, "Connection", uiFontTitle(), uiColor565(themeActive().text));
  uiSetLabelText(uiMakeLabel(card, "", uiFontBody(), uiColor565(themeActive().muted)),
                 wifiOk ? ("WiFi: " + wifiSsid) : "WiFi: offline");
  uiSetLabelText(
      uiMakeLabel(card, "", uiFontBody(), uiColor565(themeActive().text)),
      "Host: " + (cfg.hostIp.length() ? cfg.hostIp + ":" + String(cfg.hostPort) : "not set"));
  if (statusMsg.length()) {
    uiSetLabelText(uiMakeLabel(card, "", uiFontBody(), uiColor565(themeActive().warn)), statusMsg);
  }

  char rotBuf[48];
  snprintf(rotBuf, sizeof(rotBuf), "Rotate: %s", rotationLabel(cfg.screenRotation));
  addBtn(panel, rotBuf, UiActionId::RotateScreen, sw);
  addBtn(panel, (String("Layout: ") + kLayoutNames[cfg.glanceLayout]).c_str(), UiActionId::PickLayout, sw);
  addBtn(panel, (String("Theme: ") + themeName(cfg.themeId)).c_str(), UiActionId::PickTheme, sw);
  addBtn(panel, "Settings PIN", UiActionId::OpenPinSet, sw);

  lv_obj_t* row = lv_obj_create(panel);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, sw, 32);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  addBtn(row, "Find host", UiActionId::StartHostDiscovery, sw / 2 - 4);
  addBtn(row, "Manual IP", UiActionId::OpenManualHost, sw / 2 - 4);
  addBtn(panel, "WiFi networks", UiActionId::StartWifiScan, sw);
}

static void buildUpdPanel(lv_obj_t* panel, const HostSettings& cfg, const String& otaStatus, const char* fwVersion) {
  const lv_coord_t sw = lv_obj_get_width(panel);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, 6, 0);
  lv_obj_t* card = uiMakeCard(panel, sw, 90);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  uiMakeLabel(card, "Firmware", uiFontTitle(), uiColor565(themeActive().text));
  uiMakeLabel(card, (String("Version ") + fwVersion).c_str(), uiFontBody(), uiColor565(themeActive().text));
  uiMakeLabel(card, otaStatus.c_str(), uiFontBody(), uiColor565(themeActive().muted));

  addBtn(panel, cfg.checkForUpdate ? "[x] Check for updates" : "[ ] Check for updates", UiActionId::ToggleCheckUpdate,
         sw);
  addBtn(panel, cfg.autoInstallUpdate ? "[x] Auto-install" : "[ ] Auto-install", UiActionId::ToggleAutoUpdate, sw);
  addBtn(panel, cfg.gridOfflineAlert ? "[x] Grid offline alert" : "[ ] Grid offline alert", UiActionId::ToggleGridAlert,
         sw);
  addBtn(panel, cfg.useHostConfig ? "[x] Sync from host" : "[ ] Sync from host", UiActionId::ToggleUseHostConfig, sw);
  addBtn(panel, "Check now", UiActionId::OtaCheckNow, sw);
}

void uiSettingsBuild(UiShellWidgets& shell, UiSettingsWidgets& s, UiSettingsTab tab, const HostSettings& cfg,
                     bool wifiOk, const String& wifiSsid, const String& statusMsg, const String& otaStatus,
                     const char* fwVersion) {
  uiShellClearContent(shell);
  s = UiSettingsWidgets();
  const lv_coord_t sw = lv_obj_get_width(shell.content);

  s.tabs = lv_tabview_create(shell.content, LV_DIR_TOP, 28);
  lv_obj_set_size(s.tabs, sw, lv_obj_get_height(shell.content));
  s.connPanel = lv_tabview_add_tab(s.tabs, "Conn");
  s.updPanel = lv_tabview_add_tab(s.tabs, "Updates");
  buildConnPanel(s.connPanel, cfg, wifiOk, wifiSsid, statusMsg);
  buildUpdPanel(s.updPanel, cfg, otaStatus, fwVersion);
  uiSettingsSetTab(s, tab);
}

void uiSettingsSetTab(UiSettingsWidgets& s, UiSettingsTab tab) {
  if (!s.tabs) return;
  lv_tabview_set_act(s.tabs, tab == UiSettingsTab::Updates ? 1 : 0, LV_ANIM_OFF);
}
