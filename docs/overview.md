# esphome-stcc4 overview

**What:** ESPHome external component and package for the Sensirion STCC4 CO2 sensor.
**Kind:** library.
**Hardware:** STCC4 at I2C 0x64 (0x65 with ADDR high); optional SHT4x at
0x44 on the STCC4's controller pads (SDA_C/SCL_C) for temperature/humidity.

## Files

- `components/stcc4/`: the C++ driver. `sensor.py` (CO2, temperature,
  humidity, compensation and mode options), `text_sensor.py` (status, last
  error), `stcc4.cpp`/`.h`.
- `packages/stcc4.yaml`: the component with HA-ready entities, a CO2
  moving average and diagnostic text sensors.
- `examples/esp32*.yaml`: ESP32, C3, S3 and an SCD40 comparison; all
  board/pin choices. Between them they use every component option.
- `examples/common/base.yaml`: shared project, network, recovery config,
  esphome-lib packages, and the local `external_components` source.
- `tests/test_crc.py`: pytest for the CRC-8.
- `datasheets/STCC4.pdf`: Sensirion datasheet v1 (June 2025).

## Required by packages

- An I2C bus with `id: i2c0` (`stcc4_i2c_id` to change).
- The `stcc4` external component.

## Conventions

- Substitutions prefixed `stcc4_`; all have defaults.
- Options without a substitution are added with `- id: !extend stcc4_sensor`.
- The id `stcc4` is reserved by ESPHome (integration name); don't use it.
- Rules: https://github.com/romkey/esphome-ai-coding-rules/blob/main/RULES.md
