#include "ui_bms.h"
#include "cell_colors.h"
#include "ui_theme.h"
#include "ui_util.h"
#include <stdio.h>

void uiBmsBuild(UiShellWidgets& shell, UiBmsWidgets& b) {
  uiShellClearContent(shell);
  b = UiBmsWidgets();
  const lv_coord_t sw = lv_obj_get_width(shell.content);

  b.summary = uiMakeCard(shell.content, sw - 8, 48);
  lv_obj_set_flex_flow(b.summary, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(b.summary, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  for (int i = 0; i < 3; i++) {
    uiMakeLabel(b.summary, "--", uiFontBody(), uiColor565(themeActive().text));
  }

  b.cellGrid = lv_obj_create(shell.content);
  lv_obj_remove_style_all(b.cellGrid);
  lv_obj_set_size(b.cellGrid, sw - 8, lv_obj_get_height(shell.content) - 56);
  lv_obj_set_flex_flow(b.cellGrid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(b.cellGrid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(b.cellGrid, 4, 0);
  lv_obj_set_style_pad_column(b.cellGrid, 4, 0);
  lv_obj_add_flag(b.cellGrid, LV_OBJ_FLAG_SCROLLABLE);
}

void uiBmsUpdate(UiBmsWidgets& b, const BmsData& data) {
  if (!b.summary) return;
  char buf[48];
  lv_obj_t* l0 = lv_obj_get_child(b.summary, 0);
  lv_obj_t* l1 = lv_obj_get_child(b.summary, 1);
  lv_obj_t* l2 = lv_obj_get_child(b.summary, 2);
  snprintf(buf, sizeof(buf), "SOC %.0f%%", isnan(data.soc) ? 0.0f : data.soc);
  uiSetLabelText(l0, buf);
  snprintf(buf, sizeof(buf), "%.1f V", isnan(data.volts) ? 0.0f : data.volts);
  uiSetLabelText(l1, buf);
  snprintf(buf, sizeof(buf), "%.1f A", isnan(data.amps) ? 0.0f : data.amps);
  uiSetLabelText(l2, buf);

  if (!b.cellGrid) return;
  lv_obj_clean(b.cellGrid);
  const int cols = 4;
  const lv_coord_t sw = lv_obj_get_width(b.cellGrid);
  const lv_coord_t cw = (sw - (cols - 1) * 4) / cols;
  for (size_t i = 0; i < data.cells.size(); i++) {
    const float v = data.cells[i];
    lv_obj_t* cell = lv_obj_create(b.cellGrid);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, cw, 28);
    lv_obj_set_style_radius(cell, 6, 0);
    lv_obj_set_style_bg_color(cell, uiColor565(cellVoltageColor(v)), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    snprintf(buf, sizeof(buf), "%.2f", v);
    lv_obj_t* lbl = uiMakeLabel(cell, buf, uiFontBody(), lv_color_white());
    lv_obj_center(lbl);
  }
}
