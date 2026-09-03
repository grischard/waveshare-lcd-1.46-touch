# Attribution and provenance

This work builds on several upstream projects.

## chickymonkey/waveshare-lcd-1.46-touch

This repository is primarily a **fork** of:

https://github.com/chickymonkey/waveshare-lcd-1.46-touch

That project provided the original Waveshare ESP32-S3-Touch-LCD-1.46 ESPHome
configuration and SPD2010 touch integration. In particular, its June 2026 work moved
the touch implementation toward reusing ESPHome's configured I²C bus rather than
creating an unrelated private bus. Credit to chickymonkey for that work.

The fork history is the primary record of that provenance.

## paulastle/Campervan-esphome-build

An earlier SPD2010 raw-I²C/LVGL implementation was found in:

https://github.com/paulastle/Campervan-esphome-build

It predates the chickymonkey integration and is part of the lineage of the original raw
protocol/glue approach. The current component no longer maintains that raw SPD2010 parser;
Espressif's parser/state machine is used instead.

## Espressif

The display and touch protocol implementations come from Espressif's maintained IDF
components:

- `espressif/esp_lcd_spd2010`
- `espressif/esp_lcd_touch_spd2010`
- `espressif/esp_lcd_touch`

The local source is an ESPHome integration/transport layer around those drivers.

## Waveshare

Waveshare publishes the board schematic, examples, pin mappings, reset sequencing and
reference implementations for the ESP32-S3-Touch-LCD-1.46 family. Those materials were
used to verify QSPI/I²C/audio pin mappings and the PCA9554-controlled reset arrangement.

## QMI8658

The original Waveshare ESPHome configuration used the separate
`chickymonkey/QMI8658-Esphome` accelerometer component. Credit to chickymonkey again for
providing working QMI8658 support while ESPHome support was still evolving.

The current example does **not** include, vendor or load that component. ESPHome 2026.8.2's
core QMI8658 implementation is used instead because it supports substantially more of the
device.

## ESPHome

These components implement ESPHome's standard `display::Display` and
`touchscreen::Touchscreen` interfaces and use ESPHome's display metadata, LVGL, GPIO and
I²C abstractions. The showcase uses ESPHome core components for QMI8658, PCF85063,
I²S audio, sound-level measurement, Home Assistant API and time.
