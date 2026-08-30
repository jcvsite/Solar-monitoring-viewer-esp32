#pragma once
#include <lvgl.h>
#include "api_client.h"
#include "ui_shell.h"

struct UiBmsWidgets {
  lv_obj_t* summary = nullptr;
  lv_obj_t* cellGrid = nullptr;
};

void uiBmsBuild(UiShellWidgets& shell, UiBmsWidgets& b);
void uiBmsUpdate(UiBmsWidgets& b, const BmsData& data);
