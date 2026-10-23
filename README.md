# esphome-hlk-ld6004

ESPHome external component for the Hi-Link HLK-LD6004 60 GHz radar (UART, TinyFrame, 115200 baud).
Read-only: publishes presence (overall + 4 zones), target count and x/y/z of up to 3 targets.

Protocol from [swherdman/HLK-LD6004](https://github.com/swherdman/HLK-LD6004). **Untested on hardware.**

```yaml
external_components:
  - source: github://davidcoulson/esphome-hlk-ld6004
    components: [hlk_ld6004]
```

See `example.yaml` (XIAO ESP32-C6: sensor TX→D7/GPIO17, RX→D6/GPIO16, **3.3 V supply**, 3.1–3.5 V at up to 600 mA).
