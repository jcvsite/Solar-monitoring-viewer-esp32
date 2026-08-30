# Changelog

All notable changes to [Solar Monitoring Viewer (ESP32)](https://github.com/jcvsite/Solar-monitoring-viewer-esp32) are documented here.

## [Unreleased]

## [0.2.1] - 2026-08-30

### Added
- **Solar Monitoring logo** on boot splash and WiFi setup (from `static/icons/icon-192x192.png` via `scripts/png_to_logo_header.py`).
- **On-device WiFi setup**: touch network list, on-screen password keyboard, and **Phone** fallback to the SoftAP portal.
- **Optional 4-digit settings PIN** — locks the Settings tab and device web UI; syncs from host `display_config.json` when **Sync from host** is enabled.
- **Cheap Yellow Display (CYD)** hardware documentation: ESP32-2432S028R, 2.8″ ILI9341 240×320, XPT2046 touch.

### Changed
- Improved mobile styling for the WiFiManager captive portal (SoftAP fallback).
- Settings layout adjusted for PIN row; Find / Manual / WiFi buttons repositioned.
- Firmware version `0.2.1`.

## [0.2.0] - 2026-08-30

### Added
- Tabbed Settings: **Connection** (WiFi, host, layout, theme, rotation) and **Updates** (OTA check, auto-install, grid alert).
- Host config sync via `GET /api/display/config` when **Use host settings** is enabled.
- OTA firmware updates via solar-monitoring host proxy (`/api/display/firmware/latest.bin`).
- Device web UI on port 80 (fallback configuration).
- **5 glance layouts** (Classic, Compact, Ring, Bars, Flow) with portrait and landscape support.
- **5 themes** (Dark, Light, Solar, Ocean, Forest).
- Enhanced **grid offline alert** (header + NO GRID blink every 5 s, user toggle).
- BMS cell strip: console-matched voltage colors, vertical labels, min/max arrows; hidden when no cell data.
- Branded boot splash: **Solar Monitoring Viewer**.
- mDNS discovery of solar-monitoring host; weather, clock, inverter/battery temps on Glance.

### Changed
- Firmware version `0.2.0`; screen rotation supports all four orientations (0–3).
