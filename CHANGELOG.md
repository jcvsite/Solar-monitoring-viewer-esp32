# Changelog

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
