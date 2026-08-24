# STCC4 Configuration Guide

## Quick Start

The STCC4 sensor configuration is contained within the sensor platform. There is no separate component section needed.

### Minimal Configuration

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/romkey/esphome-stcc4
      ref: main
    components: [ stcc4 ]

i2c:

sensor:
  - platform: stcc4
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

This uses all defaults:

- I2C address: `0x64`
- Update interval: `60s`
- Pressure compensation: device default (101300 Pa)
- Self-test on boot: enabled
- Soft reset on boot: enabled
- Measurement mode: `continuous`

### With Custom I2C Address

```yaml
sensor:
  - platform: stcc4
    address: 0x64
    co2:
      name: "CO2"
```

### With Custom Update Interval

```yaml
sensor:
  - platform: stcc4
    update_interval: 30s  # Minimum 1s
    co2:
      name: "CO2"
```

### With Altitude Compensation

```yaml
sensor:
  - platform: stcc4
    altitude_compensation: 1000
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

### With Manual Pressure Compensation

```yaml
sensor:
  - platform: stcc4
    pressure_compensation: 89874  # Pascals (40000-110000)
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

**Note:** `pressure_compensation` and `altitude_compensation` are mutually exclusive. Specifying both is a configuration error.

### Single-Shot Mode (Battery)

```yaml
sensor:
  - platform: stcc4
    measurement_mode: single_shot
    sleep_after_measurement: true
    update_interval: 60s
    co2:
      name: "CO2"
```

### External RH/T Compensation

Use when no SHT4x is wired to the STCC4 controller pads (SDA_C/SCL_C):

```yaml
sensor:
  - platform: sht4x
    temperature:
      id: ext_temp
      name: "Room Temperature"
    humidity:
      id: ext_hum
      name: "Room Humidity"

  - platform: stcc4
    rht_compensation:
      temperature: ext_temp
      humidity: ext_hum
    co2:
      name: "CO2"
```

Both sensor IDs must refer to the same physical RH/T sensor.

### Diagnostic Text Sensors

```yaml
sensor:
  - platform: stcc4
    id: stcc4_main
    co2:
      name: "CO2"

text_sensor:
  - platform: stcc4
    id: stcc4_main
    sensor_status:
      name: "STCC4 Status"
    last_error:
      name: "STCC4 Last Error"
```

### Multiple Sensors

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
  - platform: stcc4
    address: 0x64
    update_interval: 30s
    co2:
      name: "Living Room CO2"
    temperature:
      name: "Living Room Temperature"
    humidity:
      name: "Living Room Humidity"
```

## All Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `address` | int | `0x64` | I2C address of the sensor |
| `update_interval` | Time | `60s` | Polling interval (minimum 1s) |
| `co2` | Sensor | — | CO2 concentration sensor (ppm) |
| `temperature` | Sensor | — | Temperature sensor (°C) |
| `humidity` | Sensor | — | Relative humidity sensor (%) |
| `pressure_compensation` | float | — | Pressure in Pascals (40000–110000) |
| `altitude_compensation` | int | — | Altitude in metres (0–10000) |
| `self_test_on_boot` | bool | `true` | Run self-test during setup |
| `soft_reset_on_boot` | bool | `true` | General-call soft reset at boot |
| `measurement_mode` | enum | `continuous` | `continuous` or `single_shot` |
| `sleep_after_measurement` | bool | `false` | Sleep after single-shot reading |
| `rht_compensation` | map | — | External temperature and humidity sensor IDs |

## Notes

- At least one of `co2`, `temperature`, or `humidity` should be specified.
- `pressure_compensation` and `altitude_compensation` cannot be used together — ESPHome will reject the configuration.
- Temperature and humidity readings require an SHT4x on the STCC4 controller pads, or `rht_compensation` from an external sensor.
- During the first 20 seconds of continuous mode, CO2 reads a fixed 390 ppm bypass value (normal behaviour).
- Each sensor output supports all standard ESPHome sensor options (filters, `on_value`, etc.).
