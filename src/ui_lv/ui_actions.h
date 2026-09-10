#pragma once
#include "ui.h"
#include <stdint.h>

struct HostSettings;
struct GlanceData;
struct BmsData;
struct HistoryData;
struct WifiNetwork;
class ApiClient;
class SettingsStore;
class Discovery;

enum class UiActionId : uint8_t {
  NavPage,
  OpenSettings,
  SettingsTab,
  CycleBrightness,
  ToggleNightMode,
  CycleNightBrightness,
  CycleNightStart,
  CycleNightEnd,
  OpenPickLayout,
  OpenPickTheme,
  PickLayout,
  PickTheme,
  RotateScreen,
  OpenPinSet,
  StartHostDiscovery,
  OpenManualHost,
  StartWifiScan,
  WifiRescan,
  WifiPhonePortal,
  WifiPickNetwork,
  WifiConnect,
  WifiToggleShowPass,
  WifiBack,
  HostChoiceDiscover,
  HostChoiceManual,
  HostChoiceSkip,
  FindingHostCancel,
  ManualOctetSelect,
  ManualOctetDec,
  ManualOctetInc,
  ManualPortDec,
  ManualPortInc,
  ManualTest,
  ManualSave,
  ManualBack,
  ToggleCheckUpdate,
  ToggleAutoUpdate,
  ToggleGridAlert,
  ToggleUseHostConfig,
  PinDigit,
  PinDelete,
  PinOk,
  PinBack,
  OtaCheckNow,
};

struct UiActionCtx {
  UiPage page = UiPage::Glance;
  UiSettingsTab settingsTab = UiSettingsTab::Connection;
  int index = -1;
  int digit = -1;
};

typedef void (*UiActionHandler)(UiActionId id, const UiActionCtx& ctx);

void uiActionsSetHandler(UiActionHandler handler);
void uiActionsFire(UiActionId id, const UiActionCtx& ctx);
