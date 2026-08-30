# Changelog

## [0.3.1] - 2026-08-31

### Added
- **Classic Glance layout polish**: large white SOC beside battery icon with auto-sized Montserrat font (48→32→28→20); centered battery+SOC cluster; battery terminal cap on top; animated fill when charging/discharging.
- **Caption icons** on PV / Load / Grid metric rows; right-aligned power values.
- **Icon-only bottom nav** (Home, BMS, History, Settings) with per-tab accent color and active dot indicator.
- **BMS cell bars**: color-coded voltages, min/max markers, centered Montserrat 14 labels (`3.14 V` / `3.1 V` on narrow cells).
- **History**: compact chart (¼ content height); colorful icons on today’s stat rows.
- **Settings**: scrollable panels with themed icons on rows, tabs, and HOST/Firmware cards.
- **Grid offline alert**: red **NO GRID** text blink every 5 s (replaces static bar).

### Changed
- Removed redundant inverter status line from Classic Glance (grid chip is sufficient).
- Flash usage ~**81%**, RAM ~**32%** on CYD (4 MB) after Montserrat 48 + UI polish.

## [0.3.0] - 2026-08-30

### Added
- Full **LVGL 8.x** UI: card-based Glance (5 layouts), BMS cell grid, History chart, Settings tabs, WiFi keyboard, PIN pad, splash with logo image descriptor.
- Modular `src/ui_lv/` screen layer with theme-mapped styles and event-driven touch (replaces manual hit boxes).

### Changed
- Removed legacy TFT_eSPI immediate-mode UI (`ui.cpp`, `glance_layouts.cpp`).
- Build: LVGL + partial framebuffer; **Flash ~69%**, **RAM ~32%** on CYD (4 MB).

## [0.2.1] - 2026-08-30

### Changed
- Native 192×192 logo embed (unmodified RGB565 from host icon).

## [0.2.0] - 2026-08-30

### Added
- Tabbed Settings, host config sync, OTA via GitHub releases, device web UI, grid offline alert, 5 themes, 5 glance layouts, WiFi touch setup, settings PIN.
