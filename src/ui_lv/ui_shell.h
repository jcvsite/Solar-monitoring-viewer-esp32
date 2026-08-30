#pragma once
#include <lvgl.h>
#include <stdint.h>
#include "../ui.h"
#include "api_client.h"

struct UiShellWidgets {
  lv_obj_t* root = nullptr;
  lv_obj_t* header = nullptr;
  lv_obj_t* titleLbl = nullptr;
  lv_obj_t* rightLbl = nullptr;
  lv_obj_t* content = nullptr;
  lv_obj_t* nav = nullptr;
  lv_obj_t* navBtns[4] = {nullptr, nullptr, nullptr, nullptr};
};

void uiShellCreate(UiShellWidgets& w, UiPage page, bool showNav);
void uiShellDestroy(UiShellWidgets& w);
void uiShellSetPage(UiShellWidgets& w, UiPage page);
void uiShellSetHeader(UiShellWidgets& w, const String& left, const String& right);
void uiShellClearContent(UiShellWidgets& w);

void uiClockEnsure();
void uiClockSyncTimezone(int offsetSec);
String uiClockHeaderRight(const GlanceData& g);
