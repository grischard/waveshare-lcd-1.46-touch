# Waveshare ESP32-S3-Touch-LCD-1.46 support for ESPHome

ESPHome external components and a hardware showcase for the **SPD2010** display/touch
controller used by the Waveshare **[ESP32-S3-Touch-LCD-1.46](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46B)** family.

Test target: **ESPHome 2026.8.2**.

This repository is a fork of [`chickymonkey/waveshare-lcd-1.46-touch`](https://github.com/chickymonkey/waveshare-lcd-1.46-touch), substantially streamlined and modernised to automate compile testing, and use Espressif’s maintained SPD2010 drivers and ESPHome’s standard component interfaces.

See [ATTRIBUTION.md](ATTRIBUTION.md) for the implementation lineage and upstream projects.

## Hardware showcase

[`examples/waveshare-esp32-s3-touch-lcd-1.46.yaml`](examples/waveshare-esp32-s3-touch-lcd-1.46.yaml)
demonstrates:

- 412×412 SPD2010 display with capacitive touch
- LVGL page gestures and a few test pages
- QMI8658 accelerometer, gyroscope, pitch, roll and temperature
- PCF85063 RTC
- Home Assistant time → RTC synchronisation
- I²S microphone with a live RMS dB VU meter (`sound_level`; 16-bit capture for this demo)
- I²S speaker with a compiled-in test tone
- PWM backlight
- Classic Wi-Fi, fallback AP, native Home Assistant API and OTA

```sh
# The example yaml expects `wifi_ssid` and `wifi_password` in your secrets.yaml.
cp examples/secrets.example.yaml examples/secrets.yaml
esphome run examples/waveshare-esp32-s3-touch-lcd-1.46.yaml
```

## External components

- `spd2010_display` — native Espressif `esp_lcd_spd2010` QSPI display integration.
- `spd2010_touch` — native ESPHome `touchscreen::Touchscreen` integration using
  Espressif's `esp_lcd_touch_spd2010` parser/state machine.

  These external components are thin ESPHome integration layers around Espressif’s maintained SPD2010 display and touch drivers. They let ESPHome configure and use the hardware through its normal `display`, `touchscreen`, GPIO and I²C interfaces, without reimplementing the controller protocols. The touch component also includes a small I²C compatibility shim needed by the current ESP-IDF transport layer.

## Display details

The display component publishes 412×412 geometry, big-endian RGB565 byte order and
`draw_rounding=4` during ESPHome configuration validation. LVGL therefore rounds partial
dirty rectangles to the controller's required four-pixel X boundaries automatically.

## Touch details

The touch controller protocol itself is handled by Espressif's maintained
`esp_lcd_touch_spd2010` driver. The local ESPHome component only adds lifecycle,
ESPHome touchscreen integration and a small I²C panel-IO transport shim.

The shim is currently necessary because ESP-IDF 5.5's generic LCD-I²C panel IO cannot
represent the commandless receive phase used by `esp_lcd_touch_spd2010` correctly.
