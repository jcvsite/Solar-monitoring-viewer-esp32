#include "ui_history.h"
#include "ui_theme.h"
#include "ui_util.h"
#include "config.h"
#include <stdio.h>

static lv_coord_t shellContentW(const UiShellWidgets& shell) {
  const lv_coord_t w = lv_obj_get_width(shell.root);
  return w > 0 ? w - 8 : 300;
}

static lv_coord_t shellContentH(const UiShellWidgets& shell) {
  const lv_coord_t h = lv_obj_get_height(shell.root);
  return h > 0 ? h - 24 - UI_NAV_H - 8 : 180;
}

static lv_obj_t* addStatRow(lv_obj_t* parent, lv_coord_t w, lv_coord_t rowH, const char* icon, const char* title,
                            lv_color_t accent, lv_obj_t** valOut) {
  const ThemePalette& t = themeActive();
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, w, rowH);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* left = lv_obj_create(row);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, LV_SIZE_CONTENT, rowH);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(left, 4, 0);
  uiMakeLabel(left, icon, uiFontBody(), accent);
  uiMakeLabel(left, title, uiFontBody(), uiColor565(t.muted));

  *valOut = uiMakeLabel(row, "-- kWh", uiFontBody(), uiColor565(t.text));
  return row;
}

bool uiHistoryNeedsBuild(const UiHistoryWidgets& h) { return !h.chart; }

void uiHistoryBuild(UiShellWidgets& shell, UiHistoryWidgets& h) {
  uiShellClearContent(shell);
  h = UiHistoryWidgets();
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = shellContentW(shell);
  const lv_coord_t totalH = shellContentH(shell);
  const bool landscape = shell.root && lv_obj_get_width(shell.root) > lv_obj_get_height(shell.root);
  const lv_coord_t chartH =
      landscape ? (lv_coord_t)max((int)(totalH / 3), 56) : (lv_coord_t)max((int)(totalH / 4), 70);
  const lv_coord_t rowH = landscape ? 16 : 22;

  h.emptyLbl = uiMakeLabel(shell.content, "No history yet", uiFontTitle(), uiColor565(t.muted));
  lv_obj_add_flag(h.emptyLbl, LV_OBJ_FLAG_HIDDEN);

  h.legend = uiMakeLabel(shell.content, LV_SYMBOL_LIST " 24h  PV / Load / SOC", uiFontBody(), uiColor565(t.grid));
  lv_obj_set_width(h.legend, sw);

  h.chart = lv_chart_create(shell.content);
  lv_obj_set_size(h.chart, sw, chartH);
  lv_chart_set_type(h.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(h.chart, 48);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
  lv_chart_set_div_line_count(h.chart, 2, 3);
  lv_obj_set_style_bg_color(h.chart, uiColor565(t.card), 0);
  lv_obj_set_style_bg_opa(h.chart, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(h.chart, uiColor565(t.line), 0);
  lv_obj_set_style_border_width(h.chart, 1, 0);
  lv_obj_set_style_line_width(h.chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(h.chart, 0, LV_PART_INDICATOR);
  h.serPv = lv_chart_add_series(h.chart, uiColor565(t.pv), LV_CHART_AXIS_PRIMARY_Y);
  h.serLoad = lv_chart_add_series(h.chart, uiColor565(t.text), LV_CHART_AXIS_PRIMARY_Y);
  h.serSoc = lv_chart_add_series(h.chart, uiColor565(t.charge), LV_CHART_AXIS_SECONDARY_Y);

  h.statsCard = uiMakeCard(shell.content, sw, totalH - chartH - 24);
  lv_obj_set_style_pad_all(h.statsCard, landscape ? 3 : 6, 0);
  lv_obj_set_flex_flow(h.statsCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(h.statsCard, landscape ? 1 : 4, 0);
  lv_obj_add_flag(h.statsCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(h.statsCard, LV_DIR_VER);

  static const char* icons[] = {LV_SYMBOL_CHARGE, LV_SYMBOL_HOME, LV_SYMBOL_DOWNLOAD, LV_SYMBOL_UPLOAD,
                                LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_BATTERY_EMPTY};
  static const char* titles[] = {"Today PV", "Today Load", "Grid In", "Grid Out", "Batt Charge", "Batt Discharge"};
  uint16_t colors[] = {t.pv, t.text, t.gridImport, t.gridExport, t.charge, t.disch};
  for (int i = 0; i < 6; i++) {
    addStatRow(h.statsCard, sw - 16, rowH, icons[i], titles[i], uiColor565(colors[i]), &h.statVal[i]);
  }
}

void uiHistoryUpdate(UiHistoryWidgets& h, const HistoryData& data) {
  if (!h.chart) return;

  const bool empty = !data.ok || data.points.empty();
  if (empty) {
    lv_obj_clear_flag(h.emptyLbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h.legend, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h.chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(h.statsCard, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(h.emptyLbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(h.legend, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(h.chart, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(h.statsCard, LV_OBJ_FLAG_HIDDEN);

  char legend[32];
  snprintf(legend, sizeof(legend), LV_SYMBOL_LIST " %dh  PV / Load / SOC", data.hours > 0 ? data.hours : 24);
  uiSetLabelText(h.legend, legend);

  // Evenly sample across the full window into the fixed 48-point series.
  const size_t srcN = data.points.size();
  const uint16_t n = 48;

  float maxW = 500.0f;
  for (const auto& p : data.points) {
    if (!isnan(p.pv_w)) maxW = max(maxW, p.pv_w);
    if (!isnan(p.load_w)) maxW = max(maxW, p.load_w);
  }
  const int32_t ymax = (int32_t)max(maxW / 100.0f, 5.0f);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_PRIMARY_Y, 0, ymax);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);

  lv_coord_t* pvPts = lv_chart_get_y_array(h.chart, h.serPv);
  lv_coord_t* loadPts = lv_chart_get_y_array(h.chart, h.serLoad);
  lv_coord_t* socPts = lv_chart_get_y_array(h.chart, h.serSoc);
  for (uint16_t i = 0; i < n; i++) {
    size_t idx = (srcN <= 1) ? 0 : (i * (srcN - 1)) / (n - 1);
    if (idx >= srcN) idx = srcN - 1;
    const auto& p = data.points[idx];
    pvPts[i] = isnan(p.pv_w) ? 0 : (lv_coord_t)(p.pv_w / 100.0f);
    loadPts[i] = isnan(p.load_w) ? 0 : (lv_coord_t)(p.load_w / 100.0f);
    socPts[i] = isnan(p.soc) ? 0 : (lv_coord_t)p.soc;
  }
  lv_chart_refresh(h.chart);

  const float vals[] = {data.today_pv, data.today_load, data.today_gi, data.today_ge, data.today_bc, data.today_bd};
  for (int i = 0; i < 6; i++) {
    if (h.statVal[i]) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%s kWh", uiFmtKwh(vals[i]).c_str());
      uiSetLabelText(h.statVal[i], buf);
    }
  }
}
