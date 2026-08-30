#pragma once
#include <TFT_eSPI.h>
#include "api_client.h"

void drawGlanceLayout(TFT_eSPI& tft, uint8_t layoutId, uint8_t rotation, const GlanceData& g,
                      bool stale, uint32_t animMs, bool gridAlert);
void drawGlanceGridPulseLayout(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g, uint32_t animMs,
                               bool gridAlert);
void drawGlanceHeaderOnly(TFT_eSPI& tft, uint8_t rotation, const GlanceData& g);
