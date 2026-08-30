# Contributing

1. Fork and clone [Solar-monitoring-viewer-esp32](https://github.com/jcvsite/Solar-monitoring-viewer-esp32).
2. Install [PlatformIO](https://platformio.org/).
3. Build: `pio run -e cyd_esp32`
4. Flash: `pio run -t upload --upload-port COMx -e cyd_esp32`

## Releases

Tag semver releases: `v0.2.0`, `v0.3.0`, …  
CI uploads `solar-viewer-cyd_esp32-vX.Y.Z.bin` to GitHub Releases for OTA via the solar-monitoring host proxy.

## Host integration

API changes must stay compatible with [solar-monitoring](https://github.com/jcvsite/solar-monitoring) `services/display_api.py`.
