# esphome-hlk-ld6004

ESPHome external component for the Hi-Link HLK-LD6004 60 GHz radar (UART, TinyFrame, 115200 baud).
Modelled on ESPHome's `ld2450` component. Needs both UART pins (TX is used for config commands).

| Feature | LD6004 support |
|---|---|
| Binary sensors | `presence`, `moving_target`, `still_target`, per-zone presence (4) |
| Sensors | `target_count`, `moving_target_count`, `still_target_count`; per target (3): `x`, `y`, `z`, `distance`, `angle`, `speed` |
| Select | `sensitivity`, `trigger_speed`, `install_method` (top/side), `operating_mode`, `p20_output` (OUT pin mode) |
| Number | `delay_time`, `sleep_time`, `output_interval`, `dwell_lifecycle`, `z_min`, `z_max`, `zone` bounds (detection / interference / dwell, zones 1-4) |
| Button | `auto_interference`, `clear_interference`, `reset_detection_area`, `reset_unmanned`, `clear_dwell` |
| Text sensor | `version` |

Differences from the LD2450: a target counts as "moving" when its doppler index is non-zero (the radar has no
separate still/moving flag), `speed` is that raw doppler index, no per-zone target counts, no Bluetooth or
multi-target switch, no restart/factory-reset, and zones are 3D boxes (x/y/z min/max). Settings are read back
from the radar 3 s after boot. Changing a zone bound rewrites the whole zone in the radar's flash.

Protocol from [swherdman/HLK-LD6004](https://github.com/swherdman/HLK-LD6004). **Untested on hardware.**

```yaml
external_components:
  - source: github://davidcoulson/esphome-hlk-ld6004
    components: [hlk_ld6004]
```

## Wiring (XIAO ESP32-C6)

Module right-hand 5-pin header (silkscreen `3V3 GND RX TX OUT`), per the Hi-Link V1.0 datasheet:

| LD6004 | Connect to |
|---|---|
| 3V3 | **Dedicated 3.3 V regulator, ≥1 A** (see below), not the XIAO 3V3 pin |
| GND | GND, common with the XIAO |
| TX | XIAO D7 / GPIO17 (RX) |
| RX | XIAO D6 / GPIO16 (TX), only needed once config commands exist |
| OUT | optional: XIAO GPIO as a hardware presence flag (3.3 V, high = present) |

Power: 3.1–3.5 V, ripple ≤50 mV, supply ≥1 A (typical draw 135 mA, peaks to 600 mA). If using a DC-DC,
switching frequency ≥2 MHz. Suggested: XIAO 5V pin → 1 A 3.3 V LDO (e.g. AMS1117-3.3, 10 µF in / 22 µF out)
→ module, with 4.7 µF + 100 nF at the module pins (Hi-Link reference design). Use a 5 V supply that can give
≥1 A, not a bare USB 2.0 port. The left-hand header (GND, SWCLK, SWDIO, BOOT1, 3V3) is debug only; the datasheet
says BOOT1 must be low at start, so if the module is silent, try tying BOOT1 to GND.

See `example.yaml` for the ESPHome config, `ld6004-xiao-esp32-c6.yaml` for a ready-to-flash XIAO ESP32-C6 config
(secrets.yaml with `wifi_ssid` / `wifi_password`), and `ld6004-esp32-c3-supermini.yaml` for the C3 Super Mini.
