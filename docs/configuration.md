# Configuration

Two ways to use the STCC4: the `stcc4` external component directly, or
`packages/stcc4.yaml`, which wraps it with Home Assistant-ready entities.
Both need the component:

```yaml
external_components:
  - source: github://romkey/esphome-stcc4@v0.1.0
    components: [stcc4]
```

## `packages/stcc4.yaml`

CO2, temperature and humidity, a CO2 moving average, and the two diagnostic
text sensors.

**Requires:** the component (above) and an I2C bus with `id: i2c0` (or set
`stcc4_i2c_id`).

| Substitution | Default | Meaning |
|--------------|---------|---------|
| `stcc4_id` | `stcc4_sensor` | Component id; change if included twice |
| `stcc4_i2c_id` | `i2c0` | I2C bus id |
| `stcc4_address` | `0x64` | I2C address (`0x65` with ADDR tied to VDD) |
| `stcc4_update_interval` | `60s` | Polling interval, at least `1s` |
| `stcc4_measurement_mode` | `continuous` | `continuous` or `single_shot` |
| `stcc4_self_test_on_boot` | `true` | Run the self-test at boot |
| `stcc4_soft_reset_on_boot` | `true` | I2C general-call soft reset at boot |
| `stcc4_co2_name` | `CO2` | CO2 entity name |
| `stcc4_co2_id` | `stcc4_co2` | CO2 sensor id |
| `stcc4_co2_window` | `3` | CO2 moving-average window in samples; `1` turns it off |
| `stcc4_temperature_name` | `Temperature` | Temperature entity name |
| `stcc4_temperature_id` | `stcc4_temperature` | Temperature sensor id |
| `stcc4_humidity_name` | `Humidity` | Humidity entity name |
| `stcc4_humidity_id` | `stcc4_humidity` | Humidity sensor id |
| `stcc4_status_name` | `STCC4 Status` | Status entity name |
| `stcc4_status_id` | `stcc4_status` | Status text sensor id |
| `stcc4_last_error_name` | `STCC4 Last Error` | Last-error entity name |
| `stcc4_last_error_id` | `stcc4_last_error` | Last-error text sensor id |
| `stcc4_diagnostic_category` | `diagnostic` | Entity category of the two text sensors |

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

Options that have no substitution go on the package's sensor with
`!extend`:

```yaml
sensor:
  - id: !extend stcc4_sensor
    altitude_compensation: 500
```

The examples use every option: `examples/esp32.yaml` (altitude),
`examples/esp32-c3.yaml` (single-shot, sleep, pressure) and
`examples/esp32-s3.yaml` (external RH/T compensation).

## `stcc4` component

### Sensor platform

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
  - platform: stcc4
    co2:
      name: "CO2"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `address` | int | `0x64` | I2C address of the sensor |
| `update_interval` | Time | `60s` | Polling interval (minimum 1s) |
| `co2` | Sensor | — | CO2 concentration sensor (ppm) |
| `temperature` | Sensor | — | Temperature sensor (°C) |
| `humidity` | Sensor | — | Relative humidity sensor (%) |
| `pressure_compensation` | float | — | Pressure in Pascals (40000–110000) |
| `altitude_compensation` | int | — | Altitude in metres (0–10000) |
| `self_test_on_boot` | bool | `true` | Run self-test during setup (~360 ms) |
| `soft_reset_on_boot` | bool | `true` | General-call soft reset at boot; resets every general-call-aware device on the bus |
| `measurement_mode` | enum | `continuous` | `continuous` or `single_shot` |
| `sleep_after_measurement` | bool | `false` | Sleep after each single-shot reading (battery builds) |
| `rht_compensation` | map | — | `temperature` and `humidity` sensor ids from an external RH/T sensor |

Each sensor output takes the standard ESPHome sensor options (filters,
`on_value`, ...).

- `pressure_compensation` and `altitude_compensation` are mutually
  exclusive; ESPHome rejects a config with both. Without either, the sensor
  uses its default of 101300 Pa.
- Temperature and humidity come from an SHT4x on the STCC4's controller
  pads (SDA_C/SCL_C). Without one they read the power-on defaults (25 °C,
  50 %RH) and CO2 is uncompensated, unless you set `rht_compensation`.
- For the first 20 seconds of continuous mode CO2 reads a fixed 390 ppm.
  That's normal.

### External RH/T compensation

Use when no SHT4x is wired to the STCC4's controller pads. Both ids must
refer to the same physical sensor.

```yaml
sensor:
  - platform: sht4x
    temperature:
      id: room_temperature
      name: "Temperature"
    humidity:
      id: room_humidity
      name: "Humidity"

  - platform: stcc4
    rht_compensation:
      temperature: room_temperature
      humidity: room_humidity
    co2:
      name: "CO2"
```

### Single-shot mode (battery)

```yaml
sensor:
  - platform: stcc4
    measurement_mode: single_shot
    sleep_after_measurement: true
    update_interval: 60s
    co2:
      name: "CO2"
```

### Text sensor platform

Driver status and the last error. `id` is the sensor platform's id; it can
be left out if there's only one STCC4.

| Option | Description |
|--------|-------------|
| `id` | The `stcc4` sensor platform's id |
| `sensor_status` | Text sensor with the driver's state |
| `last_error` | Text sensor with the most recent error |

At least one of `sensor_status` and `last_error` is required.

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
      entity_category: diagnostic
    last_error:
      name: "STCC4 Last Error"
      entity_category: diagnostic
```
