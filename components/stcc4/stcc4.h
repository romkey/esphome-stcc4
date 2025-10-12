#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace stcc4 {

/// This class implements support for the STCC4 CO2 sensor.
class STCC4Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_pressure_compensation(float pressure) { pressure_compensation_ = pressure; }
  void set_altitude_compensation(uint16_t altitude) { altitude_compensation_ = altitude; }

 protected:
  enum ErrorCode : uint8_t {
    COMMUNICATION_FAILED = 1,
    CRC_MISMATCH = 2,
    MEASUREMENT_INVALID = 3,
    UNKNOWN = 4,
  };

  bool write_command_(uint16_t command);
  bool write_command_with_args_(uint16_t command, const uint16_t *data, uint8_t len);
  bool read_data_(uint8_t *data, uint8_t len);
  uint8_t crc8_(const uint8_t *data, uint8_t len);
  bool start_continuous_measurement_();
  bool stop_continuous_measurement_();
  bool read_measurement_();
  bool set_pressure_compensation_(uint16_t pressure);
  float calculate_pressure_from_altitude_(uint16_t altitude);

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  
  float pressure_compensation_{101325.0f};  // Default: 101325 Pa (sea level)
  uint16_t altitude_compensation_{0};
  bool measurement_started_{false};
  
  uint8_t setup_attempts_{0};
  static constexpr uint8_t MAX_SETUP_ATTEMPTS = 5;
};

}  // namespace stcc4
}  // namespace esphome

