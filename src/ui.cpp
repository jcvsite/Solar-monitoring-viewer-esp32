#include "ui.h"
#include "config.h"
#include "layout.h"
#include "theme.h"
#include "cell_colors.h"
#include "glance_layouts.h"
#include "logo_solar_monitoring.h"
#include <cstdio>
#include <math.h>
#include <time.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static const ThemePalette& th() { return themeActive(); }

static constexpr uint16_t kSplashBg = 0xFFFF;
static constexpr uint16_t kSplashText = 0x2104;
static constexpr uint16_t kSplashAccent = 0xFD20;
static constexpr uint16_t kSplashMuted = 0x7BEF;

static void drawBrandLogo(TFT_eSPI& tft, int centerX, int centerY) {
  int x = centerX - LOGO_SOLAR_MONITORING_W / 2;
  int y = centerY - LOGO_SOLAR_MONITORING_H / 2;
  tft.pushImage(x, y, LOGO_SOLAR_MONITORING_W, LOGO_SOLAR_MONITORING_H, logo_solar_monitoring);
}

void Ui::setTheme(uint8_t themeId) {
  themeSetActive(themeId);
}

void Ui::setRotation(uint8_t rotation) {
  rotation_ = rotation & 3;
}

void Ui::begin(TFT_eSPI& tft) {
  tft.fillScreen(th().bg);
}

void Ui::drawSplash(TFT_eSPI& tft, const char* msg) {
  int w = scrW(rotation_);
  int h = scrH(rotation_);
  tft.fillScreen(kSplashBg);
  int logoY = min(24, (h - LOGO_SOLAR_MONITORING_H) / 2);
  drawBrandLogo(tft, w / 2, logoY + LOGO_SOLAR_MONITORING_H / 2);
  int textY = logoY + LOGO_SOLAR_MONITORING_H + 12;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kSplashText, kSplashBg);
  tft.drawString("Solar Monitoring", w / 2, textY, 2);
  tft.setTextColor(kSplashAccent, kSplashBg);
  tft.drawString("Viewer", w / 2, textY + 18, 2);
  tft.setTextColor(kSplashMuted, kSplashBg);
  if (textY + 36 < h - 8) {
    tft.drawString(msg, w / 2, textY + 36, 1);
  }
}

void Ui::drawWifiPortal(TFT_eSPI& tft, const char* apName) {
  int w = scrW(rotation_);
  int h = scrH(rotation_);
  tft.fillScreen(th().bg);
  drawBrandLogo(tft, w / 2, 52);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().bg);
  tft.drawString("Phone WiFi Setup", w / 2, 108, 4);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("1. Join WiFi AP:", w / 2, 138, 2);
  tft.setTextColor(th().text, th().bg);
  tft.drawString(apName, w / 2, 158, 2);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("2. Browser opens (or go to):", w / 2, 186, 2);
  tft.setTextColor(th().pv, th().bg);
  tft.drawString("192.168.4.1", w / 2, 208, 4);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("Pick network + password, Save", w / 2, min(h - 52, 238), 2);
  tft.drawString("Tip: use touch WiFi on device instead", w / 2, min(h - 28, 262), 1);
}

void Ui::fillHeader(TFT_eSPI& tft, const String& titleLeft, const String& titleRight, uint8_t rotation) {
  int w = scrW(rotation);
  tft.fillRect(0, 0, w, 22, th().header);
  tft.drawFastHLine(0, 22, w, th().line);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(th().text, th().header);
  String t = titleLeft;
  if (t.length() > 14) t = t.substring(0, 13) + ".";
  tft.drawString(t, 4, 11, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(titleRight, w - 4, 11, 2);
}

static bool s_clockStarted = false;
static int s_activeTzOffset = TIMEZONE_OFFSET_SEC;

void Ui::ensureClock() {
  if (s_clockStarted) return;
  configTime(s_activeTzOffset, 0, "pool.ntp.org", "time.nist.gov");
  s_clockStarted = true;
}

void Ui::syncClockTimezone(int offsetSec) {
  if (offsetSec < -43200 || offsetSec > 50400) return;
  if (offsetSec == s_activeTzOffset && s_clockStarted) return;
  s_activeTzOffset = offsetSec;
  configTime(s_activeTzOffset, 0, "pool.ntp.org", "time.nist.gov");
  s_clockStarted = true;
}

void Ui::fillGlanceHeader(TFT_eSPI& tft, const GlanceData& g, uint8_t rotation) {
  (void)tft;
  (void)g;
  (void)rotation;
}

void Ui::refreshHeaderTime(TFT_eSPI& tft, const GlanceData& g) {
  drawGlanceHeaderOnly(tft, rotation_, g);
}

String Ui::fmtPower(float w) {
  if (isnan(w)) return "--";
  float a = fabsf(w);
  char buf[16];
  if (a >= 1000.0f) snprintf(buf, sizeof(buf), "%.1f kW", a / 1000.0f);
  else snprintf(buf, sizeof(buf), "%.0f W", a);
  return String(buf);
}

String Ui::fmtKwh(float k) {
  if (isnan(k)) return "--";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", k);
  return String(buf);
}

uint16_t Ui::socColor(float soc) {
  if (isnan(soc)) return th().muted;
  if (soc < 20) return th().danger;
  if (soc < 40) return th().warn;
  return th().charge;
}

void Ui::drawNav(TFT_eSPI& tft, Page page) {
  uint8_t rot = rotation_;
  int y = navY(rot);
  int w = scrW(rot);
  const int h = UI_NAV_H;
  const int bw = (w - 8 - 9) / 4;
  const int gap = 3;
  const char* labels[4] = {"Home", "BMS", "Hist", "Set"};
  const uint16_t accent[4] = {th().pv, th().charge, th().grid, th().dim};

  tft.fillRect(0, y, w, h, th().card);
  tft.drawFastHLine(0, y, w, th().line);

  for (int i = 0; i < 4; i++) {
    int x = 4 + i * (bw + gap);
    bool on = (uint8_t)page == i;
    uint16_t fg = on ? accent[i] : th().dim;
    if (on && i == 3) fg = th().text;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, th().card);
    tft.drawString(labels[i], x + bw / 2, y + h / 2 + 2, 1);
    if (on) tft.fillCircle(x + bw / 2, y + 5, 2, accent[i == 3 ? 0 : i]);
#if TOUCH_SHOW_DEBUG
    tft.drawRect(x, y + 3, bw, h - 6, TFT_YELLOW);
#endif
  }
}

bool Ui::navPageAt(int16_t x, int16_t y, Page& outPage, uint8_t rotation) {
  int ny = navY(rotation);
  int sh = scrH(rotation);
  if (y < ny || y > sh - 1) return false;
  int w = scrW(rotation);
  const int bw = (w - 8 - 9) / 4;
  const int gap = 3;
  const int btnH = UI_NAV_H - 6;
  for (int i = 0; i < 4; i++) {
    int bx = 4 + i * (bw + gap);
    if (x >= bx && x <= bx + bw && y >= ny + 3 && y <= ny + 3 + btnH) {
      outPage = (Page)i;
      return true;
    }
  }
  return false;
}

bool Ui::settingsTabAt(int16_t x, int16_t y, SettingsTab& tab, uint8_t rotation) {
  int w = scrW(rotation);
  if (y < 24 || y > 44) return false;
  int half = w / 2;
  if (x < half) {
    tab = SettingsTab::Connection;
    return true;
  }
  if (x >= half) {
    tab = SettingsTab::Updates;
    return true;
  }
  return false;
}

bool Ui::pickListAt(int16_t x, int16_t y, int itemH, int count, int& index, uint8_t rotation) {
  int w = scrW(rotation);
  if (x < 8 || x > w - 8) return false;
  int startY = 50;
  if (y < startY) return false;
  index = (y - startY) / itemH;
  if (index < 0 || index >= count) return false;
  return true;
}

void Ui::drawTouchDebug(TFT_eSPI& tft, const TouchSample& s) {
  if (!s.active) return;
  int w = scrW(rotation_);
  tft.fillRoundRect(4, 24, w - 8, 28, 4, th().bg);
  tft.drawRoundRect(4, 24, w - 8, 28, 4, TFT_YELLOW);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_YELLOW, th().bg);
  char buf[64];
  snprintf(buf, sizeof(buf), "touch %d,%d  raw %d,%d z=%d", s.x, s.y, s.rawX, s.rawY, s.rawZ);
  tft.drawString(buf, 10, 38, 1);
  tft.drawCircle(s.x, s.y, 12, TFT_YELLOW);
  tft.fillCircle(s.x, s.y, 4, TFT_WHITE);
}

void Ui::drawGlance(TFT_eSPI& tft, const GlanceData& g, bool stale, uint32_t animMs, uint8_t layoutId,
                    bool gridAlert) {
  drawGlanceLayout(tft, layoutId, rotation_, g, stale, animMs, gridAlert);
  drawNav(tft, Page::Glance);
}

void Ui::drawGlanceGridPulse(TFT_eSPI& tft, const GlanceData& g, uint32_t animMs, bool gridAlert) {
  drawGlanceGridPulseLayout(tft, rotation_, g, animMs, gridAlert);
}

void Ui::drawListRow(TFT_eSPI& tft, int y, int rightX, const char* label, const String& value, bool last) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().card);
  tft.drawString(label, 12, y, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(th().text, th().card);
  tft.drawString(value, rightX, y, 2);
  if (!last) tft.drawFastHLine(12, y + 16, rightX - 12, th().line);
}

static void plotHistoryLine(TFT_eSPI& tft, const std::vector<HistoryPoint>& points, int plotX, int plotY,
                            int plotW, int plotH, float vmax, float (*getter)(const HistoryPoint&),
                            uint16_t color) {
  if (points.size() < 2 || vmax <= 0) return;
  int prevX = -1, prevY = -1;
  size_t n = points.size();
  for (size_t i = 0; i < n; i++) {
    float v = getter(points[i]);
    if (isnan(v)) continue;
    int x = plotX + (int)(i * (plotW - 1) / (n > 1 ? (n - 1) : 1));
    int y = plotY + plotH - 1 - (int)(constrain(v / vmax, 0.0f, 1.0f) * (plotH - 2));
    if (prevX >= 0) tft.drawLine(prevX, prevY, x, y, color);
    prevX = x;
    prevY = y;
  }
}

static float hpPv(const HistoryPoint& p) { return p.pv_w; }
static float hpLoad(const HistoryPoint& p) { return p.load_w; }
static float hpSoc(const HistoryPoint& p) { return p.soc; }

void Ui::drawBms(TFT_eSPI& tft, const BmsData& b) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int cH = contentH(rot);
  int rightX = w - 12;
  tft.fillRect(0, 22, w, cH, th().bg);
  fillHeader(tft, "Battery", b.ok ? "BMS" : "N/A", rot);
  if (!b.ok) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th().muted, th().bg);
    tft.drawString("No BMS data", w / 2, 22 + cH / 2, 4);
    drawNav(tft, Page::Bms);
    return;
  }

  int cardW = w - 16;
  tft.fillRoundRect(8, 28, cardW, 118, 12, th().card);
  drawListRow(tft, 38, rightX, "State of charge", isnan(b.soc) ? "--" : String(b.soc, 1) + "%", false);
  drawListRow(tft, 56, rightX, "State of health", isnan(b.soh) ? "--" : String(b.soh, 1) + "%", false);
  drawListRow(tft, 74, rightX, "Pack voltage", isnan(b.volts) ? "--" : String(b.volts, 2) + " V", false);
  drawListRow(tft, 92, rightX, "Current", isnan(b.amps) ? "--" : String(b.amps, 2) + " A", false);
  drawListRow(tft, 110, rightX, "Power", isnan(b.watts) ? "--" : fmtPower(b.watts), false);
  drawListRow(tft, 128, rightX, "Temperature", isnan(b.temp_avg) ? "--" : String(b.temp_avg, 1) + " C", true);

  tft.fillRoundRect(8, 152, cardW, 52, 12, th().card);
  drawListRow(tft, 162, rightX, "Cell delta", isnan(b.cell_delta_v) ? "--" : String(b.cell_delta_v * 1000, 0) + " mV",
              false);
  drawListRow(tft, 180, rightX, "Cycles", isnan(b.cycles) ? "--" : String((int)b.cycles), false);
  drawListRow(tft, 198, rightX, "Cell count", String(b.cell_count), true);

  if (!b.cells.empty()) {
    int n = min((int)b.cells.size(), isLandscape(rot) ? 32 : 24);
    int minIdx = 0, maxIdx = 0;
    float vmin = b.cells[0], vmax = b.cells[0];
    for (int i = 1; i < n; i++) {
      if (b.cells[i] < vmin) {
        vmin = b.cells[i];
        minIdx = i;
      }
      if (b.cells[i] > vmax) {
        vmax = b.cells[i];
        maxIdx = i;
      }
    }
    const int cellH = isLandscape(rot) ? 40 : 48;
    const int cellY = navY(rot) - 6 - cellH;
    const int x0 = 8;
    int cw = max(2, cardW / n - 1);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(th().muted, th().bg);
    tft.drawString("Cell voltages", 8, cellY - 10, 1);
    tft.drawRect(x0, cellY, cardW, cellH, th().line);
    for (int i = 0; i < n; i++) {
      int cx = x0 + 2 + i * (cw + 1);
      uint16_t c = cellVoltageColor(b.cells[i]);
      tft.fillRect(cx, cellY + 2, cw, cellH - 4, c);
      char vbuf[8];
      snprintf(vbuf, sizeof(vbuf), "%.2f", b.cells[i]);
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(th().text, c);
      tft.drawString(vbuf, cx + cw / 2, cellY + cellH / 2 - 4, 1);
      if (i == minIdx) {
        tft.setTextColor(th().grid, th().bg);
        tft.drawString("v", cx + cw / 2, cellY - 2, 1);
      }
      if (i == maxIdx) {
        tft.setTextColor(th().pv, th().bg);
        tft.drawString("^", cx + cw / 2, cellY - 2, 1);
      }
    }
  }
  drawNav(tft, Page::Bms);
}

void Ui::drawHistory(TFT_eSPI& tft, const HistoryData& h) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int cH = contentH(rot);
  int cardW = w - 16;
  tft.fillRect(0, 22, w, cH, th().bg);
  fillHeader(tft, "History", String(h.hours) + "h", rot);

  int plotX = 8, plotW = cardW;
  int plotY = 30;
  int plotH = isLandscape(rot) ? 70 : 98;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("PV / Load / SOC", 10, 24, 1);
  tft.drawRect(plotX, plotY, plotW, plotH, th().line);
  tft.fillRoundRect(plotX + 1, plotY + 1, plotW - 2, plotH - 2, 8, th().card);

  if (h.points.size() >= 2) {
    float pmax = 1;
    bool hasSoc = false;
    for (auto& p : h.points) {
      if (!isnan(p.pv_w)) pmax = max(pmax, p.pv_w);
      if (!isnan(p.load_w)) pmax = max(pmax, p.load_w);
      if (!isnan(p.soc)) hasSoc = true;
    }
    plotHistoryLine(tft, h.points, plotX + 1, plotY + 1, plotW - 2, plotH - 2, pmax, hpPv, th().pv);
    plotHistoryLine(tft, h.points, plotX + 1, plotY + 1, plotW - 2, plotH - 2, pmax, hpLoad, th().text);
    if (hasSoc)
      plotHistoryLine(tft, h.points, plotX + 1, plotY + 1, plotW - 2, plotH - 2, 100.0f, hpSoc, th().charge);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th().muted, th().bg);
    tft.drawString("No history yet", plotX + plotW / 2, plotY + plotH / 2, 2);
  }

  auto legend = [&](int x, const char* label, uint16_t color) {
    tft.fillRect(x, plotY + plotH + 6, 10, 3, color);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, th().bg);
    tft.drawString(label, x + 13, plotY + plotH + 2, 1);
  };
  legend(10, "PV", th().pv);
  legend(52, "Load", th().text);
  legend(98, "SOC%", th().charge);

  int todayY = plotY + plotH + (isLandscape(rot) ? 16 : 24);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("Today", 10, todayY, 1);
  tft.drawFastHLine(8, todayY + 10, cardW, th().line);

  int mid = w / 2;
  auto putPair = [&](int y, const char* ll, float lv, uint16_t lc, const char* rl, float rv, uint16_t rc) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(lc, th().bg);
    tft.drawString(ll, 8, y, 1);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(th().text, th().bg);
    tft.drawString(fmtKwh(lv), mid - 4, y, 1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(rc, th().bg);
    tft.drawString(rl, mid + 4, y, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(fmtKwh(rv), w - 8, y, 1);
  };
  putPair(todayY + 16, "PV", h.today_pv, th().pv, "Load", h.today_load, th().text);
  putPair(todayY + 32, "Grid In", h.today_gi, th().grid, "Grid Out", h.today_ge, th().grid);
  if (!isLandscape(rot))
    putPair(todayY + 48, "Batt Chg", h.today_bc, th().charge, "Batt Dis", h.today_bd, th().disch);
  drawNav(tft, Page::History);
}

static void drawSettingsBtn(TFT_eSPI& tft, int x, int y, int bw, int bh, const char* label, uint16_t bg,
                            uint16_t fg) {
  tft.fillRoundRect(x, y, bw, bh, 5, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + bw / 2, y + bh / 2, 2);
}

static const char* rotationLabel(uint8_t r) {
  switch (r & 3) {
    case 1:
      return "Landscape";
    case 2:
      return "Portrait flip";
    case 3:
      return "Landscape flip";
    default:
      return "Portrait";
  }
}

static const char* kLayoutNames[] = {"Classic", "Compact", "Ring", "Bars", "Flow"};

void Ui::drawSettings(TFT_eSPI& tft, const HostSettings& s, bool wifiOk, const String& wifiSsid,
                      const String& statusMsg, SettingsTab tab) {
  if (tab == SettingsTab::Updates) {
    drawSettingsUpdates(tft, s, statusMsg, FW_VERSION);
    return;
  }
  drawSettingsConnection(tft, s, wifiOk, wifiSsid, statusMsg);
}

void Ui::drawSettingsConnection(TFT_eSPI& tft, const HostSettings& s, bool wifiOk, const String& wifiSsid,
                                 const String& statusMsg) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int cH = contentH(rot);
  int cardW = w - 16;
  tft.fillRect(0, 22, w, cH, th().bg);
  fillHeader(tft, "Settings", wifiOk ? "Connected" : "Offline", rot);

  tft.fillRect(0, 24, w / 2, 20, th().card);
  tft.fillRect(w / 2, 24, w / 2, 20, th().bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().card);
  tft.drawString("Connection", w / 4, 34, 1);
  tft.setTextColor(th().dim, th().bg);
  tft.drawString("Updates", 3 * w / 4, 34, 1);

  tft.fillRoundRect(8, 48, cardW, 68, 12, th().card);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().card);
  tft.drawString("HOST", 16, 54, 1);
  String ip = s.hostIp.length() ? s.hostIp : "(not set)";
  tft.setTextColor(th().text, th().card);
  tft.drawString(ip, 16, 68, 2);
  tft.setTextColor(th().muted, th().card);
  tft.drawString("Port " + String(s.hostPort), 16, 86, 1);
  String ssid = wifiSsid.length() ? wifiSsid : "(portal)";
  if (ssid.length() > 18) ssid = ssid.substring(0, 17) + ".";
  tft.drawString("WiFi " + ssid, 16, 100, 1);

  auto row = [&](int y, const char* label, const String& val) {
    tft.fillRoundRect(8, y, cardW, 28, 8, th().card);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(th().dim, th().card);
    tft.drawString(label, 16, y + 8, 1);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(th().grid, th().card);
    tft.drawString(val + "  >", w - 16, y + 8, 2);
  };

  row(122, "Rotation", rotationLabel(s.screenRotation));
  row(154, "Layout", kLayoutNames[s.glanceLayout]);
  row(186, "Theme", themeName(s.themeId));
  row(218, "Settings PIN", s.settingsPin.length() == 4 ? "****" : "Off");

  const int btnH = 24;
  drawSettingsBtn(tft, 8, 248, cardW / 2 - 4, btnH, "Find", th().card, th().grid);
  drawSettingsBtn(tft, 8 + cardW / 2 + 4, 248, cardW / 2 - 4, btnH, "Manual", th().card, th().pv);
  drawSettingsBtn(tft, 8, 276, cardW, btnH, "WiFi Setup", th().card, th().text);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(th().muted, th().bg);
  String st = statusMsg;
  if (st.length() > 36) st = st.substring(0, 35) + ".";
  tft.drawString(st, w / 2, navY(rot) - 36, 1);
  drawNav(tft, Page::Settings);
}

void Ui::drawSettingsUpdates(TFT_eSPI& tft, const HostSettings& s, const String& otaStatus,
                             const String& fwVersion) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int cH = contentH(rot);
  int cardW = w - 16;
  tft.fillRect(0, 22, w, cH, th().bg);
  fillHeader(tft, "Settings", "Updates", rot);

  tft.fillRect(0, 24, w / 2, 20, th().bg);
  tft.fillRect(w / 2, 24, w / 2, 20, th().card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().dim, th().bg);
  tft.drawString("Connection", w / 4, 34, 1);
  tft.setTextColor(th().text, th().card);
  tft.drawString("Updates", 3 * w / 4, 34, 1);

  tft.fillRoundRect(8, 48, cardW, 80, 12, th().card);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().card);
  tft.drawString("FIRMWARE", 16, 56, 1);
  tft.setTextColor(th().text, th().card);
  tft.drawString("Version " + fwVersion, 16, 72, 2);
  tft.setTextColor(th().muted, th().card);
  tft.drawString(otaStatus, 16, 94, 1);

  auto toggle = [&](int y, const char* label, bool on) {
    tft.fillRoundRect(8, y, cardW, 28, 8, th().card);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(th().dim, th().card);
    tft.drawString(label, 16, y + 8, 2);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(on ? th().charge : th().muted, th().card);
    tft.drawString(on ? "ON" : "OFF", w - 16, y + 8, 2);
  };

  toggle(140, "Check for updates", s.checkForUpdate);
  toggle(172, "Auto install", s.autoInstallUpdate);
  toggle(204, "Grid offline alert", s.gridOfflineAlert);
  toggle(236, "Sync from host", s.useHostConfig);

  drawNav(tft, Page::Settings);
}

void Ui::drawPickList(TFT_eSPI& tft, const char* title, const char* const* names, int count, int selected) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int h = scrH(rot);
  tft.fillScreen(th().bg);
  fillHeader(tft, title, "Pick", rot);
  const int itemH = 36;
  int y = 50;
  for (int i = 0; i < count; i++) {
    uint16_t bg = (i == selected) ? th().grid : th().card;
    uint16_t fg = (i == selected) ? th().text : th().muted;
    tft.fillRoundRect(8, y, w - 16, itemH - 4, 6, bg);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(names[i], 20, y + (itemH - 4) / 2, 2);
    y += itemH;
    if (y > navY(rot) - 40) break;
  }
  tft.fillRoundRect(8, h - 40, w - 16, 30, 6, th().panel);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().panel);
  tft.drawString("BACK", w / 2, h - 25, 2);
}

void Ui::drawHostChoice(TFT_eSPI& tft, const String& wifiSsid) {
  int w = scrW(rotation_);
  int h = scrH(rotation_);
  tft.fillScreen(th().bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().pv, th().bg);
  tft.drawString("Solar Display", w / 2, 36, 4);
  tft.setTextColor(th().text, th().bg);
  tft.drawString("How to find your host?", w / 2, 72, 2);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("WiFi: " + (wifiSsid.length() ? wifiSsid : "(not connected)"), w / 2, 96, 1);

  tft.fillRoundRect(10, 120, w - 20, 72, 6, th().grid);
  tft.setTextColor(th().text, th().grid);
  tft.drawString("SEARCH NETWORK", w / 2, 148, 2);
  tft.drawString("auto-detect host", w / 2, 168, 1);

  tft.fillRoundRect(10, 210, w - 20, 72, 6, th().warn);
  tft.setTextColor(th().onAccent, th().warn);
  tft.drawString("ENTER IP MANUALLY", w / 2, 238, 2);
  tft.drawString("type address with +/-", w / 2, 258, 1);
}

void Ui::drawFindingHost(TFT_eSPI& tft, const String& statusMsg) {
  int w = scrW(rotation_);
  tft.fillScreen(th().bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().pv, th().bg);
  tft.drawString("Finding host", w / 2, 80, 4);
  tft.setTextColor(th().text, th().bg);
  tft.drawString("Looking for solar-monitoring", w / 2, 120, 2);
  tft.drawString("on your network...", w / 2, 140, 2);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString(statusMsg, w / 2, 170, 2);

  tft.fillRoundRect(10, 210, w - 20, 50, 4, th().panel);
  tft.setTextColor(th().text, th().panel);
  tft.drawString("CANCEL", w / 2, 235, 2);
}

void Ui::drawManualHost(TFT_eSPI& tft, const uint8_t octets[4], uint8_t selectedOctet, uint16_t port,
                        const String& statusMsg) {
  int w = scrW(rotation_);
  fillHeader(tft, "Host IP", "Manual", rotation_);
  const int octX[4] = {8, 66, 124, 182};
  for (int i = 0; i < 4; i++) {
    uint16_t bg = (i == selectedOctet) ? th().grid : th().panel;
    tft.fillRoundRect(octX[i], 52, 48, 40, 4, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th().text, bg);
    tft.drawString(String(octets[i]), octX[i] + 24, 72, 4);
    if (i < 3) {
      tft.setTextColor(th().muted, th().bg);
      tft.drawString(".", octX[i] + 54, 72, 4);
    }
  }

  tft.fillRoundRect(15, 110, 60, 40, 4, th().panel);
  tft.fillRoundRect(w - 75, 110, 60, 40, 4, th().panel);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().panel);
  tft.drawString("-", 45, 130, 4);
  tft.drawString("+", w - 45, 130, 4);

  tft.setTextColor(th().muted, th().bg);
  tft.drawString("Port", w / 2, 158, 2);
  tft.fillRoundRect(15, 165, 60, 30, 4, th().panel);
  tft.fillRoundRect(w - 75, 165, 60, 30, 4, th().panel);
  tft.setTextColor(th().text, th().panel);
  tft.drawString("-", 45, 180, 2);
  tft.drawString("+", w - 45, 180, 2);
  tft.setTextColor(th().text, th().bg);
  tft.drawString(String(port), w / 2, 180, 4);

  tft.fillRoundRect(10, 205, (w - 30) / 2, 34, 4, th().grid);
  tft.fillRoundRect(w / 2 + 5, 205, (w - 30) / 2, 34, 4, th().charge);
  tft.setTextColor(th().text, th().grid);
  tft.drawString("TEST", 10 + (w - 30) / 4, 222, 2);
  tft.setTextColor(th().onAccent, th().charge);
  tft.drawString("SAVE", w / 2 + 5 + (w - 30) / 4, 222, 2);

  tft.fillRoundRect(10, 248, w - 20, 34, 4, th().panel);
  tft.setTextColor(th().text, th().panel);
  tft.drawString("BACK", w / 2, 265, 2);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString(statusMsg, w / 2, scrH(rotation_) - 25, 1);
}

static void drawKeyRow(TFT_eSPI& tft, int y, int w, const char* keys, int count, bool shift, uint16_t bg,
                       uint16_t fg) {
  const int gap = 2;
  const int margin = 4;
  int total = w - margin * 2 - gap * (count - 1);
  int kw = total / count;
  for (int i = 0; i < count; i++) {
    int x = margin + i * (kw + gap);
    tft.fillRoundRect(x, y, kw, 24, 4, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    char label[2] = {0, 0};
    char c = keys[i];
    if (c >= 'a' && c <= 'z' && shift) c = (char)(c - 32);
    label[0] = c;
    tft.drawString(label, x + kw / 2, y + 12, 2);
  }
}

static void drawWideKey(TFT_eSPI& tft, int x, int y, int kw, int kh, const char* label, uint16_t bg,
                        uint16_t fg) {
  tft.fillRoundRect(x, y, kw, kh, 4, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + kw / 2, y + kh / 2, 2);
}

void Ui::drawWifiNetworks(TFT_eSPI& tft, const std::vector<WifiNetwork>& nets, int scroll, int selected,
                          const String& status) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int ny = navY(rot);
  const int itemH = 32;
  const int visible = 5;
  tft.fillScreen(th().bg);
  fillHeader(tft, "WiFi", "Touch to pick", rot);

  if (nets.empty()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th().muted, th().bg);
    tft.drawString("No networks found", w / 2, 110, 2);
    tft.drawString("Tap Rescan", w / 2, 136, 2);
  } else {
    int y = 48;
    for (int i = 0; i < visible; i++) {
      int idx = scroll + i;
      if (idx >= (int)nets.size()) break;
      const WifiNetwork& n = nets[idx];
      bool sel = idx == selected;
      uint16_t bg = sel ? th().grid : th().card;
      uint16_t fg = sel ? th().text : th().muted;
      tft.fillRoundRect(8, y, w - 16, itemH - 4, 6, bg);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(fg, bg);
      String label = n.ssid;
      if (label.length() > 22) label = label.substring(0, 21) + ".";
      tft.drawString(label, 16, y + 6, 2);
      tft.setTextDatum(TR_DATUM);
      String meta = n.secure ? "lock " : "open ";
      meta += String(n.rssi) + " dBm";
      tft.drawString(meta, w - 16, y + 8, 1);
      y += itemH;
    }
    if ((int)nets.size() > visible) {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(th().dim, th().bg);
      tft.drawString(String(scroll + 1) + "-" + String(min(scroll + visible, (int)nets.size())) + " / " +
                         String(nets.size()),
                     w / 2, ny - 78, 1);
    }
  }

  int btnY = ny - 66;
  drawWideKey(tft, 8, btnY, (w - 20) / 2, 28, "Rescan", th().card, th().grid);
  drawWideKey(tft, 12 + (w - 20) / 2, btnY, (w - 20) / 2, 28, "Phone", th().card, th().pv);
  drawWideKey(tft, 8, btnY + 34, w - 16, 28, "Back", th().panel, th().text);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(th().muted, th().bg);
  String st = status;
  if (st.length() > 34) st = st.substring(0, 33) + ".";
  tft.drawString(st, w / 2, btnY - 10, 1);
}

void Ui::drawWifiPassword(TFT_eSPI& tft, const String& ssid, const String& password, bool showPass, bool shift,
                          const String& status) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int ny = navY(rot);
  tft.fillScreen(th().bg);
  fillHeader(tft, "Password", shift ? "ABC" : "abc", rot);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().bg);
  tft.drawString("Network", 12, 28, 1);
  tft.setTextColor(th().text, th().bg);
  String s = ssid;
  if (s.length() > 28) s = s.substring(0, 27) + ".";
  tft.drawString(s, 12, 42, 2);

  tft.fillRoundRect(8, 62, w - 16, 28, 6, th().card);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().text, th().card);
  String shown = password;
  if (!showPass && shown.length()) {
    shown = "";
    for (size_t i = 0; i < password.length(); i++) shown += "*";
  }
  if (shown.length() > 26) shown = shown.substring(0, 25) + ".";
  if (shown.length() == 0) shown = "tap keys below";
  tft.drawString(shown, 16, 70, 2);

  const int rowY0 = 98;
  const int rowStep = 28;
  drawKeyRow(tft, rowY0, w, "1234567890", 10, false, th().panel, th().text);
  drawKeyRow(tft, rowY0 + rowStep, w, "qwertyuiop", 10, shift, th().panel, th().text);
  drawKeyRow(tft, rowY0 + rowStep * 2, w, "asdfghjkl", 9, shift, th().panel, th().text);

  const int gap = 2;
  const int margin = 4;
  int y3 = rowY0 + rowStep * 3;
  int zCount = 8;
  int totalZ = w - margin * 2 - gap * (zCount);
  int kw = totalZ / zCount;
  const char* zrow = "zxcvbnm";
  for (int i = 0; i < 7; i++) {
    int x = margin + i * (kw + gap);
    char lbl[2] = {zrow[i], 0};
    if (shift && lbl[0] >= 'a' && lbl[0] <= 'z') lbl[0] = (char)(lbl[0] - 32);
    drawWideKey(tft, x, y3, kw, 24, lbl, th().panel, th().text);
  }
  drawWideKey(tft, margin + 7 * (kw + gap), y3, kw, 24, "Del", th().warn, th().onAccent);

  int y4 = rowY0 + rowStep * 4;
  drawWideKey(tft, margin, y4, kw, 24, shift ? "abc" : "ABC", th().card, th().grid);
  drawWideKey(tft, margin + (kw + gap), y4, kw, 24, ".", th().panel, th().text);
  drawWideKey(tft, margin + 2 * (kw + gap), y4, kw, 24, "-", th().panel, th().text);
  drawWideKey(tft, margin + 3 * (kw + gap), y4, kw, 24, "_", th().panel, th().text);
  drawWideKey(tft, margin + 4 * (kw + gap), y4, kw, 24, "@", th().panel, th().text);
  int spaceW = w - margin * 2 - 5 * (kw + gap) - kw - gap;
  drawWideKey(tft, margin + 5 * (kw + gap), y4, spaceW, 24, "space", th().panel, th().text);
  drawWideKey(tft, margin + 5 * (kw + gap) + spaceW + gap, y4, kw, 24, showPass ? "Hide" : "Show", th().card,
              th().muted);

  int y5 = ny - 34;
  drawWideKey(tft, 8, y5, (w - 20) / 2, 28, "Back", th().panel, th().text);
  drawWideKey(tft, 12 + (w - 20) / 2, y5, (w - 20) / 2, 28, "Connect", th().charge, th().onAccent);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(th().muted, th().bg);
  String st = status;
  if (st.length() > 34) st = st.substring(0, 33) + ".";
  tft.drawString(st, w / 2, y5 - 10, 1);
}

static bool hitKeyRow(int16_t x, int16_t y, int rowY, int w, const char* keys, int count, bool shift, char& outChar) {
  const int gap = 2;
  const int margin = 4;
  if (y < rowY || y > rowY + 24) return false;
  int total = w - margin * 2 - gap * (count - 1);
  int kw = total / count;
  for (int i = 0; i < count; i++) {
    int kx = margin + i * (kw + gap);
    if (x >= kx && x <= kx + kw) {
      char c = keys[i];
      if (c >= 'a' && c <= 'z' && shift) c = (char)(c - 32);
      outChar = c;
      return true;
    }
  }
  return false;
}

bool Ui::wifiNetworkAt(int16_t x, int16_t y, int scroll, int count, int& index, bool& rescan, bool& phone,
                       bool& back, uint8_t rotation) {
  rescan = phone = back = false;
  int w = scrW(rotation);
  int ny = navY(rotation);
  const int itemH = 32;
  const int visible = 5;
  int btnY = ny - 66;

  if (y >= btnY && y <= btnY + 28) {
    if (x >= 8 && x <= 8 + (w - 20) / 2) {
      rescan = true;
      return true;
    }
    if (x >= 12 + (w - 20) / 2 && x <= w - 8) {
      phone = true;
      return true;
    }
  }
  if (y >= btnY + 34 && y <= btnY + 62 && x >= 8 && x <= w - 8) {
    back = true;
    return true;
  }
  if (x < 8 || x > w - 8 || y < 48) return false;
  int rel = (y - 48) / itemH;
  if (rel < 0 || rel >= visible) return false;
  index = scroll + rel;
  if (index < 0 || index >= count) return false;
  return true;
}

bool Ui::wifiPasswordAt(int16_t x, int16_t y, bool shift, char& outChar, bool& backspace, bool& shiftKey, bool& space,
                          bool& showPass, bool& connect, bool& back, uint8_t rotation) {
  outChar = 0;
  backspace = shiftKey = space = showPass = connect = back = false;
  int w = scrW(rotation);
  int ny = navY(rotation);
  const int rowY0 = 98;
  const int rowStep = 28;
  const int gap = 2;
  const int margin = 4;
  int zCount = 8;
  int totalZ = w - margin * 2 - gap * (zCount - 1);
  int kw = totalZ / zCount;

  if (hitKeyRow(x, y, rowY0, w, "1234567890", 10, false, outChar)) return true;
  if (hitKeyRow(x, y, rowY0 + rowStep, w, "qwertyuiop", 10, shift, outChar)) return true;
  if (hitKeyRow(x, y, rowY0 + rowStep * 2, w, "asdfghjkl", 9, shift, outChar)) return true;

  int y3 = rowY0 + rowStep * 3;
  if (y >= y3 && y <= y3 + 24) {
    for (int i = 0; i < 7; i++) {
      int kx = margin + i * (kw + gap);
      if (x >= kx && x <= kx + kw) {
        outChar = "zxcvbnm"[i];
        if (shift && outChar >= 'a' && outChar <= 'z') outChar = (char)(outChar - 32);
        return true;
      }
    }
    int delX = margin + 7 * (kw + gap);
    if (x >= delX && x <= delX + kw) {
      backspace = true;
      return true;
    }
  }

  int y4 = rowY0 + rowStep * 4;
  if (y >= y4 && y <= y4 + 24) {
    if (x >= margin && x <= margin + kw) {
      shiftKey = true;
      return true;
    }
    const char* syms = ".-_@";
    for (int i = 0; i < 4; i++) {
      int kx = margin + (i + 1) * (kw + gap);
      if (x >= kx && x <= kx + kw) {
        outChar = syms[i];
        return true;
      }
    }
    int spaceW = w - margin * 2 - 5 * (kw + gap) - kw - gap;
    int spaceX = margin + 5 * (kw + gap);
    if (x >= spaceX && x <= spaceX + spaceW) {
      space = true;
      return true;
    }
    int showX = spaceX + spaceW + gap;
    if (x >= showX && x <= showX + kw) {
      showPass = true;
      return true;
    }
  }

  int y5 = ny - 34;
  if (y >= y5 && y <= y5 + 28) {
    if (x >= 8 && x <= 8 + (w - 20) / 2) {
      back = true;
      return true;
    }
    if (x >= 12 + (w - 20) / 2 && x <= w - 8) {
      connect = true;
      return true;
    }
  }
  return false;
}

void Ui::drawPinPad(TFT_eSPI& tft, const char* title, const String& entry, const String& subtitle,
                    const String& status) {
  uint8_t rot = rotation_;
  int w = scrW(rot);
  int ny = navY(rot);
  tft.fillScreen(th().bg);
  fillHeader(tft, title, subtitle, rot);

  String dots;
  for (int i = 0; i < 4; i++) {
    dots += (i < (int)entry.length()) ? " * " : " o ";
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().bg);
  tft.drawString(dots, w / 2, 58, 4);

  const int cols = 3;
  const int gap = 6;
  const int margin = 16;
  const int kw = (w - margin * 2 - gap * (cols - 1)) / cols;
  const int kh = 36;
  const char* keys = "123456789";
  int y0 = 88;
  for (int i = 0; i < 9; i++) {
    int row = i / 3;
    int col = i % 3;
    int x = margin + col * (kw + gap);
    int y = y0 + row * (kh + gap);
    char lbl[2] = {keys[i], 0};
    drawWideKey(tft, x, y, kw, kh, lbl, th().panel, th().text);
  }
  int y3 = y0 + 3 * (kh + gap);
  drawWideKey(tft, margin, y3, kw, kh, "Back", th().card, th().muted);
  drawWideKey(tft, margin + (kw + gap), y3, kw, kh, "0", th().panel, th().text);
  drawWideKey(tft, margin + 2 * (kw + gap), y3, kw, kh, "Del", th().warn, th().onAccent);

  drawWideKey(tft, margin, y3 + kh + gap, w - margin * 2, kh, "OK", th().charge, th().onAccent);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(th().muted, th().bg);
  String st = status;
  if (st.length() > 34) st = st.substring(0, 33) + ".";
  tft.drawString(st, w / 2, ny - 12, 1);
}

bool Ui::pinPadAt(int16_t x, int16_t y, int& digit, bool& del, bool& ok, bool& back, uint8_t rotation) {
  digit = -1;
  del = ok = back = false;
  int w = scrW(rotation);
  const int cols = 3;
  const int gap = 6;
  const int margin = 16;
  const int kw = (w - margin * 2 - gap * (cols - 1)) / cols;
  const int kh = 36;
  const char* keys = "123456789";
  int y0 = 88;
  for (int i = 0; i < 9; i++) {
    int row = i / 3;
    int col = i % 3;
    int kx = margin + col * (kw + gap);
    int ky = y0 + row * (kh + gap);
    if (x >= kx && x <= kx + kw && y >= ky && y <= ky + kh) {
      digit = keys[i] - '0';
      return true;
    }
  }
  int y3 = y0 + 3 * (kh + gap);
  if (y >= y3 && y <= y3 + kh) {
    if (x >= margin && x <= margin + kw) {
      back = true;
      return true;
    }
    if (x >= margin + (kw + gap) && x <= margin + 2 * (kw + gap)) {
      digit = 0;
      return true;
    }
    if (x >= margin + 2 * (kw + gap) && x <= margin + 3 * (kw + gap)) {
      del = true;
      return true;
    }
  }
  int okY = y3 + kh + gap;
  if (y >= okY && y <= okY + kh && x >= margin && x <= w - margin) {
    ok = true;
    return true;
  }
  return false;
}
