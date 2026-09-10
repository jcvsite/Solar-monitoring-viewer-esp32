#pragma once
#include <stdint.h>
#include "config.h"

inline int scrW(uint8_t rotation) {
  return (rotation & 1) ? 320 : 240;
}

inline int scrH(uint8_t rotation) {
  return (rotation & 1) ? 240 : 320;
}

inline bool isLandscape(uint8_t rotation) { return (rotation & 1) != 0; }

inline int navY(uint8_t rotation) { return scrH(rotation) - UI_NAV_H; }

inline int contentH(uint8_t rotation) { return navY(rotation) - 22; }

inline int contentW(uint8_t rotation) { return scrW(rotation); }

inline const char* const* layoutNames() {
  static const char* kNames[] = {"Classic", "Compact", "Ring", "Bars", "Flow"};
  return kNames;
}

inline const char* layoutName(uint8_t id) {
  if (id > 4) id = 0;
  return layoutNames()[id];
}

inline const char* rotationLabel(uint8_t r) {
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
