#include "ui_glance.h"
#include "ui_theme.h"
#include "ui_util.h"
#include "layout.h"
#include <stdio.h>

static lv_obj_t* makeMetricCard(lv_obj_t* parent, const char* title, lv_obj_t** valOut, lv_color_t accent) {
  const lv_coord_t sw = lv_obj_get_width(parent);
  lv_obj_t* card = uiMakeCard(parent, (sw - 12) / 2, 52);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  uiMakeLabel(card, title, uiFontBody(), uiColor565(themeActive().muted));
  *valOut = uiMakeLabel(card, "--", uiFontTitle(), accent);
  return card;
}

void uiGlanceDestroy(UiGlanceWidgets& g) { g = UiGlanceWidgets(); }

void uiGlanceBuild(UiShellWidgets& shell, UiGlanceWidgets& g, uint8_t layoutId) {
  uiShellClearContent(shell);
  g = UiGlanceWidgets();
  const ThemePalette& t = themeActive();
  lv_obj_t* content = shell.content;
  const lv_coord_t sw = lv_obj_get_width(content);

  g.gridPill = lv_obj_create(content);
  lv_obj_remove_style_all(g.gridPill);
  lv_obj_set_size(g.gridPill, sw - 8, 22);
  lv_obj_set_style_radius(g.gridPill, 11, 0);
  lv_obj_set_style_bg_color(g.gridPill, uiColor565(t.ok), 0);
  lv_obj_set_style_bg_opa(g.gridPill, LV_OPA_COVER, 0);
  lv_obj_t* gridLbl = uiMakeLabel(g.gridPill, "GRID OK", uiFontBody(), uiColor565(t.onAccent));
  lv_obj_center(gridLbl);

  if (layoutId == 2) {
    g.arc = lv_arc_create(content);
    lv_obj_set_size(g.arc, 120, 120);
    lv_arc_set_rotation(g.arc, 135);
    lv_arc_set_bg_angles(g.arc, 0, 270);
    lv_arc_set_range(g.arc, 0, 100);
    lv_obj_set_style_arc_width(g.arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g.arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g.arc, uiColor565(t.card), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g.arc, uiSocColor(50), LV_PART_INDICATOR);
    g.socLbl = uiMakeLabel(g.arc, "0%", uiFontDisplay(), uiColor565(t.text));
    lv_obj_center(g.socLbl);
  } else if (layoutId == 3) {
    const char* labels[] = {"PV", "Load", "Grid"};
    for (int i = 0; i < 3; i++) {
      lv_obj_t* row = lv_obj_create(content);
      lv_obj_remove_style_all(row);
      lv_obj_set_size(row, sw - 8, 28);
      lv_obj_set_style_pad_bottom(row, 2, 0);
      uiMakeLabel(row, labels[i], uiFontBody(), uiColor565(t.muted));
      g.bars[i] = lv_bar_create(row);
      lv_obj_set_size(g.bars[i], sw - 20, 8);
      lv_obj_align(g.bars[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
      lv_bar_set_range(g.bars[i], 0, 100);
      lv_obj_set_style_bg_color(g.bars[i], uiColor565(t.card), LV_PART_MAIN);
    }
    g.socLbl = uiMakeLabel(content, "SOC 0%", uiFontTitle(), uiColor565(t.text));
  } else if (layoutId == 4) {
    g.flowCol = lv_obj_create(content);
    lv_obj_remove_style_all(g.flowCol);
    lv_obj_set_size(g.flowCol, sw - 8, 120);
    lv_obj_set_flex_flow(g.flowCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g.flowCol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    g.pvVal = uiMakeLabel(g.flowCol, "PV --", uiFontBody(), uiColor565(t.pv));
    g.loadVal = uiMakeLabel(g.flowCol, "Home --", uiFontBody(), uiColor565(t.text));
    g.gridVal = uiMakeLabel(g.flowCol, "Grid --", uiFontBody(), uiColor565(t.grid));
    g.socLbl = uiMakeLabel(g.flowCol, "0% SOC", uiFontTitle(), uiColor565(t.text));
  } else if (layoutId == 1) {
    g.socLbl = uiMakeLabel(content, "0%", uiFontDisplay(), uiColor565(t.text));
    lv_obj_set_width(g.socLbl, sw - 8);
    lv_obj_set_style_text_align(g.socLbl, LV_TEXT_ALIGN_CENTER, 0);
    g.socBar = lv_bar_create(content);
    lv_obj_set_size(g.socBar, sw - 24, 10);
    lv_bar_set_range(g.socBar, 0, 100);
    lv_obj_t* row = uiMakeCard(content, sw - 8, 36);
    g.battLbl = uiMakeLabel(row, "PV | Load | Grid", uiFontBody(), uiColor565(t.text));
    g.todayPv = uiMakeLabel(content, "Today -- kWh", uiFontBody(), uiColor565(t.muted));
  } else {
    g.socLbl = uiMakeLabel(content, "0%", uiFontDisplay(), uiColor565(t.text));
    lv_obj_set_width(g.socLbl, sw - 8);
    lv_obj_set_style_text_align(g.socLbl, LV_TEXT_ALIGN_CENTER, 0);
    g.socBar = lv_bar_create(content);
    lv_obj_set_size(g.socBar, sw - 24, 10);
    lv_bar_set_range(g.socBar, 0, 100);
    g.battLbl = uiMakeLabel(content, "Charging --", uiFontBody(), uiColor565(t.muted));
    lv_obj_set_width(g.battLbl, sw - 8);
    lv_obj_set_style_text_align(g.battLbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* gridRow = lv_obj_create(content);
    lv_obj_remove_style_all(gridRow);
    lv_obj_set_size(gridRow, sw - 8, 56);
    lv_obj_set_flex_flow(gridRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(gridRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    makeMetricCard(gridRow, "Solar", &g.pvVal, uiColor565(t.pv));
    makeMetricCard(gridRow, "Home", &g.loadVal, uiColor565(t.text));
    makeMetricCard(gridRow, "Grid", &g.gridVal, uiColor565(t.grid));
    makeMetricCard(gridRow, "Today", &g.todayPv, uiColor565(t.muted));

    g.todayLoad = uiMakeLabel(content, "Inverter: --", uiFontBody(), uiColor565(t.muted));
  }

  g.staleOverlay = lv_obj_create(content);
  lv_obj_remove_style_all(g.staleOverlay);
  lv_obj_set_size(g.staleOverlay, sw - 8, 28);
  lv_obj_set_style_bg_color(g.staleOverlay, uiColor565(t.danger), 0);
  lv_obj_set_style_bg_opa(g.staleOverlay, LV_OPA_60, 0);
  lv_obj_set_style_radius(g.staleOverlay, 6, 0);
  uiMakeLabel(g.staleOverlay, "STALE DATA", uiFontBody(), lv_color_white());
  lv_obj_add_flag(g.staleOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void setGridPill(UiGlanceWidgets& g, const GlanceData& data, bool gridAlert) {
  if (!g.gridPill) return;
  const ThemePalette& t = themeActive();
  lv_obj_t* lbl = lv_obj_get_child(g.gridPill, 0);
  const bool offline = gridAlert || data.grid_state == "offline" || data.grid_state == "brownout";
  lv_obj_set_style_bg_color(g.gridPill, uiColor565(offline ? t.danger : t.ok), 0);
  String text = offline ? "GRID OFFLINE" : ("GRID " + data.grid_label);
  text.toUpperCase();
  uiSetLabelText(lbl, text);
}

void uiGlanceUpdate(UiGlanceWidgets& g, const GlanceData& data, bool stale, bool gridAlert, uint8_t layoutId) {
  (void)layoutId;
  setGridPill(g, data, gridAlert);
  char buf[32];
  if (g.socLbl) {
    if (isnan(data.soc)) snprintf(buf, sizeof(buf), "--%%");
    else snprintf(buf, sizeof(buf), "%.0f%%", data.soc);
    uiSetLabelText(g.socLbl, buf);
  }
  if (g.socBar && !isnan(data.soc)) lv_bar_set_value(g.socBar, (int32_t)data.soc, LV_ANIM_ON);
  if (g.arc && !isnan(data.soc)) {
    lv_arc_set_value(g.arc, (int32_t)data.soc);
    lv_obj_set_style_arc_color(g.arc, uiSocColor(data.soc), LV_PART_INDICATOR);
  }
  if (g.battLbl) {
    String s = data.batt_status.length() ? data.batt_status : "Battery";
    s += " " + uiFmtPower(data.batt_w);
    uiSetLabelText(g.battLbl, s);
  }
  if (g.pvVal) uiSetLabelText(g.pvVal, uiFmtPower(data.pv_w));
  if (g.loadVal) uiSetLabelText(g.loadVal, uiFmtPower(data.load_w));
  if (g.gridVal) uiSetLabelText(g.gridVal, data.grid_label.length() ? data.grid_label : uiFmtPower(data.grid_w));
  if (g.todayPv && layoutId == 0) uiSetLabelText(g.todayPv, uiFmtKwh(data.pv_today_kwh) + " kWh");
  if (g.todayPv && layoutId == 1) uiSetLabelText(g.todayPv, "Today " + uiFmtKwh(data.pv_today_kwh) + " kWh");
  if (g.todayLoad) uiSetLabelText(g.todayLoad, "Inverter: " + (data.status.length() ? data.status : "--"));
  if (g.bars[0]) {
    lv_bar_set_value(g.bars[0], (int32_t)constrain(data.pv_w / 50.0f, 0, 100), LV_ANIM_ON);
    lv_bar_set_value(g.bars[1], (int32_t)constrain(data.load_w / 50.0f, 0, 100), LV_ANIM_ON);
    lv_bar_set_value(g.bars[2], (int32_t)constrain(fabsf(data.grid_w) / 30.0f, 0, 100), LV_ANIM_ON);
  }
  if (g.flowCol) {
    uiSetLabelText(g.pvVal, String("PV ") + uiFmtPower(data.pv_w));
    uiSetLabelText(g.loadVal, String("Home ") + uiFmtPower(data.load_w));
    uiSetLabelText(g.gridVal, String("Grid ") + uiFmtPower(data.grid_w));
    snprintf(buf, sizeof(buf), "%.0f%% SOC", isnan(data.soc) ? 0.0f : data.soc);
    uiSetLabelText(g.socLbl, buf);
  }
  if (g.staleOverlay) {
    if (stale) lv_obj_clear_flag(g.staleOverlay, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g.staleOverlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void uiGlancePulseGrid(UiGlanceWidgets& g, bool gridAlert, uint32_t animMs) {
  if (!g.gridPill || !gridAlert) return;
  const ThemePalette& t = themeActive();
  const bool hi = ((animMs / 500) % 2) == 0;
  lv_obj_set_style_bg_opa(g.gridPill, hi ? LV_OPA_COVER : LV_OPA_50, 0);
  lv_obj_set_style_bg_color(g.gridPill, uiColor565(t.danger), 0);
}
