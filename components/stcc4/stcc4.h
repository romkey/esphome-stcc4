#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace text_sensor {
class TextSensor;
}
namespace stcc4 {

enum MeasurementMode : uint8_t {
  CONTINUOUS = 0,
  SINGLE_SHOT = 1,
};

/// This class implements support for the STCC4 CO2 sensor.
class STCC4Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) {
    this->temperature_sensor_ = temperature_sensor;
  }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { this->status_text_sensor_ = sensor; }
  void set_error_text_sensor(text_sensor::TextSensor *sensor) { this->error_text_sensor_ = sensor; }

  void set_pressure_compensation(float pressure) {
    this->pressure_compensation_ = pressure;
    this->pressure_configured_ = true;
  }
  void set_altitude_compensation(uint16_t altitude) {
    this->altitude_compensation_ = altitude;
    this->altitude_configured_ = true;
  }
  void set_rht_temperature_source(sensor::Sensor *sensor) { this->rht_temperature_source_ = sensor; }
  void set_rht_humidity_source(sensor::Sensor *sensor) { this->rht_humidity_source_ = sensor; }
  void set_self_test_on_boot(bool self_test_on_boot) { this->self_test_on_boot_ = self_test_on_boot; }
  void set_soft_reset_on_boot(bool soft_reset_on_boot) { this->soft_reset_on_boot_ = soft_reset_on_boot; }
  void set_measurement_mode(MeasurementMode mode) { this->measurement_mode_ = mode; }
  void set_sleep_after_measurement(bool sleep) { this->sleep_after_measurement_ = sleep; }

 protected:
  enum ErrorCode : uint8_t {
    NONE = 0,
    COMMUNICATION_FAILED = 1,
    CRC_MISMATCH = 2,
    MEASUREMENT_INVALID = 3,
    UNKNOWN = 4,
  };

  enum class SetupState : uint8_t {
    SOFT_RESET = 0,
    POWER_UP,
    STOP,
    IDENTIFY,
    IDENTIFY_READ,
    SELF_TEST,
    SELF_TEST_READ,
    CONFIGURE,
    START,
    SETTLE,
    RUNNING,
    FAILED_RETRY,
  };

  static constexpr uint32_t STCC4_PRODUCT_ID = 0x0901018A;
  static constexpr uint8_t MAX_CMD_ARGS = 3;
  static constexpr uint8_t MAX_SETUP_ATTEMPTS = 5;
  static constexpr uint8_t MAX_CONSECUTIVE_READ_FAILURES = 3;
  static constexpr uint8_t MAX_CONSECUTIVE_RESTART_FAILURES = 3;
  static constexpr uint16_t STATUS_TESTING_MODE = 0x0004;

  void begin_setup_(SetupState start_state);
  void schedule_setup_state_(SetupState next_state, uint32_t delay_ms);
  void run_setup_state_();
  void handle_setup_failure_(const char *reason);
  void handle_setup_retry_();

  bool perform_soft_reset_();
  bool read_product_id_(uint32_t *product_id, uint64_t *serial_number);
  bool perform_self_test_(uint16_t *test_result);
  void decode_self_test_result_(uint16_t test_result);
  bool apply_compensation_();
  bool apply_rht_compensation_();
  bool disable_testing_mode_();

  bool write_command_(uint16_t command);
  bool write_command_with_args_(uint16_t command, const uint16_t *data, uint8_t len);
  bool read_data_(uint8_t *data, uint8_t len);
  uint8_t crc8_(const uint8_t *data, uint8_t len);

  bool start_continuous_measurement_();
  bool stop_continuous_measurement_();
  bool measure_single_shot_();
  bool enter_sleep_mode_();
  bool exit_sleep_mode_();
  bool set_pressure_compensation_(uint16_t pressure);
  bool set_rht_compensation_(uint16_t raw_temperature, uint16_t raw_humidity);

  void schedule_read_();
  void execute_read_();
  bool parse_and_publish_measurement_(const uint8_t *data);
  void on_read_success_();
  void on_read_failure_(bool after_retry);

  void handle_continuous_update_();
  void handle_single_shot_update_();
  void handle_restart_measurement_();

  void set_last_error_(ErrorCode error);
  const char *error_code_to_string_(ErrorCode error) const;
  const char *setup_state_to_string_(SetupState state) const;
  float calculate_pressure_from_altitude_(uint16_t altitude);
  uint16_t encode_rht_temperature_(float temperature) const;
  uint16_t encode_rht_humidity_(float humidity) const;

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *rht_temperature_source_{nullptr};
  sensor::Sensor *rht_humidity_source_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};
  text_sensor::TextSensor *error_text_sensor_{nullptr};

  float pressure_compensation_{101300.0f};  // Device default: 101300 Pa (§3.4.5)
  uint16_t altitude_compensation_{0};
  bool pressure_configured_{false};
  bool altitude_configured_{false};
  bool self_test_on_boot_{true};
  bool soft_reset_on_boot_{true};
  bool sleep_after_measurement_{false};
  MeasurementMode measurement_mode_{MeasurementMode::CONTINUOUS};

  SetupState setup_state_{SetupState::SOFT_RESET};
  bool measurement_started_{false};
  bool waiting_for_settle_{false};
  bool read_retry_pending_{false};
  bool read_is_retry_{false};
  bool read_in_progress_{false};
  bool single_shot_in_progress_{false};

  uint32_t product_id_{0};
  uint64_t serial_number_{0};
  uint16_t last_status_{0xFFFF};
  ErrorCode last_error_{ErrorCode::NONE};

  uint8_t setup_attempts_{0};
  uint8_t consecutive_read_failures_{0};
  uint8_t consecutive_restart_failures_{0};
};

}  // namespace stcc4
}  // namespace esphome
