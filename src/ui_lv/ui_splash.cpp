#include "ui_splash.h"
#include "logo_solar_monitoring_lvgl.h"
#include "ui_theme.h"
#include "ui_util.h"

static lv_obj_t* s_splash = nullptr;
static lv_obj_t* s_msgLbl = nullptr;

void uiSplashShow(const char* msg) {
  if (s_splash) lv_obj_del(s_splash);
  s_splash = lv_obj_create(NULL);
  lv_obj_remove_style_all(s_splash);
  lv_obj_set_style_bg_color(s_splash, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(s_splash, LV_OPA_COVER, 0);
  lv_obj_set_size(s_splash, LV_HOR_RES, LV_VER_RES);

  lv_obj_t* img = lv_img_create(s_splash);
  lv_img_set_src(img, &logo_solar_monitoring_img);
  lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 8);

  uiMakeLabel(s_splash, "Solar Monitoring", uiFontTitle(), lv_color_hex(0x210400));
  lv_obj_align(lv_obj_get_child(s_splash, 1), LV_ALIGN_TOP_MID, 0, 108);
  uiMakeLabel(s_splash, "Viewer", uiFontTitle(), lv_color_hex(0xFD2000));
  lv_obj_align(lv_obj_get_child(s_splash, 2), LV_ALIGN_TOP_MID, 0, 128);

  s_msgLbl = uiMakeLabel(s_splash, msg ? msg : "", uiFontBody(), lv_color_hex(0x7BEF00));
  lv_obj_align(s_msgLbl, LV_ALIGN_TOP_MID, 0, 152);
  lv_scr_load(s_splash);
}

void uiSplashSetMsg(const char* msg) {
  if (s_msgLbl) uiSetLabelText(s_msgLbl, msg ? msg : "");
}
