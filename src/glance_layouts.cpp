#include "glance_layouts.h"
#include "layout.h"
#include "theme.h"
#include "config.h"
#include <cstdio>
#include <math.h>
#include <time.h>

namespace {

const ThemePalette& th() { return themeActive(); }

String fmtPower(float w) {
  if (isnan(w)) return "--";
  float a = fabsf(w);
  char buf[16];
  if (a >= 1000.0f) snprintf(buf, sizeof(buf), "%.1f kW", a / 1000.0f);
  else snprintf(buf, sizeof(buf), "%.0f W", a);
  return String(buf);
}

String fmtKwh(float k) {
  if (isnan(k)) return "--";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", k);
  return String(buf);
}

uint16_t socColor(float soc) {
  if (isnan(soc)) return th().muted;
  if (soc < 20) return th().danger;
  if (soc < 40) return th().warn;
  return th().charge;
}

uint16_t gridStateColor(const String& state) {
  if (state == "import") return th().gridImport;
  if (state == "export") return th().gridExport;
  if (state == "offline" || state == "brownout") return th().danger;
  if (state == "unknown") return th().muted;
  return th().ok;
}

static String currentClockStr() {
  struct tm ti;
  if (!getLocalTime(&ti, 50)) return "--:--";
  char buf[16];
  int h = ti.tm_hour;
  int m = ti.tm_min;
  const char* ap = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12;
  if (h12 == 0) h12 = 12;
  snprintf(buf, sizeof(buf), "%d:%02d %s", h12, m, ap);
  return String(buf);
}

enum class WeatherIcon : uint8_t { ClearDay, ClearNight, PartlyCloudy, Cloudy, Rain, Snow, Thunder };

WeatherIcon weatherIconFor(int code, int isDay) {
  if (code <= 1) return isDay ? WeatherIcon::ClearDay : WeatherIcon::ClearNight;
  if (code == 2) return WeatherIcon::PartlyCloudy;
  if (code == 3 || (code >= 45 && code <= 48)) return WeatherIcon::Cloudy;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return WeatherIcon::Rain;
  if (code >= 71 && code <= 86) return WeatherIcon::Snow;
  if (code >= 95 && code <= 99) return WeatherIcon::Thunder;
  return WeatherIcon::Cloudy;
}

void drawCloud(TFT_eSPI& tft, int cx, int cy, uint16_t color) {
  tft.fillCircle(cx - 4, cy + 1, 4, color);
  tft.fillCircle(cx + 1, cy - 1, 5, color);
  tft.fillCircle(cx + 6, cy + 1, 4, color);
  tft.fillRect(cx - 7, cy + 1, 15, 4, color);
}

void drawWeatherIcon(TFT_eSPI& tft, int cx, int cy, WeatherIcon icon) {
  switch (icon) {
    case WeatherIcon::ClearDay:
      tft.fillCircle(cx, cy, 5, th().warn);
      break;
    case WeatherIcon::ClearNight:
      tft.fillCircle(cx + 1, cy, 4, th().muted);
      tft.fillCircle(cx + 4, cy - 2, 4, th().header);
      break;
    case WeatherIcon::PartlyCloudy:
      tft.fillCircle(cx - 5, cy - 3, 3, th().warn);
      drawCloud(tft, cx + 1, cy + 1, th().muted);
      break;
    case WeatherIcon::Cloudy:
      drawCloud(tft, cx, cy, th().muted);
      break;
    case WeatherIcon::Rain:
      drawCloud(tft, cx, cy - 2, th().muted);
      tft.drawFastVLine(cx - 3, cy + 4, 4, th().grid);
      tft.drawFastVLine(cx, cy + 5, 4, th().grid);
      tft.drawFastVLine(cx + 3, cy + 4, 4, th().grid);
      break;
    case WeatherIcon::Snow:
      drawCloud(tft, cx, cy - 2, th().muted);
      tft.drawPixel(cx - 3, cy + 5, th().text);
      tft.drawPixel(cx, cy + 6, th().text);
      tft.drawPixel(cx + 3, cy + 5, th().text);
      break;
    case WeatherIcon::Thunder:
      drawCloud(tft, cx, cy - 2, th().muted);
      tft.drawLine(cx + 1, cy + 3, cx - 1, cy + 6, th().warn);
      tft.drawLine(cx - 1, cy + 6, cx + 2, cy + 6, th().warn);
      break;
  }
}

void drawGlanceHeader(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g) {
  int w = scrW(rotation);
  tft.fillRect(0, 0, w, 22, th().header);
  tft.drawFastHLine(0, 22, w, th().line);

  String clockStr = g.clock.length() ? g.clock : currentClockStr();
  int rightX = w - 4;
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(th().text, th().header);
  tft.drawString(clockStr, rightX, 11, 2);
  int x = rightX - tft.textWidth(clockStr, 2) - 4;

  if (g.weather_enabled && !isnan(g.weather_temp)) {
    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%.0f%c", g.weather_temp, g.weather_unit);
    String temp = tbuf;
    tft.setTextDatum(MR_DATUM);
    tft.drawString(temp, x, 11, 1);
    x -= tft.textWidth(temp, 1) + 6;
    drawWeatherIcon(tft, x - 12, 11, weatherIconFor(g.weather_code, g.weather_is_day));
    x -= 20;
  }

  tft.setTextDatum(ML_DATUM);
  String t = g.title;
  int maxW = (x - 8) > 40 ? (x - 8) : 40;
  while (t.length() > 1 && tft.textWidth(t, 2) > maxW) t.remove(t.length() - 1);
  if (t.length() < g.title.length()) t += ".";
  tft.drawString(t, 4, 11, 2);

  if (g.alerts) tft.fillCircle(w - 8, 11, 4, th().danger);
}

void drawMetricTile(TFT_eSPI& tft, int x, int y, int w, int h, const char* label, const String& value,
                    uint16_t accent) {
  tft.fillRoundRect(x, y, w, h, 10, th().card);
  tft.fillRoundRect(x + 6, y + 4, w - 12, 2, 1, accent);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().card);
  tft.drawString(label, x + 8, y + 10, 1);
  tft.setTextColor(th().text, th().card);
  tft.drawString(value, x + 8, y + 26, 2);
}

void drawVerticalFlowArrows(TFT_eSPI& tft, int cx, int cy, bool upward, uint16_t c) {
  for (int i = -1; i <= 1; i++) {
    int ax = cx + i * 7;
    if (upward) {
      tft.drawLine(ax, cy + 5, ax, cy - 2, c);
      tft.drawLine(ax, cy - 2, ax - 2, cy + 1, c);
      tft.drawLine(ax, cy - 2, ax + 2, cy + 1, c);
    } else {
      tft.drawLine(ax, cy - 5, ax, cy + 2, c);
      tft.drawLine(ax, cy + 2, ax - 2, cy - 1, c);
      tft.drawLine(ax, cy + 2, ax + 2, cy - 1, c);
    }
  }
}

void drawBatteryIconVertical(TFT_eSPI& tft, int x, int y, int w, int h, float soc, uint16_t fill,
                             bool charging, bool discharging) {
  const int tipH = 5;
  const int tipW = max(12, w - 8);
  const int tipX = x + (w - tipW) / 2;
  const int bodyH = h - tipH;
  const int by = y + tipH;
  const int pad = 4;
  const int ix = x + pad;
  const int iy = by + pad;
  const int innerW = w - pad * 2;
  const int innerH = bodyH - pad * 2;

  tft.fillRoundRect(tipX, y, tipW, tipH, 2, th().text);
  tft.drawRoundRect(x, by, w, bodyH, 4, th().text);
  tft.fillRoundRect(ix, iy, innerW, innerH, 3, th().panel);

  float pct = constrain(soc, 0.0f, 100.0f);
  int fillH = (int)(innerH * pct / 100.0f + 0.5f);
  if (pct > 0 && fillH < 2) fillH = 2;
  if (fillH > innerH) fillH = innerH;

  if (fillH > 0) {
    int fy = iy + innerH - fillH;
    if (fillH >= innerH - 1) tft.fillRoundRect(ix, iy, innerW, innerH, 3, fill);
    else {
      tft.fillRect(ix, fy, innerW, fillH, fill);
      tft.fillRect(ix, fy, innerW, min(3, fillH), fill);
    }
  }
  int cx = x + w / 2;
  int cy = by + bodyH / 2;
  if (charging) drawVerticalFlowArrows(tft, cx, cy, true, th().text);
  else if (discharging) drawVerticalFlowArrows(tft, cx, cy, false, th().text);
}

int drawSocPercent(TFT_eSPI& tft, int x, int y, float soc) {
  const uint8_t numFont = 8;
  tft.setTextDatum(TL_DATUM);
  if (isnan(soc)) {
    tft.setTextColor(th().text, th().bg);
    tft.drawString("--", x, y, numFont);
    return tft.fontHeight(numFont);
  }
  char num[8];
  snprintf(num, sizeof(num), "%.0f", soc);
  tft.setTextColor(th().text, th().bg);
  tft.drawString(num, x, y, numFont);
  int nw = tft.textWidth(num, numFont);
  int nh = tft.fontHeight(numFont);
  tft.setTextColor(th().muted, th().bg);
  tft.drawString("%", x + nw + 6, y + nh - 22, 4);
  return nh;
}

void drawSocBar(TFT_eSPI& tft, int x, int barY, int barW, float soc, uint16_t sc) {
  tft.drawRoundRect(x, barY, barW, 14, 5, th().line);
  tft.fillRoundRect(x + 2, barY + 2, barW - 4, 10, 4, th().panel);
  int fillW = (int)((barW - 4) * constrain(soc, 0.0f, 100.0f) / 100.0f);
  if (fillW > 0) tft.fillRoundRect(x + 2, barY + 2, fillW, 10, 4, sc);
}

int drawGridPill(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool gridAlert, int pillY) {
  if (!gridAlert) return pillY;
  bool badGrid = (g.grid_state == "offline" || g.grid_state == "brownout");
  if (!badGrid && g.grid_state != "import" && g.grid_state != "export") return pillY;

  int w = scrW(rotation);
  int pillW = isLandscape(rotation) ? 180 : 160;
  int pillX = (w - pillW) / 2;

  if (badGrid) {
    uint16_t c = (g.grid_state == "brownout") ? th().warn : th().danger;
    tft.fillRoundRect(pillX, pillY, pillW, 20, 10, c);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th().text, c);
    const char* msg = (g.grid_state == "brownout") ? "Grid Brownout" : "NO GRID";
    tft.drawString(msg, w / 2, pillY + 10, 1);
    return pillY + 24;
  }
  uint16_t c = gridStateColor(g.grid_state);
  const char* chip = (g.grid_state == "import") ? "Grid Import" : "Grid Export";
  int cw = isLandscape(rotation) ? 110 : 96;
  tft.fillRoundRect((w - cw) / 2, pillY, cw, 18, 9, th().card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(c, th().card);
  tft.drawString(chip, w / 2, pillY + 9, 1);
  return pillY + 22;
}

void drawStaleOverlay(TFT_eSPI& tft, int cx, int cy) {
  tft.fillRoundRect(cx - 90, cy - 15, 180, 30, 8, th().danger);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().danger);
  tft.drawString("No connection", cx, cy, 2);
}

void drawHBarMeter(TFT_eSPI& tft, int x, int y, int w, int h, const char* label, float val, float maxVal,
                   uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 4, th().card);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(th().dim, th().card);
  tft.drawString(label, x + 6, y + 2, 1);
  int barX = x + 6;
  int barY = y + 14;
  int barW = w - 12;
  int barH = h - 18;
  tft.fillRoundRect(barX, barY, barW, barH, 2, th().panel);
  if (!isnan(val) && maxVal > 0) {
    int fw = (int)(barW * constrain(fabsf(val) / maxVal, 0.0f, 1.0f));
    if (fw > 0) tft.fillRoundRect(barX, barY, fw, barH, 2, color);
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(th().text, th().card);
  tft.drawString(fmtPower(val), x + w - 6, y + 2, 1);
}

// --- Classic (layout 0) ---
void layoutClassic(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool stale, bool gridAlert) {
  int w = scrW(rotation);
  int cH = contentH(rotation);
  tft.fillRect(0, 22, w, cH, th().bg);
  drawGlanceHeader(tft, rotation, g);

  int pillY = 26;
  pillY = drawGridPill(tft, rotation, g, gridAlert, pillY);

  float soc = isnan(g.soc) ? 0 : constrain(g.soc, 0.0f, 100.0f);
  uint16_t sc = socColor(g.soc);
  bool charging = (!isnan(g.batt_w) && g.batt_w < -25) || g.batt_status.indexOf("Charg") >= 0;
  bool discharging = (!isnan(g.batt_w) && g.batt_w > 25) || g.batt_status.indexOf("Disch") >= 0;
  if (charging) sc = th().charge;
  if (discharging) sc = th().disch;

  const int rowY = pillY + 4;
  const int socH = drawSocPercent(tft, 8, rowY, g.soc);

  const int battW = 44;
  const int battH = socH;
  const int battX = w - battW - 10;
  drawBatteryIconVertical(tft, battX, rowY, battW, battH, soc, sc, charging, discharging);

  const int barY = rowY + socH + 10;
  int barFullW = w - 20;
  drawSocBar(tft, 10, barY, barFullW, soc, sc);

  String statusLine;
  if (charging) statusLine = "Charging ";
  else if (discharging) statusLine = "Discharging ";
  if (!isnan(g.batt_w)) statusLine += fmtPower(g.batt_w);
  else if (g.batt_status.length() && !statusLine.length()) statusLine = g.batt_status;

  if (statusLine.length()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(sc, th().bg);
    tft.drawString(statusLine, w / 2, barY + 22, 2);
  }

  bool badGrid = gridAlert && (g.grid_state == "offline" || g.grid_state == "brownout");
  int tileW = isLandscape(rotation) ? (w - 24) / 2 : 110;
  int tileH = isLandscape(rotation) ? 36 : 42;
  int gap = 8;
  int tileY = barY + (statusLine.length() ? 36 : 24);

  String gridVal = badGrid ? g.grid_label : fmtPower(g.grid_w);
  if (!badGrid && g.grid_state == "ok") gridVal = "Idle";
  uint16_t gridAccent = badGrid ? th().danger : gridStateColor(g.grid_state);

  if (isLandscape(rotation)) {
    drawMetricTile(tft, gap, tileY, tileW, tileH, "Solar", fmtPower(g.pv_w), th().pv);
    drawMetricTile(tft, gap + tileW + gap, tileY, tileW, tileH, "Home", fmtPower(g.load_w), th().text);
    drawMetricTile(tft, gap, tileY + tileH + gap, tileW, tileH, "Grid", gridVal, gridAccent);
    drawMetricTile(tft, gap + tileW + gap, tileY + tileH + gap, tileW, tileH, "Today",
                     fmtKwh(g.pv_today_kwh) + " kWh", th().pv);
  } else {
    drawMetricTile(tft, gap, tileY, tileW, tileH, "Solar", fmtPower(g.pv_w), th().pv);
    drawMetricTile(tft, gap + tileW + gap, tileY, tileW, tileH, "Home", fmtPower(g.load_w), th().text);
    drawMetricTile(tft, gap, tileY + tileH + gap, tileW, tileH, "Grid", gridVal, gridAccent);
    drawMetricTile(tft, gap + tileW + gap, tileY + tileH + gap, tileW, tileH, "Today",
                     fmtKwh(g.pv_today_kwh) + " kWh", th().pv);
    int tempY = tileY + tileH + gap + tileH + 8;
    auto drawTemp = [&](int x, int tw, const char* label, float tempC) {
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(th().dim, th().bg);
      String line = String(label) + " ";
      line += isnan(tempC) ? "--" : String(tempC, 0) + " C";
      tft.drawString(line, x + tw / 2, tempY, 1);
    };
    drawTemp(gap, tileW, "Inv", g.inv_temp_c);
    drawTemp(gap + tileW + gap, tileW, "Bat", g.batt_temp_c);
  }

  if (stale) drawStaleOverlay(tft, w / 2, rowY + socH / 2);
}

// --- Compact (layout 1) ---
void layoutCompact(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool stale, bool gridAlert) {
  int w = scrW(rotation);
  int cH = contentH(rotation);
  tft.fillRect(0, 22, w, cH, th().bg);
  drawGlanceHeader(tft, rotation, g);
  int y = drawGridPill(tft, rotation, g, gridAlert, 26);

  uint16_t sc = socColor(g.soc);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(sc, th().bg);
  String socLine = isnan(g.soc) ? "SOC --" : "SOC " + String((int)g.soc) + "%";
  tft.drawString(socLine, 8, y + 4, 4);

  int rowY = y + 28;
  int colW = (w - 16) / 4;
  const char* labels[] = {"PV", "Load", "Grid", "Today"};
  String vals[] = {fmtPower(g.pv_w), fmtPower(g.load_w), fmtPower(g.grid_w),
                   fmtKwh(g.pv_today_kwh) + "k"};
  uint16_t cols[] = {th().pv, th().text, gridStateColor(g.grid_state), th().pv};
  for (int i = 0; i < 4; i++) {
    int x = 8 + i * colW;
    tft.fillRoundRect(x, rowY, colW - 4, 52, 6, th().card);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(th().dim, th().card);
    tft.drawString(labels[i], x + (colW - 4) / 2, rowY + 6, 1);
    tft.setTextColor(cols[i], th().card);
    tft.drawString(vals[i], x + (colW - 4) / 2, rowY + 24, 2);
  }

  int barY = rowY + 60;
  drawSocBar(tft, 8, barY, w - 16, isnan(g.soc) ? 0 : g.soc, sc);

  if (!isLandscape(rotation)) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(th().dim, th().bg);
    String bt = isnan(g.batt_w) ? g.batt_status : fmtPower(g.batt_w);
    tft.drawString(bt, w / 2, barY + 22, 2);
  }
  if (stale) drawStaleOverlay(tft, w / 2, rowY + 26);
}

// --- Ring (layout 2) ---
void layoutRing(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool stale, bool gridAlert) {
  int w = scrW(rotation);
  int cH = contentH(rotation);
  tft.fillRect(0, 22, w, cH, th().bg);
  drawGlanceHeader(tft, rotation, g);
  int y = drawGridPill(tft, rotation, g, gridAlert, 26);

  float soc = isnan(g.soc) ? 0 : constrain(g.soc, 0.0f, 100.0f);
  uint16_t sc = socColor(g.soc);
  int cx = w / 2;
  int cy = y + (isLandscape(rotation) ? 55 : 75);
  int r = isLandscape(rotation) ? 42 : 52;
  tft.drawCircle(cx, cy, r, th().line);
  tft.drawCircle(cx, cy, r - 8, th().panel);
  int endAngle = (int)(360.0f * soc / 100.0f);
  if (endAngle > 0) tft.drawArc(cx, cy, r - 4, r - 12, 0, endAngle, sc, th().bg, false);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().bg);
  if (isnan(g.soc)) tft.drawString("--", cx, cy, 4);
  else tft.drawString(String((int)soc) + "%", cx, cy, 4);

  int cornerY = cy + r + 8;
  int hw = (w - 24) / 2;
  drawMetricTile(tft, 8, cornerY, hw, 36, "Solar", fmtPower(g.pv_w), th().pv);
  drawMetricTile(tft, 8 + hw + 8, cornerY, hw, 36, "Home", fmtPower(g.load_w), th().text);
  if (stale) drawStaleOverlay(tft, cx, cy);
}

// --- Bars (layout 3) ---
void layoutBars(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool stale, bool gridAlert) {
  int w = scrW(rotation);
  int cH = contentH(rotation);
  tft.fillRect(0, 22, w, cH, th().bg);
  drawGlanceHeader(tft, rotation, g);
  int y = drawGridPill(tft, rotation, g, gridAlert, 26);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(socColor(g.soc), th().bg);
  tft.drawString(isnan(g.soc) ? "Battery --" : "Battery " + String((int)g.soc) + "%", 8, y + 2, 2);

  float pmax = 5000.0f;
  if (!isnan(g.pv_w)) pmax = max(pmax, fabsf(g.pv_w));
  if (!isnan(g.load_w)) pmax = max(pmax, fabsf(g.load_w));
  if (!isnan(g.grid_w)) pmax = max(pmax, fabsf(g.grid_w));

  int barH = isLandscape(rotation) ? 32 : 38;
  int gap = 6;
  int by = y + 22;
  drawHBarMeter(tft, 8, by, w - 16, barH, "PV", g.pv_w, pmax, th().pv);
  drawHBarMeter(tft, 8, by + barH + gap, w - 16, barH, "Load", g.load_w, pmax, th().text);
  drawHBarMeter(tft, 8, by + 2 * (barH + gap), w - 16, barH, "Grid", g.grid_w, pmax,
                gridStateColor(g.grid_state));
  drawSocBar(tft, 8, by + 3 * (barH + gap), w - 16, isnan(g.soc) ? 0 : g.soc, socColor(g.soc));
  if (stale) drawStaleOverlay(tft, w / 2, by + barH);
}

// --- Flow (layout 4) ---
void layoutFlow(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, bool stale, bool gridAlert) {
  int w = scrW(rotation);
  int cH = contentH(rotation);
  tft.fillRect(0, 22, w, cH, th().bg);
  drawGlanceHeader(tft, rotation, g);
  int y = drawGridPill(tft, rotation, g, gridAlert, 26);

  int cx = w / 2;
  int homeY = y + (isLandscape(rotation) ? 50 : 70);
  int nodeR = isLandscape(rotation) ? 28 : 34;

  tft.fillRoundRect(cx - nodeR, homeY - 20, nodeR * 2, 40, 8, th().card);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, th().card);
  tft.drawString("Home", cx, homeY - 8, 1);
  tft.drawString(fmtPower(g.load_w), cx, homeY + 10, 2);

  int pvX = isLandscape(rotation) ? 40 : cx;
  int pvY = isLandscape(rotation) ? homeY : homeY - 55;
  int gridX = isLandscape(rotation) ? w - 40 : cx;
  int gridY = isLandscape(rotation) ? homeY : homeY + 55;

  tft.fillCircle(pvX, pvY, 22, th().pv);
  tft.setTextColor(th().onAccent, th().pv);
  tft.drawString("PV", pvX, pvY - 6, 1);
  tft.drawString(fmtPower(g.pv_w), pvX, pvY + 8, 1);

  uint16_t gc = gridStateColor(g.grid_state);
  tft.fillCircle(gridX, gridY, 22, gc);
  tft.setTextColor(th().text, gc);
  tft.drawString("Grid", gridX, gridY - 6, 1);
  tft.drawString(fmtPower(g.grid_w), gridX, gridY + 8, 1);

  tft.drawLine(pvX, pvY + (pvY < homeY ? 22 : -22), cx - nodeR + 4, homeY, th().dim);
  tft.drawLine(gridX, gridY + (gridY > homeY ? -22 : 22), cx + nodeR - 4, homeY, th().dim);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(socColor(g.soc), th().bg);
  String socStr = isnan(g.soc) ? "SOC --" : "SOC " + String((int)g.soc) + "%";
  tft.drawString(socStr, cx, homeY + 50, 2);

  if (stale) drawStaleOverlay(tft, cx, homeY);
}

}  // namespace

void drawGlanceHeaderOnly(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g) {
  drawGlanceHeader(tft, rotation, g);
}

void drawGlanceLayout(TFT_eSPI& tft, uint8_t layoutId, uint8_t rotation, const GlanceData& g, bool stale,
                      uint32_t animMs, bool gridAlert) {
  (void)animMs;
  layoutId = constrain(layoutId, (uint8_t)0, (uint8_t)4);
  switch (layoutId) {
    case 1:
      layoutCompact(tft, rotation, g, stale, gridAlert);
      break;
    case 2:
      layoutRing(tft, rotation, g, stale, gridAlert);
      break;
    case 3:
      layoutBars(tft, rotation, g, stale, gridAlert);
      break;
    case 4:
      layoutFlow(tft, rotation, g, stale, gridAlert);
      break;
    default:
      layoutClassic(tft, rotation, g, stale, gridAlert);
      break;
  }
}

void drawGlanceGridPulseLayout(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, uint32_t animMs,
                               bool gridAlert) {
  if (!gridAlert) return;
  if (g.grid_state != "offline" && g.grid_state != "brownout") return;
  int w = scrW(rotation);
  int y = 26;
  bool soft = ((animMs / 500) % 2) == 0;
  uint16_t c = (g.grid_state == "brownout") ? th().warn : th().danger;
  if (!soft) c = th().card;
  int pillW = isLandscape(rotation) ? 180 : 160;
  int pillX = (w - pillW) / 2;
  tft.fillRoundRect(pillX, y, pillW, 20, 10, c);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(th().text, c);
  const char* msg = (g.grid_state == "brownout") ? "Grid Brownout" : "NO GRID";
  tft.drawString(msg, w / 2, y + 10, 1);
}
