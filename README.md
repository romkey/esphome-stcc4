# esphome-stcc4

[![CI](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yaml/badge.svg)](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yaml)
[![Lint](https://github.com/romkey/esphome-stcc4/actions/workflows/lint.yaml/badge.svg)](https://github.com/romkey/esphome-stcc4/actions/workflows/lint.yaml)
[![ESPHome](https://img.shields.io/badge/ESPHome-external%20component-18BCF2?logo=esphome&logoColor=white)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An ESPHome external component, plus a ready-made package, for the
Sensirion STCC4 CO2 sensor. It reports CO2, passes through temperature and
humidity from an SHT4x on the STCC4's controller pads, compensates for
pressure or altitude, can take temperature and humidity from another
sensor instead, and recovers from sensor resets and I2C errors without
rebooting the ESP.

> **AI disclosure:** This project was written by AI under human oversight,
> following [these rules](https://github.com/romkey/esphome-ai-coding-rules/blob/main/RULES.md).

## What's here

| Path | What it is |
|------|------------|
| `components/stcc4/` | The external component: `stcc4` sensor and text sensor platforms |
| `packages/stcc4.yaml` | The component with Home Assistant-ready entities, a CO2 moving average and diagnostic text sensors |
| `examples/esp32.yaml` | ESP32 DevKit, altitude compensation |
| `examples/esp32-c3.yaml` | ESP32-C3 DevKitM-1, single-shot mode with sleep and pressure compensation |
| `examples/esp32-s3.yaml` | ESP32-S3 DevKitC-1, temperature/humidity compensation from a separate SHT4x |
| `examples/esp32-scd40-comparison.yaml` | SparkFun ESP32 Thing Plus with an STCC4 and an SCD40 side by side |
| `docs/configuration.md` | Every option of the package and the component |

Features:

- CO2 in ppm; temperature and humidity from an SHT4x on the STCC4's
  controller pads
- Pressure or altitude compensation
- RH/T compensation from another ESPHome sensor when there's no SHT4x on
  the controller pads
- Continuous or single-shot measurement, with optional sleep between
  single-shot readings
- Product ID check and self-test on boot; retries startup and recovers from
  sensor resets
- Status and last-error text sensors

## Technical details

From the [STCC4 datasheet](datasheets/STCC4.pdf) (version 1, June 2025):

| | |
|---|---|
| Supply voltage | 2.7–5.5 V (3.3 V typical) |
| I2C address | 0x64; 0x65 with ADDR tied to VDD |
| I2C modes | Standard, Fast and Fast-mode Plus; no clock stretching |
| I2C bus voltage | 1.8 V to VDD |
| CO2 output range | 380–32,000 ppm |
| CO2 accuracy | ±(100 ppm + 10% of reading), 400–5,000 ppm, with SHT4x compensation |
| Response time (τ63%) | 20 s |

Wiring, using the ESP32 example's pins:

| STCC4 | ESP32 DevKit | Notes |
|-------|--------------|-------|
| VDD | 3.3V | Powering it from 3.3 V keeps the I2C levels safe for the ESP32 |
| GND | GND | |
| SDA | GPIO21 | Pull-up required |
| SCL | GPIO22 | Pull-up required |

The ESP32-C3 example uses GPIO6/GPIO7 and the ESP32-S3 example
GPIO8/GPIO9.

Temperature and humidity come from an **SHT4x on the STCC4's controller
pads** (SDA_C/SCL_C) at 0x44, not from the ESP's bus. Without one the STCC4
reports its power-on defaults (25 °C, 50 %RH) and CO2 is uncompensated,
unless you set `rht_compensation` (see `examples/esp32-s3.yaml`).

**Warm-up:** for the first 20 seconds of continuous mode CO2 reads a fixed
390 ppm. That's normal. After a long idle period accuracy can take up to an
hour to settle, and up to 12 hours on the very first power-up.

**Troubleshooting:**

- Not detected: check wiring and pull-ups, and look at the boot log for a
  product ID mismatch or self-test warning. The I2C scan sensor (from
  esphome-lib) shows what's on the bus.
- Temperature and humidity stuck at exactly 25 °C / 50 %: no SHT4x on the
  controller pads; wire one or use `rht_compensation`.
- Communication errors: the driver retries and recovers on its own;
  persistent errors appear in the `STCC4 Last Error` sensor. Long wires
  need stronger pull-ups (e.g. 4.7 kΩ) or a lower bus frequency.

The driver is based on Sensirion's
[arduino-i2c-stcc4](https://github.com/Sensirion/arduino-i2c-stcc4)
library.

## Using it in your own config

The package, pinned to a release:

```yaml
external_components:
  - source: github://romkey/esphome-stcc4@v0.1.0
    components: [stcc4]

i2c:
  id: i2c0
  sda: GPIO21
  scl: GPIO22

packages:
  stcc4: github://romkey/esphome-stcc4/packages/stcc4.yaml@v0.1.0
```

Or the component on its own:

```yaml
external_components:
  - source: github://romkey/esphome-stcc4@v0.1.0
    components: [stcc4]

sensor:
  - platform: stcc4
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

See [docs/configuration.md](docs/configuration.md) for every option.

## Building

Requires Docker.

```bash
cp examples/secrets.yaml.example examples/secrets.yaml   # then edit it
docker compose run --rm validate
docker compose run --rm build
docker compose run --rm lint
```

The first build downloads the toolchains into a Docker volume; later builds
reuse them and only recompile what changed. For your own device, copy an
example to `examples/local-<name>.yaml` (gitignored) and flash it with
`docker compose run --rm esphome run examples/local-<name>.yaml`. Docker
Desktop on macOS can't pass USB through, so flash the first time with
[ESPHome Web](https://web.esphome.io) and use OTA after that.

## License

[MIT](LICENSE), except the manufacturer datasheets in [datasheets/](datasheets/),
which belong to their manufacturers and are not covered by this license; see
each datasheet's `-license.txt`.
