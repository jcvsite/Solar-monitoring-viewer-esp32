#include "ui_shell.h"
#include "ui_actions.h"
#include "ui_theme.h"
#include "ui_util.h"
#include "config.h"
#include "layout.h"
#include <stdio.h>
#include <time.h>

static bool s_clockStarted = false;
static int s_activeTzOffset = TIMEZONE_OFFSET_SEC;

void uiClockEnsure() {
  if (s_clockStarted) return;
  configTime(s_activeTzOffset, 0, "pool.ntp.org", "time.nist.gov");
  s_clockStarted = true;
}

void uiClockSyncTimezone(int offsetSec) {
  if (offsetSec < -43200 || offsetSec > 50400) return;
  if (offsetSec == s_activeTzOffset && s_clockStarted) return;
  s_activeTzOffset = offsetSec;
  configTime(s_activeTzOffset, 0, "pool.ntp.org", "time.nist.gov");
  s_clockStarted = true;
}

static String weatherSuffix(const GlanceData& g) {
  if (!g.weather_enabled || isnan(g.weather_temp)) return String("");
  char buf[24];
  snprintf(buf, sizeof(buf), "  %c%.0f%c", g.weather_is_day ? 0x00 : 0x00, g.weather_temp, g.weather_unit);
  (void)buf;
  char wbuf[20];
  snprintf(wbuf, sizeof(wbuf), " %.0f%c", g.weather_temp, g.weather_unit);
  return String(wbuf);
}

String uiClockHeaderRight(const GlanceData& g) {
  struct tm ti;
  String out;
  if (g.clock.length()) {
    out = g.clock;
  } else if (getLocalTime(&ti)) {
    char buf[16];
    strftime(buf, sizeof(buf), "%-I:%M %p", &ti);
    out = String(buf);
  } else {
    out = "--:--";
  }
  out += weatherSuffix(g);
  return out;
}

static void navClicked(lv_event_t* e) {
  const int idx = (int)(intptr_t)lv_event_get_user_data(e);
  UiActionCtx ctx;
  ctx.index = idx;
  if (idx == 3) {
    uiActionsFire(UiActionId::OpenSettings, ctx);
  } else {
    ctx.page = (UiPage)idx;
    uiActionsFire(UiActionId::NavPage, ctx);
  }
}

void uiShellCreate(UiShellWidgets& w, UiPage page, bool showNav) {
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = (lv_coord_t)scrW(0);
  const lv_coord_t sh = (lv_coord_t)scrH(0);

  w.root = lv_obj_create(NULL);
  lv_obj_remove_style_all(w.root);
  lv_obj_add_style(w.root, &uiStyleScreen, 0);
  lv_obj_set_size(w.root, sw, sh);
  lv_obj_clear_flag(w.root, LV_OBJ_FLAG_SCROLLABLE);

  w.header = lv_obj_create(w.root);
  lv_obj_remove_style_all(w.header);
  lv_obj_add_style(w.header, &uiStyleHeader, 0);
  lv_obj_set_size(w.header, sw, 24);
  lv_obj_align(w.header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(w.header, LV_OBJ_FLAG_SCROLLABLE);

  w.titleLbl = uiMakeLabel(w.header, "", uiFontBody(), uiColor565(t.text));
  lv_obj_align(w.titleLbl, LV_ALIGN_LEFT_MID, 0, 0);

  w.rightLbl = uiMakeLabel(w.header, "", uiFontBody(), uiColor565(t.muted));
  lv_obj_align(w.rightLbl, LV_ALIGN_RIGHT_MID, 0, 0);

  const lv_coord_t contentH = showNav ? (lv_coord_t)(navY(0) - 24) : (lv_coord_t)(sh - 24);
  w.content = lv_obj_create(w.root);
  lv_obj_remove_style_all(w.content);
  lv_obj_set_size(w.content, sw, contentH);
  lv_obj_align(w.content, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_set_style_bg_opa(w.content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(w.content, 4, 0);
  lv_obj_set_flex_flow(w.content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(w.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_add_flag(w.content, LV_OBJ_FLAG_SCROLLABLE);

  if (showNav) {
    w.nav = lv_obj_create(w.root);
    lv_obj_remove_style_all(w.nav);
    lv_obj_set_size(w.nav, sw, UI_NAV_H);
    lv_obj_align(w.nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(w.nav, uiColor565(t.card), 0);
    lv_obj_set_style_bg_opa(w.nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(w.nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(w.nav, uiColor565(t.line), 0);
    lv_obj_set_style_border_width(w.nav, 1, 0);
    lv_obj_set_flex_flow(w.nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w.nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(w.nav, 2, 0);
    lv_obj_clear_flag(w.nav, LV_OBJ_FLAG_SCROLLABLE);

    static const char* labels[] = {"Home", "BMS", "Hist", "Set"};
    for (int i = 0; i < 4; i++) {
      lv_obj_t* btn = lv_btn_create(w.nav);
      lv_obj_remove_style_all(btn);
      const bool on = (uint8_t)page == (uint8_t)i || (page == UiPage::Settings && i == 3);
      lv_obj_add_style(btn, on ? &uiStyleNavBtnActive : &uiStyleNavBtn, 0);
      lv_obj_set_size(btn, (sw - 16) / 4, UI_NAV_H - 6);
      lv_obj_add_event_cb(btn, navClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
      lv_obj_t* lbl = uiMakeLabel(btn, labels[i], uiFontBody(),
                                  on ? uiColor565(t.text) : uiColor565(t.dim));
      lv_obj_center(lbl);
      w.navBtns[i] = btn;
    }
  }

  lv_scr_load(w.root);
}

void uiShellDestroy(UiShellWidgets& w) {
  if (w.root) {
    lv_obj_del(w.root);
    w = UiShellWidgets();
  }
}

void uiShellSetPage(UiShellWidgets& w, UiPage page) {
  if (!w.nav) return;
  const ThemePalette& t = themeActive();
  for (int i = 0; i < 4; i++) {
    if (!w.navBtns[i]) continue;
    const bool on = (uint8_t)page == (uint8_t)i || (page == UiPage::Settings && i == 3);
    lv_obj_remove_style_all(w.navBtns[i]);
    lv_obj_add_style(w.navBtns[i], on ? &uiStyleNavBtnActive : &uiStyleNavBtn, 0);
    lv_obj_t* lbl = lv_obj_get_child(w.navBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, on ? uiColor565(t.text) : uiColor565(t.dim), 0);
  }
}

void uiShellSetHeader(UiShellWidgets& w, const String& left, const String& right) {
  uiSetLabelText(w.titleLbl, uiTruncate(left, 16));
  uiSetLabelText(w.rightLbl, right);
}

void uiShellClearContent(UiShellWidgets& w) {
  if (!w.content) return;
  lv_obj_clean(w.content);
}
