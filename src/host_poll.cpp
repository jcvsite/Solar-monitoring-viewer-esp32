#include "host_poll.h"

#include <WiFi.h>

static ApiClient* s_api = nullptr;
static HostSettings* s_settings = nullptr;
static TaskHandle_t s_task = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

static volatile bool s_busy = false;
static volatile int s_wantPage = (int)UiPage::Glance;
static volatile bool s_wantConfig = false;

static GlanceData s_glance;
static BmsData s_bms;
static HistoryData s_history;
static DisplayConfig s_config;
static volatile bool s_haveGlance = false;
static volatile bool s_haveBms = false;
static volatile bool s_haveHistory = false;
static volatile bool s_haveConfig = false;

static void hostPollTask(void* arg) {
  (void)arg;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!s_api || !s_settings) continue;

    String hostIp;
    uint16_t hostPort = 0;
    String token;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(80)) == pdTRUE) {
      hostIp = s_settings->hostIp;
      hostPort = s_settings->hostPort;
      token = s_settings->token;
      xSemaphoreGive(s_mutex);
    } else {
      continue;
    }

    if (hostIp.length() == 0 || WiFi.status() != WL_CONNECTED) {
      s_busy = false;
      s_wantConfig = false;
      continue;
    }

    s_busy = true;
    const bool wantConfig = s_wantConfig;
    s_wantConfig = false;
    const UiPage page = (UiPage)s_wantPage;

    if (wantConfig) {
      DisplayConfig cfg;
      if (s_api->fetchDisplayConfig(hostIp, hostPort, token, cfg)) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(80)) == pdTRUE) {
          s_config = cfg;
          s_haveConfig = true;
          xSemaphoreGive(s_mutex);
        }
      }
    } else if (page == UiPage::Bms) {
      BmsData b;
      if (s_api->fetchBms(hostIp, hostPort, token, b)) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(80)) == pdTRUE) {
          s_bms = b;
          s_haveBms = true;
          xSemaphoreGive(s_mutex);
        }
      }
    } else if (page == UiPage::History) {
      HistoryData h;
      if (s_api->fetchHistory(hostIp, hostPort, token, 24, h)) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(80)) == pdTRUE) {
          s_history = h;
          s_haveHistory = true;
          xSemaphoreGive(s_mutex);
        }
      }
    } else {
      GlanceData g;
      if (s_api->fetchGlance(hostIp, hostPort, token, g)) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(80)) == pdTRUE) {
          s_glance = g;
          s_haveGlance = true;
          xSemaphoreGive(s_mutex);
        }
      }
    }

    s_busy = false;
  }
}

void hostPollBegin(ApiClient* api, HostSettings* settings) {
  s_api = api;
  s_settings = settings;
  if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
  if (!s_task) {
    xTaskCreatePinnedToCore(hostPollTask, "hostPoll", 12288, nullptr, 1, &s_task, 0);
  }
}

void hostPollRequest(UiPage page, bool /*force*/) {
  s_wantPage = (int)page;
  if (s_task) xTaskNotifyGive(s_task);
}

void hostPollRequestConfig() {
  s_wantConfig = true;
  if (s_task) xTaskNotifyGive(s_task);
}

bool hostPollBusy() { return s_busy; }

bool hostPollTakeGlance(GlanceData& out) {
  if (!s_haveGlance || !s_mutex) return false;
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  out = s_glance;
  s_haveGlance = false;
  xSemaphoreGive(s_mutex);
  return true;
}

bool hostPollTakeBms(BmsData& out) {
  if (!s_haveBms || !s_mutex) return false;
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  out = s_bms;
  s_haveBms = false;
  xSemaphoreGive(s_mutex);
  return true;
}

bool hostPollTakeHistory(HistoryData& out) {
  if (!s_haveHistory || !s_mutex) return false;
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  out = s_history;
  s_haveHistory = false;
  xSemaphoreGive(s_mutex);
  return true;
}

bool hostPollTakeConfig(DisplayConfig& out) {
  if (!s_haveConfig || !s_mutex) return false;
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  out = s_config;
  s_haveConfig = false;
  xSemaphoreGive(s_mutex);
  return true;
}
