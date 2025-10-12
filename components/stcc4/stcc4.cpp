#include "stcc4.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>

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
static const uint16_t CMD_ENTER_SLEEP_MODE = 0x3650;
static const uint16_t CMD_EXIT_SLEEP_MODE = 0x0000;

// Timing constants (in milliseconds)
static const uint16_t STOP_MEASUREMENT_DELAY = 1200;
static const uint16_t MEASUREMENT_DELAY = 1;
static const uint16_t SINGLE_SHOT_DELAY = 500;
static const uint16_t EXIT_SLEEP_DELAY = 5;

void STCC4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STCC4...");
  
  // Try to stop any ongoing measurement
  this->stop_continuous_measurement_();
  delay(STOP_MEASUREMENT_DELAY);
  
  // Verify communication by reading product ID
  uint8_t data[18];
  if (this->write_command_(CMD_GET_PRODUCT_ID)) {
    delay(MEASUREMENT_DELAY);
    if (this->read_data_(data, 18)) {
      uint32_t product_id = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | 
                            (uint32_t(data[3]) << 8) | uint32_t(data[4]);
      ESP_LOGD(TAG, "Product ID: 0x%08X", product_id);
    }
  }
  
  // Apply altitude compensation if set
  if (this->altitude_compensation_ > 0) {
    this->pressure_compensation_ = this->calculate_pressure_from_altitude_(this->altitude_compensation_);
    ESP_LOGD(TAG, "Calculated pressure from altitude %d m: %.0f Pa", 
             this->altitude_compensation_, this->pressure_compensation_);
  }
  
  // Set pressure compensation
  if (this->pressure_compensation_ != 101325.0f) {
    uint16_t pressure_raw = static_cast<uint16_t>(this->pressure_compensation_ / 2.0f);
    if (this->set_pressure_compensation_(pressure_raw)) {
      ESP_LOGD(TAG, "Pressure compensation set to %.0f Pa", this->pressure_compensation_);
    } else {
      ESP_LOGW(TAG, "Failed to set pressure compensation");
    }
  }
  
  // Start continuous measurement
  if (!this->start_continuous_measurement_()) {
    ESP_LOGE(TAG, "Failed to start continuous measurement");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "STCC4 setup complete");
}

void STCC4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "STCC4:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "  Pressure Compensation: %.0f Pa", this->pressure_compensation_);
  if (this->altitude_compensation_ > 0) {
    ESP_LOGCONFIG(TAG, "  Altitude Compensation: %d m", this->altitude_compensation_);
  }
}

void STCC4Component::update() {
  if (!this->measurement_started_) {
    if (!this->start_continuous_measurement_()) {
      ESP_LOGW(TAG, "Failed to restart continuous measurement");
      return;
    }
  }
  
  if (!this->read_measurement_()) {
    ESP_LOGW(TAG, "Failed to read measurement, retrying...");
    this->status_set_warning();
    
    // Retry after delay as recommended by datasheet
    delay(150);
    if (!this->read_measurement_()) {
      ESP_LOGE(TAG, "Repeated measurement failure");
      this->status_set_error();
    }
  }
}

float STCC4Component::get_setup_priority() const { 
  return setup_priority::DATA; 
}

bool STCC4Component::write_command_(uint16_t command) {
  uint8_t data[2];
  data[0] = (command >> 8) & 0xFF;
  data[1] = command & 0xFF;
  
  if (this->write(data, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write failed for command 0x%04X", command);
    return false;
  }
  return true;
}

bool STCC4Component::write_command_with_args_(uint16_t command, const uint16_t *args, uint8_t len) {
  uint8_t buffer[8];
  buffer[0] = (command >> 8) & 0xFF;
  buffer[1] = command & 0xFF;
  
  uint8_t pos = 2;
  for (uint8_t i = 0; i < len; i++) {
    buffer[pos++] = (args[i] >> 8) & 0xFF;
    buffer[pos++] = args[i] & 0xFF;
    buffer[pos++] = this->crc8_(&buffer[pos - 2], 2);
  }
  
  if (this->write(buffer, pos) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write with args failed for command 0x%04X", command);
    return false;
  }
  return true;
}

bool STCC4Component::read_data_(uint8_t *data, uint8_t len) {
  if (this->read(data, len) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed");
    return false;
  }
  
  // Verify CRC for each word (2 bytes data + 1 byte CRC)
  for (uint8_t i = 0; i < len; i += 3) {
    uint8_t crc = this->crc8_(&data[i], 2);
    if (crc != data[i + 2]) {
      ESP_LOGW(TAG, "CRC mismatch at position %d: expected 0x%02X, got 0x%02X", i, crc, data[i + 2]);
      return false;
    }
  }
  
  return true;
}

uint8_t STCC4Component::crc8_(const uint8_t *data, uint8_t len) {
  // CRC-8 with polynomial 0x31 (x^8 + x^5 + x^4 + 1)
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

bool STCC4Component::read_measurement_() {
  uint8_t data[12];  // 4 words of 3 bytes each (2 data + 1 CRC)
  
  if (!this->write_command_(CMD_READ_MEASUREMENT)) {
    return false;
  }
  
  delay(MEASUREMENT_DELAY);
  
  if (!this->read_data_(data, 12)) {
    return false;
  }
  
  // Parse CO2 concentration (signed int16)
  int16_t co2_raw = (int16_t(data[0]) << 8) | int16_t(data[1]);
  
  // Parse temperature (uint16)
  uint16_t temp_raw = (uint16_t(data[3]) << 8) | uint16_t(data[4]);
  float temperature = -45.0f + (175.0f * temp_raw / 65535.0f);
  
  // Parse humidity (uint16)
  uint16_t hum_raw = (uint16_t(data[6]) << 8) | uint16_t(data[7]);
  float humidity = -6.0f + (125.0f * hum_raw / 65535.0f);
  
  // Sensor status
  uint16_t status = (uint16_t(data[9]) << 8) | uint16_t(data[10]);
  
  // Log values
  ESP_LOGD(TAG, "CO2: %d ppm, Temperature: %.2f°C, Humidity: %.2f%%, Status: 0x%04X", 
           co2_raw, temperature, humidity, status);
  
  // Publish sensor values
  if (this->co2_sensor_ != nullptr) {
    this->co2_sensor_->publish_state(co2_raw);
  }
  
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature);
  }
  
  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(humidity);
  }
  
  this->status_clear_warning();
  this->status_clear_error();
  return true;
}

bool STCC4Component::set_pressure_compensation_(uint16_t pressure) {
  uint16_t args[] = {pressure};
  bool result = this->write_command_with_args_(CMD_SET_PRESSURE_COMPENSATION, args, 1);
  if (result) {
    delay(MEASUREMENT_DELAY);
  }
  return result;
}

float STCC4Component::calculate_pressure_from_altitude_(uint16_t altitude) {
  // Barometric formula for pressure at altitude
  // P = P0 * (1 - L * h / T0)^(g * M / (R * L))
  // where:
  //   P0 = sea level pressure (101325 Pa)
  //   L = temperature lapse rate (0.0065 K/m)
  //   h = altitude (m)
  //   T0 = sea level standard temperature (288.15 K)
  //   g = gravitational acceleration (9.80665 m/s²)
  //   M = molar mass of air (0.0289644 kg/mol)
  //   R = universal gas constant (8.31447 J/(mol·K))
  
  const float P0 = 101325.0f;
  const float L = 0.0065f;
  const float T0 = 288.15f;
  const float exponent = 5.25588f;  // g * M / (R * L)
  
  float pressure = P0 * powf(1.0f - (L * altitude / T0), exponent);
  return pressure;
}

}  // namespace stcc4
}  // namespace esphome

