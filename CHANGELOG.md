# Changelog

## [Unreleased]

## [0.3.3] - 2026-09-11

### Added
- **Night mode**: schedule start/end hour and night brightness from Settings (and device web UI).
- On-device **Theme** and **Layout** pickers with Back navigation.
- HostChoice **Skip for now**; WiFi password keyboard with discoverable ABC/abc/1# modes.

### Changed
- Firmware version **0.3.3** (platformio.ini, webflash manifest).
- Host config sync applies visual prefs (theme/layout/rotation/brightness) only when `config_rev` increases.
- Taller nav/settings touch targets (36px); Classic Glance cards shortened so Inv/Batt temps clear the nav.
- Lean-up: remove unused Montserrat 60 font / dead logo TU; shared layout/theme/rotation name helpers.

### Fixed
- Theme/layout Settings rows corrupted NVS (`index = -1`) and never opened the picker; theme changes now rebuild Glance/shell so Home repaints.
- LVGL active-screen delete crashes on setup/PIN screens; Glance `gridPill` double-free after shell teardown.
- Settings tab taps interrupted by glance redraw; swipe exclusion over the tab bar.
- Settings freeze after toggles (e.g. Grid alert / Check now): host config + OTA work no longer block the UI thread; host prefs only sync on `config_rev` bump; OTA status updates the label without rebuilding the tabview.
- WiFi boot stuck on splash / SoftAP without UI tick; reconnect backoff after drops.
- Landscape History/BMS clipping; PIN pad overlapping Back; label truncation on SSID/host/metrics.

## [0.3.2] - 2026-09-07

### Added
- Semver OTA gating: Check now / auto-install only flash when remote version is newer.
- Host force-update can target `force_update_version`.

### Changed
- Firmware version **0.3.2**.
- **OTA is host-independent**: device fetches GitHub releases over HTTPS directly (no solar-monitoring proxy / `urlopen`).

### Fixed
- Avoid silent downgrade when GitHub latest is older than the flashed build.
- Update failures caused by host PC GitHub/SSL errors.


### Added
- **Host poll worker** (host_poll): FreeRTOS task fetches Glance/BMS/History off the UI thread.
- **SOC fonts**: custom lv_font_soc_40/48/73 for Classic and landscape glance layouts.
- **`battery_charge_state`**: glance API field drives charge / discharge / float animation (matches solar-monitoring display API).

### Changed
- Firmware version **0.3.2** (platformio.ini, webflash manifest).
- Glance / BMS / shell / touch / LVGL port polish for landscape and Classic layout.

### Fixed
- Battery flow direction prefers host `battery_charge_state` over power-sign guesses.

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