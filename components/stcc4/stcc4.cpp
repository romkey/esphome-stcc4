#include "stcc4.h"
#include "esphome/core/log.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace stcc4 {

static const char *const TAG = "stcc4";

// Command IDs
static const uint16_t CMD_START_CONTINUOUS_MEASUREMENT = 0x218B;
static const uint16_t CMD_READ_MEASUREMENT = 0xEC05;
static const uint16_t CMD_STOP_CONTINUOUS_MEASUREMENT = 0x3F86;
static const uint16_t CMD_MEASURE_SINGLE_SHOT = 0x219D;
static const uint16_t CMD_GET_PRODUCT_ID = 0x365B;
static const uint16_t CMD_SET_PRESSURE_COMPENSATION = 0xE016;
static const uint16_t CMD_SET_RHT_COMPENSATION = 0xE000;
static const uint16_t CMD_ENTER_SLEEP_MODE = 0x3650;
static const uint16_t CMD_EXIT_SLEEP_MODE = 0x0000;
static const uint16_t CMD_PERFORM_SELF_TEST = 0x278C;
static const uint16_t CMD_DISABLE_TESTING_MODE = 0x3F3D;

// Timing constants (milliseconds) — annotated per STCC4 datasheet v1 (June 2025)
static const uint16_t SOFT_RESET_DELAY = 10;            // §3.4.10 perform_soft_reset execution time
static const uint16_t POWER_UP_DELAY = 10;              // §2.6 tPU (VDD >= 2.7 V)
static const uint16_t STOP_MEASUREMENT_DELAY = 1200;    // §3.4.2: 1 s interval + 200 ms tolerance
static const uint16_t READ_EXECUTION_DELAY = 1;         // §3.4.3 read_measurement execution time
static const uint16_t MEASUREMENT_SETTLE_DELAY = 1000;  // §3.4.1: wait 1 s after start before first read
static const uint16_t READ_RETRY_DELAY = 150;           // §3.4.1: retry read after 150 ms if NACK
static const uint16_t SELF_TEST_DELAY = 360;            // §3.4.12 perform_self_test execution time
static const uint16_t SINGLE_SHOT_DELAY = 500;          // §3.4.10 measure_single_shot (reference driver)
static const uint16_t EXIT_SLEEP_DELAY = 5;             // §3.4.8 exit_sleep_mode
static const uint16_t SETUP_RETRY_DELAYS[] = {250, 500, 1000, 1000, 1000};

void STCC4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STCC4...");
  SetupState start = this->soft_reset_on_boot_ ? SetupState::SOFT_RESET : SetupState::POWER_UP;
  this->begin_setup_(start);
}

void STCC4Component::begin_setup_(SetupState start_state) {
  this->setup_state_ = start_state;
  this->waiting_for_settle_ = false;
  this->read_retry_pending_ = false;
  this->read_in_progress_ = false;
  this->single_shot_in_progress_ = false;
  this->run_setup_state_();
}

void STCC4Component::schedule_setup_state_(SetupState next_state, uint32_t delay_ms) {
  this->setup_state_ = next_state;
  if (delay_ms == 0) {
    this->run_setup_state_();
    return;
  }
  this->set_timeout("setup", delay_ms, [this]() { this->run_setup_state_(); });
}

void STCC4Component::run_setup_state_() {
  switch (this->setup_state_) {
    case SetupState::SOFT_RESET: {
      this->perform_soft_reset_();
      this->schedule_setup_state_(SetupState::POWER_UP, SOFT_RESET_DELAY);
      break;
    }
    case SetupState::POWER_UP: {
      this->schedule_setup_state_(SetupState::STOP, POWER_UP_DELAY);
      break;
    }
    case SetupState::STOP: {
      this->stop_continuous_measurement_();
      this->schedule_setup_state_(SetupState::IDENTIFY, STOP_MEASUREMENT_DELAY);
      break;
    }
    case SetupState::IDENTIFY: {
      if (!this->write_command_(CMD_GET_PRODUCT_ID)) {
        this->handle_setup_failure_("Failed to send get product ID command");
        return;
      }
      this->schedule_setup_state_(SetupState::IDENTIFY_READ, READ_EXECUTION_DELAY);
      break;
    }
    case SetupState::IDENTIFY_READ: {
      uint32_t product_id = 0;
      uint64_t serial_number = 0;
      if (!this->read_product_id_(&product_id, &serial_number)) {
        this->handle_setup_failure_("Failed to read product ID");
        return;
      }
      this->product_id_ = product_id;
      this->serial_number_ = serial_number;
      if (product_id != STCC4_PRODUCT_ID) {
        ESP_LOGE(TAG, "STCC4 not found: expected product ID 0x%08" PRIX32 ", got 0x%08" PRIX32, STCC4_PRODUCT_ID,
                 product_id);
        this->mark_failed();
        return;
      }
      ESP_LOGCONFIG(TAG, "Product ID: 0x%08" PRIX32 ", Serial: 0x%016" PRIX64, product_id, serial_number);
      if (this->self_test_on_boot_) {
        this->schedule_setup_state_(SetupState::SELF_TEST, 0);
      } else {
        this->schedule_setup_state_(SetupState::CONFIGURE, 0);
      }
      break;
    }
    case SetupState::SELF_TEST: {
      if (!this->write_command_(CMD_PERFORM_SELF_TEST)) {
        this->handle_setup_failure_("Failed to start self-test");
        return;
      }
      this->schedule_setup_state_(SetupState::SELF_TEST_READ, SELF_TEST_DELAY);
      break;
    }
    case SetupState::SELF_TEST_READ: {
      uint16_t test_result = 0;
      if (!this->perform_self_test_(&test_result)) {
        this->handle_setup_failure_("Failed to read self-test result");
        return;
      }
      this->decode_self_test_result_(test_result);
      this->schedule_setup_state_(SetupState::CONFIGURE, 0);
      break;
    }
    case SetupState::CONFIGURE: {
      if (!this->apply_compensation_()) {
        this->handle_setup_failure_("Failed to apply compensation");
        return;
      }
      this->schedule_setup_state_(SetupState::START, 0);
      break;
    }
    case SetupState::START: {
      bool started = false;
      if (this->measurement_mode_ == MeasurementMode::SINGLE_SHOT) {
        this->measurement_started_ = false;
        started = true;
      } else {
        started = this->start_continuous_measurement_();
      }
      if (!started) {
        this->handle_setup_failure_("Failed to start measurement");
        return;
      }
      if (this->measurement_mode_ == MeasurementMode::CONTINUOUS) {
        this->schedule_setup_state_(SetupState::SETTLE, MEASUREMENT_SETTLE_DELAY);
      } else {
        this->setup_attempts_ = 0;
        this->setup_state_ = SetupState::RUNNING;
        ESP_LOGCONFIG(TAG, "STCC4 setup complete (single-shot mode)");
      }
      break;
    }
    case SetupState::SETTLE: {
      this->setup_attempts_ = 0;
      this->consecutive_read_failures_ = 0;
      this->consecutive_restart_failures_ = 0;
      this->setup_state_ = SetupState::RUNNING;
      ESP_LOGCONFIG(TAG, "STCC4 setup complete");
      break;
    }
    case SetupState::FAILED_RETRY: {
      this->handle_setup_retry_();
      break;
    }
    case SetupState::RUNNING:
      break;
  }
}

void STCC4Component::handle_setup_failure_(const char *reason) {
  ESP_LOGW(TAG, "Setup step '%s' failed in state %s (attempt %u/%u)", reason,
           this->setup_state_to_string_(this->setup_state_), this->setup_attempts_ + 1, MAX_SETUP_ATTEMPTS);
  this->set_last_error_(ErrorCode::COMMUNICATION_FAILED);
  this->setup_attempts_++;
  if (this->setup_attempts_ >= MAX_SETUP_ATTEMPTS) {
    ESP_LOGE(TAG, "Setup failed after %u attempts: %s", MAX_SETUP_ATTEMPTS, reason);
    this->mark_failed();
    return;
  }
  this->schedule_setup_state_(SetupState::FAILED_RETRY, 0);
}

void STCC4Component::handle_setup_retry_() {
  uint8_t idx = this->setup_attempts_ - 1;
  if (idx >= sizeof(SETUP_RETRY_DELAYS) / sizeof(SETUP_RETRY_DELAYS[0])) {
    idx = sizeof(SETUP_RETRY_DELAYS) / sizeof(SETUP_RETRY_DELAYS[0]) - 1;
  }
  uint32_t delay_ms = SETUP_RETRY_DELAYS[idx];
  ESP_LOGW(TAG, "Retrying setup in %" PRIu32 " ms", delay_ms);
  SetupState restart = this->soft_reset_on_boot_ ? SetupState::SOFT_RESET : SetupState::POWER_UP;
  this->set_timeout("setup_retry", delay_ms, [this, restart]() { this->begin_setup_(restart); });
}

void STCC4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "STCC4:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "  Setup State: %s", this->setup_state_to_string_(this->setup_state_));
  if (this->product_id_ != 0) {
    ESP_LOGCONFIG(TAG, "  Product ID: 0x%08" PRIX32, this->product_id_);
    ESP_LOGCONFIG(TAG, "  Serial Number: 0x%016" PRIX64, this->serial_number_);
  }
  ESP_LOGCONFIG(TAG, "  Measurement Mode: %s",
                this->measurement_mode_ == MeasurementMode::CONTINUOUS ? "continuous" : "single_shot");
  ESP_LOGCONFIG(TAG, "  Self Test On Boot: %s", YESNO(this->self_test_on_boot_));
  ESP_LOGCONFIG(TAG, "  Soft Reset On Boot: %s", YESNO(this->soft_reset_on_boot_));
  if (this->pressure_configured_) {
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: %.0f Pa", this->pressure_compensation_);
  } else {
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: device default (101300 Pa)");
  }
  if (this->altitude_configured_) {
    ESP_LOGCONFIG(TAG, "  Altitude Compensation: %d m", this->altitude_compensation_);
  }
  if (this->last_error_ != ErrorCode::NONE) {
    ESP_LOGCONFIG(TAG, "  Last Error: %s", this->error_code_to_string_(this->last_error_));
  }
}

void STCC4Component::update() {
  if (this->setup_state_ != SetupState::RUNNING) {
    return;
  }
  if (this->waiting_for_settle_ || this->read_retry_pending_ || this->read_in_progress_ ||
      this->single_shot_in_progress_) {
    return;
  }

  if (this->measurement_mode_ == MeasurementMode::SINGLE_SHOT) {
    this->handle_single_shot_update_();
    return;
  }

  this->handle_continuous_update_();
}

void STCC4Component::handle_continuous_update_() {
  if (!this->measurement_started_) {
    this->handle_restart_measurement_();
    return;
  }

  if (this->rht_temperature_source_ != nullptr && this->rht_humidity_source_ != nullptr) {
    this->apply_rht_compensation_();
  }

  this->schedule_read_();
}

void STCC4Component::handle_single_shot_update_() {
  this->single_shot_in_progress_ = true;

  if (!this->exit_sleep_mode_()) {
    ESP_LOGD(TAG, "Exit sleep not acknowledged (may not have been sleeping)");
  }

  this->set_timeout("exit_sleep", EXIT_SLEEP_DELAY, [this]() {
    if (this->rht_temperature_source_ != nullptr && this->rht_humidity_source_ != nullptr) {
      this->apply_rht_compensation_();
    }

    if (!this->measure_single_shot_()) {
      this->single_shot_in_progress_ = false;
      this->on_read_failure_(false);
      return;
    }

    this->set_timeout("single_shot", SINGLE_SHOT_DELAY, [this]() {
      this->schedule_read_();
      this->single_shot_in_progress_ = false;
    });
  });
}

void STCC4Component::handle_restart_measurement_() {
  if (!this->start_continuous_measurement_()) {
    this->consecutive_restart_failures_++;
    ESP_LOGW(TAG, "Failed to restart continuous measurement (%u/%u)", this->consecutive_restart_failures_,
             MAX_CONSECUTIVE_RESTART_FAILURES);
    if (this->consecutive_restart_failures_ >= MAX_CONSECUTIVE_RESTART_FAILURES) {
      this->consecutive_restart_failures_ = 0;
      this->status_set_error();
      SetupState restart = this->soft_reset_on_boot_ ? SetupState::SOFT_RESET : SetupState::POWER_UP;
      this->begin_setup_(restart);
    }
    return;
  }

  this->consecutive_restart_failures_ = 0;
  this->waiting_for_settle_ = true;
  this->set_timeout("settle", MEASUREMENT_SETTLE_DELAY, [this]() { this->waiting_for_settle_ = false; });
}

void STCC4Component::schedule_read_() {
  if (this->read_in_progress_) {
    return;
  }
  this->read_in_progress_ = true;

  if (!this->write_command_(CMD_READ_MEASUREMENT)) {
    this->read_in_progress_ = false;
    this->on_read_failure_(this->read_is_retry_);
    this->read_is_retry_ = false;
    return;
  }

  this->set_timeout("read_exec", READ_EXECUTION_DELAY, [this]() { this->execute_read_(); });
}

void STCC4Component::execute_read_() {
  uint8_t data[12];
  if (!this->read_data_(data, 12)) {
    this->read_in_progress_ = false;
    this->on_read_failure_(this->read_is_retry_);
    this->read_is_retry_ = false;
    return;
  }

  if (!this->parse_and_publish_measurement_(data)) {
    this->read_in_progress_ = false;
    this->on_read_failure_(this->read_is_retry_);
    this->read_is_retry_ = false;
    return;
  }

  this->read_in_progress_ = false;
  this->read_is_retry_ = false;
  this->on_read_success_();
}

bool STCC4Component::parse_and_publish_measurement_(const uint8_t *data) {
  uint16_t co2_u16 = (uint16_t(data[0]) << 8) | uint16_t(data[1]);
  int16_t co2_raw = static_cast<int16_t>(co2_u16);

  uint16_t temp_raw = (uint16_t(data[3]) << 8) | uint16_t(data[4]);
  float temperature = -45.0f + (175.0f * temp_raw / 65535.0f);

  uint16_t hum_raw = (uint16_t(data[6]) << 8) | uint16_t(data[7]);
  float humidity = -6.0f + (125.0f * hum_raw / 65535.0f);

  uint16_t status = (uint16_t(data[9]) << 8) | uint16_t(data[10]);

  if (status != this->last_status_) {
    ESP_LOGW(TAG, "Sensor status changed: 0x%04X -> 0x%04X", this->last_status_, status);
    this->last_status_ = status;
  }

  if (this->status_text_sensor_ != nullptr) {
#ifdef USE_TEXT_SENSOR
    char buf[8];
    snprintf(buf, sizeof(buf), "0x%04X", status);
    this->status_text_sensor_->publish_state(buf);
#endif
  }

  if (status == STATUS_TESTING_MODE) {
    ESP_LOGW(TAG, "Sensor in testing mode, disabling and skipping sample");
    this->disable_testing_mode_();
    this->set_last_error_(ErrorCode::MEASUREMENT_INVALID);
    return false;
  }

  ESP_LOGD(TAG, "CO2: %d ppm, Temperature: %.2f°C, Humidity: %.2f%%, Status: 0x%04X", co2_raw, temperature, humidity,
           status);

  if (this->co2_sensor_ != nullptr) {
    this->co2_sensor_->publish_state(co2_raw);
  }
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(humidity);
  }

  return true;
}

void STCC4Component::on_read_success_() {
  this->consecutive_read_failures_ = 0;
  this->consecutive_restart_failures_ = 0;
  this->status_clear_warning();
  this->status_clear_error();
  this->set_last_error_(ErrorCode::NONE);

  if (this->measurement_mode_ == MeasurementMode::SINGLE_SHOT && this->sleep_after_measurement_) {
    this->enter_sleep_mode_();
  }
}

void STCC4Component::on_read_failure_(bool after_retry) {
  this->consecutive_read_failures_++;
  this->status_set_warning();
  this->set_last_error_(ErrorCode::COMMUNICATION_FAILED);

  if (!after_retry && this->consecutive_read_failures_ == 1) {
    ESP_LOGW(TAG, "Failed to read measurement, retrying in %u ms...", READ_RETRY_DELAY);
    this->read_retry_pending_ = true;
    this->read_is_retry_ = true;
    this->set_timeout("read_retry", READ_RETRY_DELAY, [this]() {
      this->read_retry_pending_ = false;
      this->schedule_read_();
    });
    return;
  }

  ESP_LOGW(TAG, "Measurement read failed (%u/%u)", this->consecutive_read_failures_, MAX_CONSECUTIVE_READ_FAILURES);

  if (this->consecutive_read_failures_ >= MAX_CONSECUTIVE_READ_FAILURES) {
    this->consecutive_read_failures_ = 0;
    this->measurement_started_ = false;
    this->handle_restart_measurement_();
  }
}

float STCC4Component::get_setup_priority() const { return setup_priority::DATA; }

bool STCC4Component::perform_soft_reset_() {
  uint8_t reset_byte = 0x06;
  // General call address 0x00 — NACK is expected and must not be treated as failure (§3.4.10)
  this->bus_->write_readv(0x00, &reset_byte, 1, nullptr, 0);
  ESP_LOGD(TAG, "Soft reset issued (general call)");
  return true;
}

bool STCC4Component::read_product_id_(uint32_t *product_id, uint64_t *serial_number) {
  uint8_t data[18];
  if (!this->read_data_(data, 18)) {
    return false;
  }

  *product_id = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[3]) << 8) | data[4];
  *serial_number = (uint64_t(data[6]) << 56) | (uint64_t(data[7]) << 48) | (uint64_t(data[9]) << 40) |
                   (uint64_t(data[10]) << 32) | (uint64_t(data[12]) << 24) | (uint64_t(data[13]) << 16) |
                   (uint64_t(data[15]) << 8) | uint64_t(data[16]);
  return true;
}

bool STCC4Component::perform_self_test_(uint16_t *test_result) {
  uint8_t data[3];
  if (!this->read_data_(data, 3)) {
    return false;
  }
  *test_result = (uint16_t(data[0]) << 8) | data[1];
  return true;
}

void STCC4Component::decode_self_test_result_(uint16_t test_result) {
  if (test_result == 0x0000) {
    ESP_LOGCONFIG(TAG, "Self-test passed (0x0000)");
  } else if (test_result == 0x0010) {
    ESP_LOGCONFIG(TAG, "Self-test passed with SHT4x-not-connected flag (0x0010)");
  } else {
    ESP_LOGW(TAG, "Self-test reported malfunction: 0x%04X", test_result);
  }

  if (test_result & 0x0001) {
    ESP_LOGW(TAG, "Self-test: supply voltage out of specified range");
  }
  if (test_result & 0x000E) {
    ESP_LOGW(TAG, "Self-test: debug field = 0x%01X (refer to Sensirion)", (test_result >> 1) & 0x07);
  }
  if (test_result & 0x0010) {
    ESP_LOGW(TAG, "Self-test: SHT4x not connected via STCC4 controller interface pad");
  }
  if (test_result & 0x0060) {
    ESP_LOGW(TAG, "Self-test: memory error (bits 6:5 = 0x%01X)", (test_result >> 5) & 0x03);
  }
}

bool STCC4Component::apply_compensation_() {
  if (this->altitude_configured_) {
    this->pressure_compensation_ = this->calculate_pressure_from_altitude_(this->altitude_compensation_);
    this->pressure_configured_ = true;
    ESP_LOGD(TAG, "Calculated pressure from altitude %u m: %.0f Pa", this->altitude_compensation_,
             this->pressure_compensation_);
  }

  if (this->pressure_configured_) {
    // §3.4.5: register value = Pa / 2; e.g. 101300 Pa -> 50650 (0xC5DA)
    uint16_t pressure_raw = static_cast<uint16_t>(this->pressure_compensation_ / 2.0f);
    if (!this->set_pressure_compensation_(pressure_raw)) {
      return false;
    }
    ESP_LOGD(TAG, "Pressure compensation set to %.0f Pa (raw 0x%04X)", this->pressure_compensation_, pressure_raw);
  }

  return this->apply_rht_compensation_();
}

bool STCC4Component::apply_rht_compensation_() {
  if (this->rht_temperature_source_ == nullptr || this->rht_humidity_source_ == nullptr) {
    return true;
  }
  if (!this->rht_temperature_source_->has_state() || !this->rht_humidity_source_->has_state()) {
    ESP_LOGD(TAG, "RHT compensation sources not ready");
    return true;
  }

  uint16_t temp_raw = this->encode_rht_temperature_(this->rht_temperature_source_->state);
  uint16_t hum_raw = this->encode_rht_humidity_(this->rht_humidity_source_->state);
  if (!this->set_rht_compensation_(temp_raw, hum_raw)) {
    ESP_LOGW(TAG, "Failed to set RHT compensation");
    return false;
  }
  ESP_LOGD(TAG, "RHT compensation updated (temp raw 0x%04X, hum raw 0x%04X)", temp_raw, hum_raw);
  return true;
}

uint16_t STCC4Component::encode_rht_temperature_(float temperature) const {
  float raw = (temperature + 45.0f) * 65535.0f / 175.0f;
  if (raw < 0.0f) {
    raw = 0.0f;
  } else if (raw > 65535.0f) {
    raw = 65535.0f;
  }
  return static_cast<uint16_t>(raw);
}

uint16_t STCC4Component::encode_rht_humidity_(float humidity) const {
  float raw = (humidity + 6.0f) * 65535.0f / 125.0f;
  if (raw < 0.0f) {
    raw = 0.0f;
  } else if (raw > 65535.0f) {
    raw = 65535.0f;
  }
  return static_cast<uint16_t>(raw);
}

bool STCC4Component::disable_testing_mode_() { return this->write_command_(CMD_DISABLE_TESTING_MODE); }

bool STCC4Component::write_command_(uint16_t command) {
  uint8_t data[2];
  data[0] = (command >> 8) & 0xFF;
  data[1] = command & 0xFF;

  if (this->write(data, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write failed for command 0x%04X", command);
    this->set_last_error_(ErrorCode::COMMUNICATION_FAILED);
    return false;
  }
  return true;
}

bool STCC4Component::write_command_with_args_(uint16_t command, const uint16_t *args, uint8_t len) {
  if (len > MAX_CMD_ARGS) {
    ESP_LOGE(TAG, "Command 0x%04X: too many arguments (%u, max %u)", command, len, MAX_CMD_ARGS);
    this->set_last_error_(ErrorCode::UNKNOWN);
    return false;
  }

  uint8_t buffer[2 + MAX_CMD_ARGS * 3];
  buffer[0] = (command >> 8) & 0xFF;
  buffer[1] = command & 0xFF;

  uint8_t pos = 2;
  for (uint8_t i = 0; i < len; i++) {
    buffer[pos++] = (args[i] >> 8) & 0xFF;
    buffer[pos++] = args[i] & 0xFF;
    buffer[pos] = this->crc8_(&buffer[pos - 2], 2);
    pos++;
  }

  if (this->write(buffer, pos) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write with args failed for command 0x%04X", command);
    this->set_last_error_(ErrorCode::COMMUNICATION_FAILED);
    return false;
  }
  return true;
}

bool STCC4Component::read_data_(uint8_t *data, uint8_t len) {
  if (len == 0 || len % 3 != 0) {
    ESP_LOGE(TAG, "Invalid read length %u (must be non-zero multiple of 3)", len);
    this->set_last_error_(ErrorCode::UNKNOWN);
    return false;
  }

  if (this->read(data, len) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed");
    this->set_last_error_(ErrorCode::COMMUNICATION_FAILED);
    return false;
  }

  for (uint8_t i = 0; i < len; i += 3) {
    uint8_t crc = this->crc8_(&data[i], 2);
    if (crc != data[i + 2]) {
      ESP_LOGW(TAG, "CRC mismatch at position %u: expected 0x%02X, got 0x%02X", i, crc, data[i + 2]);
      this->set_last_error_(ErrorCode::CRC_MISMATCH);
      return false;
    }
  }

  return true;
}

uint8_t STCC4Component::crc8_(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;

  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = crc << 1;
      }
    }
  }

  return crc;
}

bool STCC4Component::start_continuous_measurement_() {
  if (this->write_command_(CMD_START_CONTINUOUS_MEASUREMENT)) {
    this->measurement_started_ = true;
    ESP_LOGD(TAG, "Continuous measurement started");
    return true;
  }
  return false;
}

bool STCC4Component::stop_continuous_measurement_() {
  if (this->write_command_(CMD_STOP_CONTINUOUS_MEASUREMENT)) {
    this->measurement_started_ = false;
    ESP_LOGD(TAG, "Continuous measurement stopped");
    return true;
  }
  return false;
}

bool STCC4Component::measure_single_shot_() {
  if (this->write_command_(CMD_MEASURE_SINGLE_SHOT)) {
    ESP_LOGD(TAG, "Single-shot measurement started");
    return true;
  }
  return false;
}

bool STCC4Component::enter_sleep_mode_() {
  if (this->write_command_(CMD_ENTER_SLEEP_MODE)) {
    this->measurement_started_ = false;
    ESP_LOGD(TAG, "Entered sleep mode");
    return true;
  }
  return false;
}

bool STCC4Component::exit_sleep_mode_() {
  uint8_t data[2] = {0x00, 0x00};
  return this->write(data, 2) == i2c::ERROR_OK;
}

bool STCC4Component::set_pressure_compensation_(uint16_t pressure) {
  uint16_t args[] = {pressure};
  return this->write_command_with_args_(CMD_SET_PRESSURE_COMPENSATION, args, 1);
}

bool STCC4Component::set_rht_compensation_(uint16_t raw_temperature, uint16_t raw_humidity) {
  uint16_t args[] = {raw_temperature, raw_humidity};
  return this->write_command_with_args_(CMD_SET_RHT_COMPENSATION, args, 2);
}

void STCC4Component::set_last_error_(ErrorCode error) {
  this->last_error_ = error;
  if (this->error_text_sensor_ != nullptr) {
#ifdef USE_TEXT_SENSOR
    this->error_text_sensor_->publish_state(this->error_code_to_string_(error));
#endif
  }
}

const char *STCC4Component::error_code_to_string_(ErrorCode error) const {
  switch (error) {
    case ErrorCode::NONE:
      return "none";
    case ErrorCode::COMMUNICATION_FAILED:
      return "communication_failed";
    case ErrorCode::CRC_MISMATCH:
      return "crc_mismatch";
    case ErrorCode::MEASUREMENT_INVALID:
      return "measurement_invalid";
    case ErrorCode::UNKNOWN:
    default:
      return "unknown";
  }
}

const char *STCC4Component::setup_state_to_string_(SetupState state) const {
  switch (state) {
    case SetupState::SOFT_RESET:
      return "SOFT_RESET";
    case SetupState::POWER_UP:
      return "POWER_UP";
    case SetupState::STOP:
      return "STOP";
    case SetupState::IDENTIFY:
      return "IDENTIFY";
    case SetupState::IDENTIFY_READ:
      return "IDENTIFY_READ";
    case SetupState::SELF_TEST:
      return "SELF_TEST";
    case SetupState::SELF_TEST_READ:
      return "SELF_TEST_READ";
    case SetupState::CONFIGURE:
      return "CONFIGURE";
    case SetupState::START:
      return "START";
    case SetupState::SETTLE:
      return "SETTLE";
    case SetupState::RUNNING:
      return "RUNNING";
    case SetupState::FAILED_RETRY:
      return "FAILED_RETRY";
    default:
      return "UNKNOWN";
  }
}

float STCC4Component::calculate_pressure_from_altitude_(uint16_t altitude) {
  const float P0 = 101300.0f;
  const float L = 0.0065f;
  const float T0 = 288.15f;
  const float exponent = 5.25588f;

  return P0 * powf(1.0f - (L * altitude / T0), exponent);
}

}  // namespace stcc4
}  // namespace esphome
