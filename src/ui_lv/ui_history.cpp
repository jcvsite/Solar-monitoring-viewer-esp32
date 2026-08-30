#include "ui_history.h"
#include "ui_theme.h"
#include "ui_util.h"
#include <stdio.h>

void uiHistoryBuild(UiShellWidgets& shell, UiHistoryWidgets& h) {
  uiShellClearContent(shell);
  h = UiHistoryWidgets();
  const ThemePalette& t = themeActive();
  const lv_coord_t sw = lv_obj_get_width(shell.content);
  const lv_coord_t ch = lv_obj_get_height(shell.content) - 40;

  h.chart = lv_chart_create(shell.content);
  lv_obj_set_size(h.chart, sw - 8, ch);
  lv_chart_set_type(h.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(h.chart, 24);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_obj_set_style_size(h.chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(h.chart, 2, LV_PART_ITEMS);
  h.serPv = lv_chart_add_series(h.chart, uiColor565(t.pv), LV_CHART_AXIS_PRIMARY_Y);
  h.serLoad = lv_chart_add_series(h.chart, uiColor565(t.text), LV_CHART_AXIS_PRIMARY_Y);

  h.footer = uiMakeLabel(shell.content, "Today PV -- | Load --", uiFontBody(), uiColor565(t.muted));
}

void uiHistoryUpdate(UiHistoryWidgets& h, const HistoryData& data) {
  if (!h.chart) return;
  lv_chart_set_point_count(h.chart, data.points.empty() ? 1 : (uint16_t)data.points.size());
  float maxW = 1000.0f;
  for (const auto& p : data.points) {
    if (!isnan(p.pv_w)) maxW = max(maxW, p.pv_w);
    if (!isnan(p.load_w)) maxW = max(maxW, p.load_w);
  }
  const int32_t ymax = (int32_t)max(maxW / 100.0f, 10.0f);
  lv_chart_set_range(h.chart, LV_CHART_AXIS_PRIMARY_Y, 0, ymax);

  for (size_t i = 0; i < data.points.size(); i++) {
    const auto& p = data.points[i];
    lv_chart_set_next_value(h.chart, h.serPv, isnan(p.pv_w) ? 0 : (int32_t)(p.pv_w / 100.0f));
    lv_chart_set_next_value(h.chart, h.serLoad, isnan(p.load_w) ? 0 : (int32_t)(p.load_w / 100.0f));
  }
  lv_chart_refresh(h.chart);

  char buf[64];
  snprintf(buf, sizeof(buf), "Today PV %s kWh | Load %s kWh", uiFmtKwh(data.today_pv).c_str(),
           uiFmtKwh(data.today_load).c_str());
  uiSetLabelText(h.footer, buf);
}
