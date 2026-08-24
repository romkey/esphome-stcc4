# ESPHome STCC4 CO2 Sensor Component

[![Validate](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml/badge.svg?job=lint)](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml)
[![Build](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml/badge.svg?job=build)](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml)
[![ESPHome](https://img.shields.io/badge/ESPHome-external%20component-18BCF2?style=flat-square&logo=esphome&logoColor=white)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This is an ESPHome custom component for the Sensirion STCC4 CO2 sensor. The STCC4 measures CO2 concentration and reports temperature and humidity values sourced from an external SHT4x sensor on the module's dedicated I2C controller interface.

## Features

- **CO2 measurement**: CO2 concentration in ppm
- **Temperature and humidity passthrough**: Values from an external SHT4x at address 0x44 on the STCC4 controller pads (SDA_C/SCL_C)
- **Pressure compensation**: Compensates CO2 readings for local barometric pressure
- **External RH/T compensation**: Push temperature and humidity from another ESPHome sensor when no SHT4x is wired to the STCC4 controller pads
- **Hardware verification**: Product ID check and self-test on boot
- **Automatic recovery**: Retries startup and recovers from sensor resets without rebooting the ESP
- **I2C interface**: Up to 400 kHz; the STCC4 does not support clock stretching
- **Continuous or single-shot measurement modes**

## Hardware Connection

The STCC4 uses I2C communication and operates at 3.3V:

| STCC4 Pin | ESP32 Pin | Description |
|-----------|-----------|-------------|
| VDD       | 3.3V      | Power supply (3.3V) |
| GND       | GND       | Ground |
| SDA       | GPIO21    | I2C Data (default) |
| SCL       | GPIO22    | I2C Clock (default) |

**Important notes:**

- The STCC4 operates at 3.3V. Do not connect to 5V.
- Temperature and humidity come from an **external SHT4x** connected to the STCC4's dedicated controller pads (SDA_C/SCL_C) at I2C address **0x44**, not from the main ESP I2C bus. If that SHT4x is absent, the STCC4 reports power-on defaults (25 °C, 50 %RH) and CO2 is uncompensated unless you provide `rht_compensation` from another sensor.
- Use I2C pull-ups and keep bus frequency at or below **400 kHz**.

## Installation

### Option 1: Use as External Component (Recommended)

Add this to your ESPHome configuration to load the component directly from GitHub:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/romkey/esphome-stcc4
      ref: main
    components: [ stcc4 ]
```

### Option 2: Copy Component Files

1. Copy the `components/stcc4` directory to your ESPHome configuration directory.
2. Add the external component reference in your YAML configuration:

   ```yaml
   external_components:
     - source:
         type: local
         path: components
   ```

## Configuration

### Basic Configuration

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/romkey/esphome-stcc4
      ref: main
    components: [ stcc4 ]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

sensor:
  - platform: stcc4
    co2:
      name: "CO2 Concentration"
    temperature:
      name: "Temperature"
    humidity:
      name: "Relative Humidity"
```

### Advanced Configuration

```yaml
sensor:
  - platform: stcc4
    address: 0x64
    update_interval: 60s
    self_test_on_boot: true
    soft_reset_on_boot: true
    measurement_mode: continuous
    co2:
      name: "CO2 Concentration"
    temperature:
      name: "Temperature"
    humidity:
      name: "Relative Humidity"
    pressure_compensation: 95000  # Pascals (40000-110000)
    # OR:
    # altitude_compensation: 500

text_sensor:
  - platform: stcc4
    sensor_status:
      name: "STCC4 Status"
    last_error:
      name: "STCC4 Last Error"
```

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `address` | `0x64` | I2C address of the STCC4 |
| `update_interval` | `60s` | Polling interval (minimum **1s**) |
| `co2` | — | CO2 sensor in ppm |
| `temperature` | — | Temperature sensor in °C (from STCC4/SHT4x passthrough) |
| `humidity` | — | Humidity sensor in % RH (from STCC4/SHT4x passthrough) |
| `pressure_compensation` | device default (101300 Pa) | Pressure in **Pascals** (40000–110000). Mutually exclusive with `altitude_compensation`. |
| `altitude_compensation` | — | Altitude in metres (0–10000); calculates pressure automatically. Mutually exclusive with `pressure_compensation`. |
| `self_test_on_boot` | `true` | Run hardware self-test during setup (~360 ms) |
| `soft_reset_on_boot` | `true` | Issue I2C general-call soft reset at boot (resets all general-call-aware devices on the bus) |
| `measurement_mode` | `continuous` | `continuous` or `single_shot` |
| `sleep_after_measurement` | `false` | Enter sleep mode after each single-shot reading (battery builds) |
| `rht_compensation` | — | Provide external temperature and humidity sensor IDs when no SHT4x is on the STCC4 controller pads |

### RH/T compensation (no onboard SHT4x)

```yaml
sensor:
  - platform: sht4x
    id: room_sht
    temperature:
      id: room_temp
      name: "Room Temperature"
    humidity:
      id: room_hum
      name: "Room Humidity"

  - platform: stcc4
    rht_compensation:
      temperature: room_temp
      humidity: room_hum
    co2:
      name: "CO2"
```

Both values must come from the same physical RH/T sensor.

## Sensor Specifications

| Parameter | Range | Accuracy |
|-----------|-------|----------|
| CO2 Concentration | 0–5000 ppm | ±(100 ppm + 10% of reading) from 400 ppm |
| Temperature (via SHT4x) | Recommended operating 10–40 °C | ±0.5 °C |
| Relative Humidity (via SHT4x) | 0–100 % RH | ±3.5 % RH |
| Update Rate (continuous mode) | 1 second | — |

## Warm-up and Startup Behaviour

Per the STCC4 datasheet:

- **First 20 seconds** of continuous mode output a fixed **390 ppm** bypass value — this is normal, not a sensor fault.
- After a long idle period, accuracy may take up to **1 hour** to settle.
- On **first-ever power-up**, full accuracy may take up to **12 hours**.

The driver verifies the product ID and runs a self-test on boot. Boot logs include the product ID, serial number, and decoded self-test result.

## Tips and Best Practices

1. **Placement**: Use a well-ventilated location away from vents, windows, and heat sources.
2. **Altitude compensation**: If you know your altitude, set `altitude_compensation` for more accurate CO2 readings.
3. **Single-shot mode**: Use `measurement_mode: single_shot` with `sleep_after_measurement: true` for battery-powered builds.
4. **Power consumption**: Continuous mode draws approximately 10–12 mA; single-shot with sleep is much lower.
5. **Calibration**: The STCC4 includes automatic self-calibration (ASC) when exposed to fresh air periodically.

## Troubleshooting

### Sensor Not Detected

1. Check I2C wiring (SDA/SCL connections)
2. Verify power supply is 3.3V
3. Enable I2C scanning: `i2c: scan: true`
4. Check boot log for product ID mismatch or self-test warnings

### Inaccurate Readings

1. Allow time for warm-up (see above); ignore 390 ppm for the first 20 s
2. Verify SHT4x wiring on SDA_C/SCL_C if temperature/humidity sit at exactly 25 °C / 50 %
3. Check pressure or altitude compensation settings
4. Use `rht_compensation` if no SHT4x is attached to the STCC4 controller pads

### Communication Errors

The driver retries startup and recovers from transient I2C errors automatically. Persistent errors appear in the `last_error` text sensor.

1. Add 4.7 kΩ pull-ups on SDA and SCL if cable runs are long
2. Reduce I2C bus speed: `i2c: frequency: 100kHz`

## References

- [GitHub Repository](https://github.com/romkey/esphome-stcc4)
- [STCC4 Datasheet](datasheets/STCC4.pdf)
- [Sensirion Arduino Library](https://github.com/Sensirion/arduino-i2c-stcc4)
- [ESPHome Documentation](https://esphome.io/)
- [ESPHome External Components](https://esphome.io/components/external_components.html)

## License

This component is based on the Sensirion STCC4 Arduino library and follows ESPHome coding standards.

## Credits

Based on the Sensirion I2C STCC4 Arduino library.
