# STCC4 Configuration Guide

## Quick Start

The STCC4 sensor configuration is completely contained within the sensor platform. There is no separate component section needed.

### Minimal Configuration

```yaml
# Load the component from GitHub
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
- Pressure compensation: `101325 Pa` (sea level)

### With Custom I2C Address

```yaml
sensor:
  - platform: stcc4
    address: 0x64  # Change if using a different address
    co2:
      name: "CO2"
```

### With Custom Update Interval

```yaml
sensor:
  - platform: stcc4
    update_interval: 30s  # Poll every 30 seconds
    co2:
      name: "CO2"
```

### With Altitude Compensation

For accurate readings at altitude:

```yaml
sensor:
  - platform: stcc4
    altitude_compensation: 1000  # 1000 meters above sea level
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

### With Manual Pressure Compensation

If you know the exact pressure:

```yaml
sensor:
  - platform: stcc4
    pressure_compensation: 89874  # Pressure in Pascals
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

### Multiple Sensors

You can configure multiple STCC4 sensors on the same I2C bus (if using different addresses):

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
  
  # If you had another sensor with different address:
  # - platform: stcc4
  #   address: 0x65
  #   update_interval: 60s
  #   co2:
  #     name: "Bedroom CO2"
```

## All Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `address` | int | `0x64` | I2C address of the sensor |
| `update_interval` | Time | `60s` | How often to poll the sensor |
| `co2` | Sensor | - | CO2 concentration sensor (ppm) |
| `temperature` | Sensor | - | Temperature sensor (°C) |
| `humidity` | Sensor | - | Relative humidity sensor (%) |
| `pressure_compensation` | float | `101325` | Pressure in Pascals |
| `altitude_compensation` | int | - | Altitude in meters (0-10000) |

## Notes

- At least one of `co2`, `temperature`, or `humidity` should be specified
- Use either `pressure_compensation` OR `altitude_compensation`, not both
- If both are specified, `altitude_compensation` takes precedence
- Each sensor output supports all standard ESPHome sensor options (filters, on_value, etc.)

