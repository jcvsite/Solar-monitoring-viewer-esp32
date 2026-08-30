#pragma once
#include <TFT_eSPI.h>
#include "api_client.h"
#include "touch_input.h"
#include "settings_store.h"

enum class Page : uint8_t {
  Glance = 0,
  Bms = 1,
  History = 2,
  Settings = 3,
  SettingsConn = 4,
  SettingsUpdates = 5,
  PickLayout = 6,
  PickTheme = 7,
  FindingHost = 8,
  ManualHost = 9,
  HostChoice = 10
};

enum class SettingsTab : uint8_t { Connection = 0, Updates = 1 };

class Ui {
 public:
  void begin(TFT_eSPI& tft);
  void setTheme(uint8_t themeId);
  void setRotation(uint8_t rotation);

  void drawGlance(TFT_eSPI& tft, const GlanceData& g, bool stale, uint32_t animMs, uint8_t layoutId,
                  bool gridAlert);
  void drawGlanceGridPulse(TFT_eSPI& tft, const GlanceData& g, uint32_t animMs, bool gridAlert);
  void ensureClock();
  void syncClockTimezone(int offsetSec);
  void refreshHeaderTime(TFT_eSPI& tft, const GlanceData& g);

  void drawBms(TFT_eSPI& tft, const BmsData& b);
  void drawHistory(TFT_eSPI& tft, const HistoryData& h);

  void drawSettings(TFT_eSPI& tft, const HostSettings& s, bool wifiOk, const String& wifiSsid,
                    const String& statusMsg, SettingsTab tab);
  void drawSettingsConnection(TFT_eSPI& tft, const HostSettings& s, bool wifiOk, const String& wifiSsid,
                              const String& statusMsg);
  void drawSettingsUpdates(TFT_eSPI& tft, const HostSettings& s, const String& otaStatus,
                           const String& fwVersion);
  void drawPickList(TFT_eSPI& tft, const char* title, const char* const* names, int count, int selected);

  void drawFindingHost(TFT_eSPI& tft, const String& statusMsg);
  void drawHostChoice(TFT_eSPI& tft, const String& wifiSsid);
  void drawManualHost(TFT_eSPI& tft, const uint8_t octets[4], uint8_t selectedOctet, uint16_t port,
                      const String& statusMsg);

  void drawNav(TFT_eSPI& tft, Page page);
  bool navPageAt(int16_t x, int16_t y, Page& outPage, uint8_t rotation);
  bool settingsTabAt(int16_t x, int16_t y, SettingsTab& tab, uint8_t rotation);
  bool pickListAt(int16_t x, int16_t y, int itemH, int count, int& index, uint8_t rotation);

  void drawTouchDebug(TFT_eSPI& tft, const TouchSample& s);
  void drawSplash(TFT_eSPI& tft, const char* msg);
  void drawWifiPortal(TFT_eSPI& tft, const char* apName);

  String fmtPower(float w);
  String fmtKwh(float k);
  uint16_t socColor(float soc);

 private:
  void fillHeader(TFT_eSPI& tft, const String& titleLeft, const String& titleRight, uint8_t rotation);
  void fillGlanceHeader(TFT_eSPI& tft, const GlanceData& g, uint8_t rotation);
  void drawListRow(TFT_eSPI& tft, int y, int rightX, const char* label, const String& value, bool last);
  uint8_t rotation_ = 0;
};
