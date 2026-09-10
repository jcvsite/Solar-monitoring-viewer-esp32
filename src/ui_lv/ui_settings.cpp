#include "ui_settings.h"
#include "ui.h"
#include "ui_actions.h"
#include "ui_theme.h"
#include "ui_util.h"
#include "theme.h"
#include "layout.h"
#include "config.h"
#include <stdio.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static lv_coord_t panelW(const UiShellWidgets& shell) {
  const lv_coord_t w = lv_obj_get_width(shell.root);
  return w > 0 ? w - 8 : 300;
}

static lv_coord_t panelH(const UiShellWidgets& shell) {
  // Fill the shell content area exactly — extra subtraction left a black band above the nav.
  if (shell.content) {
    const lv_coord_t ch = lv_obj_get_content_height(shell.content);
    if (ch > 40) return ch;
    const lv_coord_t h = lv_obj_get_height(shell.content);
    if (h > 40) return h;
  }
  const lv_coord_t h = lv_obj_get_height(shell.root);
  return h > 0 ? h - 24 - UI_NAV_H : 160;
}

static bool settingsLandscape(const UiShellWidgets& shell) {
  const lv_coord_t w = lv_obj_get_width(shell.root);
  const lv_coord_t h = lv_obj_get_height(shell.root);
  return w > 0 && h > 0 && w > h;
}

static void settingsBtnClicked(lv_event_t* e) {
  uiActionsFire((UiActionId)(intptr_t)lv_event_get_user_data(e), UiActionCtx());
}

static void settingsTabChanged(lv_event_t* e) {
  lv_obj_t* tv = lv_event_get_target(e);
  UiActionCtx ctx;
  ctx.settingsTab =
      (lv_tabview_get_tab_act(tv) == 1) ? UiSettingsTab::Updates : UiSettingsTab::Connection;
  uiActionsFire(UiActionId::SettingsTab, ctx);
}

static void enablePanelScroll(lv_obj_t* panel) {
  lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_style_pad_bottom(panel, 8, 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_set_height(panel, LV_PCT(100));
  lv_obj_set_width(panel, LV_PCT(100));
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLL_CHAIN);
}

static void setBtnIconLabel(lv_obj_t* btn, const char* icon, lv_color_t iconColor, const char* text) {
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn, 6, 0);
  lv_obj_set_style_pad_left(btn, 8, 0);
  uiMakeLabel(btn, icon, uiFontBody(), iconColor);
  uiMakeLabel(btn, text, uiFontBody(), uiColor565(themeActive().text));
}

static lv_obj_t* addBtn(lv_obj_t* parent, const char* icon, lv_color_t iconColor, const char* label, UiActionId action,
                        lv_coord_t w, lv_coord_t h = 36) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &uiStyleCard, 0);
  lv_obj_set_size(btn, w, h);
  lv_obj_add_event_cb(btn, settingsBtnClicked, LV_EVENT_CLICKED, (void*)(intptr_t)action);
  setBtnIconLabel(btn, icon, iconColor, label);
  return btn;
}

static void addCardTitle(lv_obj_t* card, const char* icon, lv_color_t iconColor, const char* title) {
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 6, 0);
  uiMakeLabel(row, icon, uiFontTitle(), iconColor);
  uiMakeLabel(row, title, uiFontTitle(), uiColor565(themeActive().text));
}

static lv_obj_t* makeDotLabel(lv_obj_t* parent, lv_coord_t maxW, lv_color_t color) {
  lv_obj_t* lbl = uiMakeLabel(parent, "", uiFontBody(), color);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl, maxW);
  return lbl;
}

static void buildConnPanel(lv_obj_t* panel, const UiShellWidgets& shell, const HostSettings& cfg, bool wifiOk,
                           const String& wifiSsid, const String& statusMsg) {
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = panelW(shell);
  const lv_coord_t rowH = 36;
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, settingsLandscape(shell) ? 4 : 6, 0);
  lv_obj_set_style_pad_all(panel, 2, 0);
  enablePanelScroll(panel);

  lv_obj_t* card = uiMakeCard(panel, sw, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(card, 6, 0);
  lv_obj_set_style_pad_row(card, 2, 0);
  addCardTitle(card, LV_SYMBOL_WIFI, uiColor565(t.grid), "HOST");
  {
    const lv_coord_t lw = sw > 24 ? sw - 24 : sw;
    uiSetLabelText(makeDotLabel(card, lw, uiColor565(t.text)),
                   cfg.hostIp.length() ? (cfg.hostIp + ":" + String(cfg.hostPort)) : "Not configured");
    uiSetLabelText(makeDotLabel(card, lw, uiColor565(t.muted)),
                   wifiOk ? ("WiFi: " + wifiSsid) : "WiFi: offline");
    if (statusMsg.length()) {
      uiSetLabelText(makeDotLabel(card, lw, uiColor565(t.warn)), statusMsg);
    }
  }

  char rotBuf[48];
  snprintf(rotBuf, sizeof(rotBuf), "Rotate: %s", rotationLabel(cfg.screenRotation));
  auto chev = [&](const char* icon, lv_color_t ic, const char* label, UiActionId action) {
    lv_obj_t* btn = addBtn(panel, icon, ic, label, action, sw, rowH);
    lv_obj_t* c = uiMakeLabel(btn, LV_SYMBOL_RIGHT, uiFontBody(), uiColor565(themeActive().muted));
    lv_obj_align(c, LV_ALIGN_RIGHT_MID, -6, 0);
    return btn;
  };
  chev(LV_SYMBOL_REFRESH, uiColor565(t.pv), rotBuf, UiActionId::RotateScreen);
  {
    char layBuf[40];
    snprintf(layBuf, sizeof(layBuf), "Layout: %s", layoutName(cfg.glanceLayout));
    chev(LV_SYMBOL_LIST, uiColor565(t.charge), layBuf, UiActionId::OpenPickLayout);
  }
  {
    char thBuf[40];
    snprintf(thBuf, sizeof(thBuf), "Theme: %s", themeName(cfg.themeId));
    chev(LV_SYMBOL_TINT, uiColor565(t.grid), thBuf, UiActionId::OpenPickTheme);
  }
  {
    char briBuf[32];
    snprintf(briBuf, sizeof(briBuf), "Brightness: %u", (unsigned)cfg.brightness);
    chev(LV_SYMBOL_CHARGE, uiColor565(t.pv), briBuf, UiActionId::CycleBrightness);
  }
  addBtn(panel, LV_SYMBOL_MUTE, uiColor565(t.warn), cfg.nightMode ? "Night mode: ON" : "Night mode: OFF",
         UiActionId::ToggleNightMode, sw, rowH);
  if (cfg.nightMode) {
    char nbBuf[32];
    snprintf(nbBuf, sizeof(nbBuf), "Night bri: %u", (unsigned)cfg.nightBrightness);
    chev(LV_SYMBOL_CHARGE, uiColor565(t.dim), nbBuf, UiActionId::CycleNightBrightness);
    char nsBuf[32];
    snprintf(nsBuf, sizeof(nsBuf), "Night start: %02u:00", (unsigned)(cfg.nightStartMin / 60));
    chev(LV_SYMBOL_GPS, uiColor565(t.grid), nsBuf, UiActionId::CycleNightStart);
    char neBuf[32];
    snprintf(neBuf, sizeof(neBuf), "Night end: %02u:00", (unsigned)(cfg.nightEndMin / 60));
    chev(LV_SYMBOL_GPS, uiColor565(t.grid), neBuf, UiActionId::CycleNightEnd);
  }
  chev(LV_SYMBOL_EDIT, uiColor565(t.warn), "Settings PIN", UiActionId::OpenPinSet);

  lv_obj_t* row = lv_obj_create(panel);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, sw, rowH);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  addBtn(row, LV_SYMBOL_GPS, uiColor565(t.ok), "Find", UiActionId::StartHostDiscovery, sw / 2 - 3, rowH);
  addBtn(row, LV_SYMBOL_KEYBOARD, uiColor565(t.grid), "Manual", UiActionId::OpenManualHost, sw / 2 - 3, rowH);
  addBtn(panel, LV_SYMBOL_WIFI, uiColor565(t.gridImport), "WiFi Setup", UiActionId::StartWifiScan, sw, rowH);
}

static void buildUpdPanel(lv_obj_t* panel, UiSettingsWidgets& s, const UiShellWidgets& shell, const HostSettings& cfg,
                          const String& otaStatus, const char* fwVersion) {
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = panelW(shell);
  const lv_coord_t rowH = 36;
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, settingsLandscape(shell) ? 4 : 6, 0);
  lv_obj_set_style_pad_all(panel, 2, 0);
  enablePanelScroll(panel);

  lv_obj_t* card = uiMakeCard(panel, sw, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(card, 6, 0);
  lv_obj_set_style_pad_row(card, 2, 0);
  addCardTitle(card, LV_SYMBOL_DOWNLOAD, uiColor565(t.charge), "Firmware");
  uiMakeLabel(card, (String("Version ") + fwVersion).c_str(), uiFontBody(), uiColor565(t.text));
  s.otaStatusLbl = uiMakeLabel(card, otaStatus.c_str(), uiFontBody(), uiColor565(t.muted));

  addBtn(panel, LV_SYMBOL_REFRESH, uiColor565(t.pv), cfg.checkForUpdate ? "Check updates: ON" : "Check updates: OFF",
         UiActionId::ToggleCheckUpdate, sw, rowH);
  addBtn(panel, LV_SYMBOL_LOOP, uiColor565(t.charge),
         cfg.autoInstallUpdate ? "Auto-install: ON" : "Auto-install: OFF", UiActionId::ToggleAutoUpdate, sw, rowH);
  addBtn(panel, LV_SYMBOL_WARNING, uiColor565(t.danger),
         cfg.gridOfflineAlert ? "Grid alert: ON" : "Grid alert: OFF", UiActionId::ToggleGridAlert, sw, rowH);
  addBtn(panel, LV_SYMBOL_SHUFFLE, uiColor565(t.grid),
         cfg.useHostConfig ? "Sync from host: ON" : "Sync from host: OFF", UiActionId::ToggleUseHostConfig, sw, rowH);
  addBtn(panel, LV_SYMBOL_DOWNLOAD, uiColor565(t.gridImport), "Check now", UiActionId::OtaCheckNow, sw, rowH);
}

void uiSettingsBuild(UiShellWidgets& shell, UiSettingsWidgets& s, UiSettingsTab tab, const HostSettings& cfg,
                     bool wifiOk, const String& wifiSsid, const String& statusMsg, const String& otaStatus,
                     const char* fwVersion) {
  uiShellClearContent(shell);
  s = UiSettingsWidgets();
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = panelW(shell);
  const lv_coord_t sh = panelH(shell);

  lv_obj_set_style_pad_all(shell.content, 0, 0);
  lv_obj_set_style_pad_row(shell.content, 0, 0);
  lv_obj_clear_flag(shell.content, LV_OBJ_FLAG_SCROLLABLE);

  const lv_coord_t tabH = settingsLandscape(shell) ? 36 : 40;
  s.tabs = lv_tabview_create(shell.content, LV_DIR_TOP, tabH);
  lv_obj_set_size(s.tabs, sw, sh);
  lv_obj_set_style_bg_color(s.tabs, uiColor565(t.bg), 0);
  lv_obj_set_style_bg_opa(s.tabs, LV_OPA_COVER, 0);
  lv_obj_t* tabCont = lv_tabview_get_content(s.tabs);
  if (tabCont) {
    lv_obj_set_style_bg_color(tabCont, uiColor565(t.bg), 0);
    lv_obj_set_style_bg_opa(tabCont, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tabCont, LV_OBJ_FLAG_SCROLLABLE);
  }
  s.connPanel = lv_tabview_add_tab(s.tabs, LV_SYMBOL_WIFI " Conn");
  s.updPanel = lv_tabview_add_tab(s.tabs, LV_SYMBOL_DOWNLOAD " Upd");
  lv_obj_t* tabBtns = lv_tabview_get_tab_btns(s.tabs);
  if (tabBtns) {
    lv_obj_set_style_bg_color(tabBtns, uiColor565(t.bg), 0);
    lv_obj_set_style_bg_opa(tabBtns, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_top(tabBtns, 4, 0);
    lv_obj_set_style_pad_bottom(tabBtns, 4, 0);
    lv_obj_set_style_text_color(tabBtns, uiColor565(t.grid), LV_PART_ITEMS);
    lv_obj_set_style_text_color(tabBtns, uiColor565(t.charge), LV_PART_ITEMS | LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(s.tabs, settingsTabChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  buildConnPanel(s.connPanel, shell, cfg, wifiOk, wifiSsid, statusMsg);
  buildUpdPanel(s.updPanel, s, shell, cfg, otaStatus, fwVersion);
  uiSettingsSetTab(s, tab);
}

void uiSettingsSetTab(UiSettingsWidgets& s, UiSettingsTab tab) {
  if (!s.tabs) return;
  lv_tabview_set_act(s.tabs, tab == UiSettingsTab::Updates ? 1 : 0, LV_ANIM_OFF);
}

void uiSettingsUpdateOtaStatus(UiSettingsWidgets& s, const String& otaStatus) {
  if (!s.otaStatusLbl) return;
  uiSetLabelText(s.otaStatusLbl, otaStatus.c_str());
}
