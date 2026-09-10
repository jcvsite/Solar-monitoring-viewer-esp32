#include "ui_wifi.h"
#include "ui_actions.h"
#include "ui_logo.h"
#include "ui_splash.h"
#include "ui_theme.h"
#include "ui_util.h"
#include <stdio.h>
#include <string.h>

static void wifiAction(lv_event_t* e) {
  uiActionsFire((UiActionId)(intptr_t)lv_event_get_user_data(e), UiActionCtx());
}

static void wifiNetClicked(lv_event_t* e) {
  UiActionCtx ctx;
  ctx.index = (int)(intptr_t)lv_event_get_user_data(e);
  uiActionsFire(UiActionId::WifiPickNetwork, ctx);
}

/** Load neu first, then delete previous/old screens (never delete lv_scr_act first). */
static void wifiSwapScreen(UiWifiWidgets& w, lv_obj_t* neu) {
  lv_obj_t* oldScreen = w.screen;
  lv_obj_t* oldPass = w.passScreen;
  w.screen = nullptr;
  w.passScreen = nullptr;
  w.list = nullptr;
  w.statusLbl = nullptr;
  w.ta = nullptr;
  w.kb = nullptr;

  lv_obj_t* prev = lv_scr_act();
  lv_scr_load(neu);
  if (prev && prev != neu) {
    if (uiSplashOwns(prev)) uiSplashAbandon();
    lv_obj_del(prev);
  }
  if (oldScreen && oldScreen != prev && oldScreen != neu) lv_obj_del(oldScreen);
  if (oldPass && oldPass != prev && oldPass != neu) lv_obj_del(oldPass);
}

void uiWifiDestroy(UiWifiWidgets& w) {
  lv_obj_t* screen = w.screen;
  lv_obj_t* pass = w.passScreen;
  w = UiWifiWidgets();
  auto safeDel = [](lv_obj_t* obj) {
    if (!obj) return;
    if (lv_scr_act() == obj) {
      lv_obj_t* blank = lv_obj_create(NULL);
      lv_obj_remove_style_all(blank);
      lv_obj_set_style_bg_color(blank, lv_color_black(), 0);
      lv_obj_set_style_bg_opa(blank, LV_OPA_COVER, 0);
      lv_obj_set_size(blank, LV_HOR_RES, LV_VER_RES);
      lv_scr_load(blank);
      lv_obj_del(obj);
    } else {
      lv_obj_del(obj);
    }
  };
  safeDel(screen);
  safeDel(pass);
}

void uiWifiBuildPortal(UiWifiWidgets& w, const char* apName) {
  uiLogoEnsureLoaded();
  const ThemePalette& t = themeActive();
  lv_obj_t* neu = lv_obj_create(NULL);
  lv_obj_remove_style_all(neu);
  lv_obj_add_style(neu, &uiStyleScreen, 0);
  lv_obj_set_size(neu, LV_HOR_RES, LV_VER_RES);

  const lv_img_dsc_t* logo = uiLogoDescriptor();
  lv_obj_t* img = nullptr;
  if (logo) {
    img = lv_img_create(neu);
    lv_img_set_src(img, logo);
    lv_img_set_zoom(img, uiLogoSplashZoom());
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 8);
  }

  lv_obj_t* title = uiMakeLabel(neu, "Phone WiFi Setup", uiFontTitle(), uiColor565(t.text));
  if (img) lv_obj_align_to(title, img, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
  else lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_t* apLbl = uiMakeLabel(neu, apName, uiFontBody(), uiColor565(t.pv));
  lv_obj_align_to(apLbl, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
  lv_obj_t* hint = uiMakeLabel(neu, "Join AP, open 192.168.4.1", uiFontBody(), uiColor565(t.muted));
  lv_obj_align_to(hint, apLbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

  wifiSwapScreen(w, neu);
  w.screen = neu;
}

void uiWifiBuildList(UiWifiWidgets& w, const std::vector<WifiNetwork>& nets, int selected, const String& status) {
  const ThemePalette& t = themeActive();
  lv_obj_t* neu = lv_obj_create(NULL);
  lv_obj_remove_style_all(neu);
  lv_obj_add_style(neu, &uiStyleScreen, 0);
  lv_obj_set_size(neu, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_flex_flow(neu, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(neu, 6, 0);

  lv_obj_t* statusLbl = uiMakeLabel(neu, status.c_str(), uiFontBody(), uiColor565(t.muted));
  lv_obj_t* list = lv_list_create(neu);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);

  for (size_t i = 0; i < nets.size(); i++) {
    char ssidShort[24];
    const String& ssid = nets[i].ssid;
    if (ssid.length() > 18) {
      snprintf(ssidShort, sizeof(ssidShort), "%.15s...", ssid.c_str());
    } else {
      snprintf(ssidShort, sizeof(ssidShort), "%s", ssid.c_str());
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %ddB", ssidShort, nets[i].secure ? "[sec]" : "", nets[i].rssi);
    lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
    if ((int)i == selected) lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn, wifiNetClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }

  lv_obj_t* row = lv_obj_create(neu);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), 36);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* b1 = lv_btn_create(row);
  lv_obj_add_event_cb(b1, wifiAction, LV_EVENT_CLICKED, (void*)(intptr_t)UiActionId::WifiRescan);
  uiMakeLabel(b1, "Rescan", uiFontBody(), uiColor565(t.text));
  lv_obj_t* b2 = lv_btn_create(row);
  lv_obj_add_event_cb(b2, wifiAction, LV_EVENT_CLICKED, (void*)(intptr_t)UiActionId::WifiPhonePortal);
  uiMakeLabel(b2, "Phone", uiFontBody(), uiColor565(t.text));
  lv_obj_t* b3 = lv_btn_create(row);
  lv_obj_add_event_cb(b3, wifiAction, LV_EVENT_CLICKED, (void*)(intptr_t)UiActionId::WifiBack);
  uiMakeLabel(b3, "Back", uiFontBody(), uiColor565(t.text));

  wifiSwapScreen(w, neu);
  w.screen = neu;
  w.statusLbl = statusLbl;
  w.list = list;
}

static void passAction(lv_event_t* e) {
  uiActionsFire((UiActionId)(intptr_t)lv_event_get_user_data(e), UiActionCtx());
}

static void wifiKbReady(lv_event_t* e) {
  (void)e;
  uiActionsFire(UiActionId::WifiConnect, UiActionCtx());
}

static void wifiKbCancel(lv_event_t* e) {
  (void)e;
  uiActionsFire(UiActionId::WifiBack, UiActionCtx());
}

// Phone-style maps: letter keys show the case you type; ABC/abc is a wide shift key.
#define WIFI_KB_BTN(w) ((lv_btnmatrix_ctrl_t)(LV_BTNMATRIX_CTRL_POPOVER | (w)))

static const char* kWifiKbLc[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", "@", ".", "\n",
    "1#", "space", LV_SYMBOL_OK, ""};

static const lv_btnmatrix_ctrl_t kWifiKbLcCtrl[] = {
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    (lv_btnmatrix_ctrl_t)(LV_BTNMATRIX_CTRL_CHECKED | 6),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 7, WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 5, 12, LV_KEYBOARD_CTRL_BTN_FLAGS | 6,
};

static const char* kWifiKbUc[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", "@", ".", "\n",
    "1#", "space", LV_SYMBOL_OK, ""};

static const lv_btnmatrix_ctrl_t kWifiKbUcCtrl[] = {
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    (lv_btnmatrix_ctrl_t)(LV_BTNMATRIX_CTRL_CHECKED | 6),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 7, WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 5, 12, LV_KEYBOARD_CTRL_BTN_FLAGS | 6,
};

static const char* kWifiKbSpec[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "/", ":", ";", "(", ")", "$", "&", "_", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "!", "?", "#", "%", "+", "=", "*", "\"", "'", "\n",
    "ABC", "space", LV_SYMBOL_OK, ""};

static const lv_btnmatrix_ctrl_t kWifiKbSpecCtrl[] = {
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    (lv_btnmatrix_ctrl_t)(LV_BTNMATRIX_CTRL_CHECKED | 6),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 7, WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4), WIFI_KB_BTN(4),
    LV_KEYBOARD_CTRL_BTN_FLAGS | 5, 12, LV_KEYBOARD_CTRL_BTN_FLAGS | 6,
};

static void wifiKbEvent(lv_event_t* e) {
  lv_obj_t* kb = lv_event_get_target(e);
  uint16_t btn = lv_btnmatrix_get_selected_btn(kb);
  if (btn == LV_BTNMATRIX_BTN_NONE) return;
  const char* txt = lv_btnmatrix_get_btn_text(kb, btn);
  if (txt && strcmp(txt, "space") == 0) {
    lv_obj_t* ta = lv_keyboard_get_textarea(kb);
    if (ta) lv_textarea_add_text(ta, " ");
    return;
  }
  lv_keyboard_def_event_cb(e);
}

void uiWifiBuildPassword(UiWifiWidgets& w, const String& ssid, const String& password, bool showPass,
                         const String& status) {
  const ThemePalette& t = themeActive();
  lv_obj_t* neu = lv_obj_create(NULL);
  lv_obj_remove_style_all(neu);
  lv_obj_add_style(neu, &uiStyleScreen, 0);
  lv_obj_set_size(neu, LV_HOR_RES, LV_VER_RES);
  lv_obj_clear_flag(neu, LV_OBJ_FLAG_SCROLLABLE);

  // Top bar: Back + SSID
  lv_obj_t* top = lv_obj_create(neu);
  lv_obj_remove_style_all(top);
  lv_obj_set_size(top, LV_PCT(100), 28);
  lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 2);
  lv_obj_set_style_pad_hor(top, 4, 0);
  lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* back = lv_btn_create(top);
  lv_obj_set_size(back, 56, 26);
  lv_obj_add_event_cb(back, passAction, LV_EVENT_CLICKED, (void*)(intptr_t)UiActionId::WifiBack);
  uiMakeLabel(back, LV_SYMBOL_LEFT " Back", uiFontBody(), uiColor565(t.text));

  lv_obj_t* ssidLbl = uiMakeLabel(top, ssid.c_str(), uiFontTitle(), uiColor565(t.text));
  lv_obj_set_flex_grow(ssidLbl, 1);
  lv_label_set_long_mode(ssidLbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ssidLbl, LV_PCT(100));

  // Password field + Show — common mobile pattern
  lv_obj_t* passRow = lv_obj_create(neu);
  lv_obj_remove_style_all(passRow);
  lv_obj_set_size(passRow, LV_PCT(100), 34);
  lv_obj_align(passRow, LV_ALIGN_TOP_MID, 0, 32);
  lv_obj_set_style_pad_hor(passRow, 4, 0);
  lv_obj_set_flex_flow(passRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(passRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(passRow, 4, 0);

  lv_obj_t* ta = lv_textarea_create(passRow);
  lv_obj_set_height(ta, 32);
  lv_obj_set_flex_grow(ta, 1);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, !showPass);
  lv_textarea_set_placeholder_text(ta, "Password");
  lv_textarea_set_text(ta, password.c_str());
  lv_obj_set_style_pad_all(ta, 6, 0);
  lv_obj_set_style_text_font(ta, uiFontBody(), 0);

  lv_obj_t* showBtn = lv_btn_create(passRow);
  lv_obj_set_size(showBtn, 52, 30);
  lv_obj_add_event_cb(showBtn, passAction, LV_EVENT_CLICKED, (void*)(intptr_t)UiActionId::WifiToggleShowPass);
  lv_obj_t* showLbl =
      uiMakeLabel(showBtn, showPass ? "Hide" : "Show", uiFontBody(), uiColor565(t.text));

  lv_obj_t* hint = uiMakeLabel(neu, "ABC = uppercase   ·   1# = symbols", uiFontBody(), uiColor565(t.muted));
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 68);

  lv_obj_t* statusLbl = uiMakeLabel(neu, status.c_str(), uiFontBody(), uiColor565(t.warn));
  lv_obj_align(statusLbl, LV_ALIGN_TOP_MID, 0, 86);

  lv_obj_t* kb = lv_keyboard_create(neu);
  lv_obj_set_size(kb, LV_PCT(100), LV_VER_RES - 108);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_pad_all(kb, 3, 0);
  lv_obj_set_style_pad_row(kb, 3, 0);
  lv_obj_set_style_pad_column(kb, 2, 0);
  lv_obj_set_style_text_font(kb, uiFontBody(), 0);
  // Mode keys (ABC / abc / 1#) use CHECKED — read as Shift-like controls.
  lv_obj_set_style_bg_color(kb, uiColor565(t.card), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, uiColor565(t.grid), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(kb, uiColor565(t.onAccent), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(kb, uiColor565(t.pv), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);

  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, kWifiKbLc, kWifiKbLcCtrl);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, kWifiKbUc, kWifiKbUcCtrl);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, kWifiKbSpec, kWifiKbSpecCtrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_popovers(kb, true);
  lv_keyboard_set_textarea(kb, ta);

  // Replace default handler so "space" inserts a real space, not the word.
  lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
  lv_obj_add_event_cb(kb, wifiKbEvent, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(kb, wifiKbReady, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(kb, wifiKbCancel, LV_EVENT_CANCEL, NULL);

  wifiSwapScreen(w, neu);
  w.passScreen = neu;
  w.ta = ta;
  w.kb = kb;
  w.showBtnLbl = showLbl;
  w.statusLbl = statusLbl;
}
