# Configuration — Solar Monitoring Viewer

## Host (solar-monitoring)

On the PC running solar-monitoring, configure `config.ini`:

```ini
[WEB_DASHBOARD]
ENABLE_WEB_DASHBOARD = True
WEB_DASHBOARD_PORT = 8081
ENABLE_MDNS = true
# DISPLAY_API_TOKEN =          ; optional LAN auth
# DISPLAY_GITHUB_TOKEN =         ; for private repo OTA proxy
```

Global display preferences live in **`display_config.json`** at the solar-monitoring project root, editable from the web dashboard **Settings → ESP32 display**. Optional **`settings_pin`** (4 digits) locks display settings on the web dashboard, device Settings tab, and device web UI.

## Device NVS keys (`solar_disp`)

| Key | Field |
|-----|-------|
| `host`, `port`, `token`, `poll`, `bri`, `rot` | Connection |
| `layout`, `theme`, `grid_alert`, `use_host`, `setPin` | Appearance / PIN |
| `chk_upd`, `auto_upd`, `cfg_rev` | OTA / host sync |

Settings survive firmware OTA (NVS is not erased).

## Touch tuning

Edit `include/config.h` — `TOUCH_MAP_*`, `TOUCH_MIRROR_X/Y`, `TOUCH_SWAP_XY` for non-CYD boards.

## API contract

Device polls:

- `GET /api/display` — glance data
- `GET /api/display/config` — global settings (when use host settings)
- `GET /api/display/firmware/latest.bin` — OTA binary (via host proxy)

See [solar-monitoring CONFIGURATION.md](https://github.com/jcvsite/solar-monitoring/blob/main/CONFIGURATION.md).
