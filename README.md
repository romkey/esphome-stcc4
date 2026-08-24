# ESPHome STCC4 CO2 Sensor Component

[![Validate](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml/badge.svg?job=lint)](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml)
[![Build](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml/badge.svg?job=build)](https://github.com/romkey/esphome-stcc4/actions/workflows/ci.yml)
[![ESPHome](https://img.shields.io/badge/ESPHome-external%20component-18BCF2?style=flat-square&logo=esphome&logoColor=white)](https://esphome.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This is an ESPHome custom component for the Sensirion STCC4 CO2 sensor. The STCC4 is a high-accuracy CO2 sensor that also provides temperature and humidity measurements.

## Features

- **CO2 measurement**: Accurate CO2 concentration in ppm
- **Temperature measurement**: Built-in temperature sensor (-10°C to +60°C)
- **Humidity measurement**: Built-in humidity sensor (0-100% RH)
- **Pressure compensation**: Automatically compensates for altitude/pressure
- **I2C interface**: Simple two-wire communication
- **Continuous measurement mode**: 1-second sampling interval

## Hardware Connection

The STCC4 uses I2C communication and operates at 3.3V:

| STCC4 Pin | ESP32 Pin | Description |
|-----------|-----------|-------------|
| VDD       | 3.3V      | Power supply (3.3V) |
| GND       | GND       | Ground |
| SDA       | GPIO21    | I2C Data (default) |
| SCL       | GPIO22    | I2C Clock (default) |

**Important**: The STCC4 operates at 3.3V. Do not connect to 5V!

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

1. Copy the `components/stcc4` directory to your ESPHome configuration directory:
   ```
   esphome/
   ├── config.yaml
   └── components/
       └── stcc4/
           ├── __init__.py
           ├── sensor.py
           ├── stcc4.h
           └── stcc4.cpp
   ```

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
# Load the component from GitHub
external_components:
  - source:
      type: git
      url: https://github.com/romkey/esphome-stcc4
      ref: main
    components: [ stcc4 ]

# I2C Bus Configuration
i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

# STCC4 Sensor
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
# Load the component from GitHub
external_components:
  - source:
      type: git
      url: https://github.com/romkey/esphome-stcc4
      ref: main
    components: [ stcc4 ]

# I2C Bus Configuration
i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

# STCC4 Sensor
sensor:
  - platform: stcc4
    address: 0x64         # I2C address (default: 0x64)
    update_interval: 60s  # Polling interval (default: 60s)
    co2:
      name: "CO2 Concentration"
      id: co2_ppm
      accuracy_decimals: 0
      filters:
        - sliding_window_moving_average:
            window_size: 5
            send_every: 5
    temperature:
      name: "Temperature"
      id: temp_c
      accuracy_decimals: 2
    humidity:
      name: "Relative Humidity"
      id: humidity_pct
      accuracy_decimals: 2
    # Pressure compensation in Pascals (default: 101325 Pa at sea level)
    pressure_compensation: 95000  # Example: 95 kPa
    # OR use altitude compensation in meters (calculates pressure automatically)
    # altitude_compensation: 500  # Example: 500 meters above sea level
```

## Configuration Options

### Sensor Platform Options

- **address** (Optional, int): The I2C address of the sensor. Defaults to `0x64`.
- **update_interval** (Optional, Time): The interval to check the sensor. Defaults to `60s`.
- **co2** (Optional): CO2 concentration sensor in ppm
  - All options from [Sensor](https://esphome.io/components/sensor/index.html#config-sensor)
- **temperature** (Optional): Temperature sensor in °C
  - All options from [Sensor](https://esphome.io/components/sensor/index.html#config-sensor)
- **humidity** (Optional): Relative humidity sensor in %
  - All options from [Sensor](https://esphome.io/components/sensor/index.html#config-sensor)
- **pressure_compensation** (Optional, float): Pressure compensation value in Pascals. Defaults to `101325` (sea level).
- **altitude_compensation** (Optional, int): Altitude in meters above sea level. When set, automatically calculates the pressure compensation. Range: 0-10000 meters.

**Note**: Use either `pressure_compensation` OR `altitude_compensation`, not both. If both are specified, `altitude_compensation` will take precedence.

## Sensor Specifications

| Parameter | Range | Accuracy |
|-----------|-------|----------|
| CO2 Concentration | 0-5000 ppm | ±(30 ppm + 3% of reading) |
| Temperature | -10 to +60 °C | ±0.5 °C |
| Relative Humidity | 0-100 % RH | ±3.5 % RH |
| Update Rate | 1 second | - |

## Tips and Best Practices

1. **Warm-up Time**: Allow the sensor to warm up for at least 3 minutes after power-on for stable readings.

2. **Placement**: 
   - Place the sensor in a well-ventilated area
   - Avoid placing near air vents, windows, or doors
   - Keep away from heat sources

3. **Altitude Compensation**: If you're not at sea level, use the `altitude_compensation` option for more accurate CO2 readings.

4. **Power Consumption**: The STCC4 uses continuous measurement mode, consuming approximately 10-12 mA.

5. **Calibration**: The STCC4 includes automatic self-calibration (ASC) which adjusts the sensor baseline over time when exposed to fresh air.

## Troubleshooting

### Sensor Not Detected

1. Check I2C wiring (SDA/SCL connections)
2. Verify power supply is 3.3V
3. Enable I2C scanning in your config to verify the sensor address:
   ```yaml
   i2c:
     scan: true
   ```

### Inaccurate Readings

1. Allow sufficient warm-up time (3+ minutes)
2. Check altitude/pressure compensation settings
3. Ensure good ventilation around the sensor
4. Check for proper ground connection

### Communication Errors

1. Add pull-up resistors (4.7kΩ) to SDA and SCL lines if cables are long
2. Reduce I2C bus speed:
   ```yaml
   i2c:
     frequency: 50kHz
   ```

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

