# Changelog

All notable changes to Solar Monitoring Viewer (ESP32) will be documented in this file.

## [Unreleased]

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
