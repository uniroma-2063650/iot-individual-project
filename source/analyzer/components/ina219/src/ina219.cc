#include "ina219.hh"
#include "ina219_regs.hh"
#include <FreeRTOS/FreeRTOS.h>
#include <array>
#include <esp_log.h>

namespace regs = ina219_regs;

#undef TAG
constexpr const char *TAG = "INA219";

constexpr double BUS_VOLTAGE_LSB = 0.004;
constexpr double SHUNT_VOLTAGE_LSB = 0.00001;

constexpr double MAX_BUS_VOLTAGE = 3.7;
constexpr double MAX_SHUNT_VOLTAGE = 0.32;
constexpr double R_SHUNT = 0.1;

constexpr double MAX_POSSIBLE_CURRENT = (MAX_SHUNT_VOLTAGE / R_SHUNT);

constexpr double MAX_EXPECTED_CURRENT = 2;

constexpr double MIN_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 32767.0);
constexpr double MAX_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 4096.0);

constexpr double CURRENT_LSB = 0.000075;
static_assert(CURRENT_LSB >= MIN_CURRENT_LSB);
static_assert(CURRENT_LSB <= MAX_CURRENT_LSB);

constexpr double POWER_LSB = (5000.0 * BUS_VOLTAGE_LSB * CURRENT_LSB);

constexpr uint16_t CAL_VALUE = 0.04096 / (CURRENT_LSB * R_SHUNT);

constexpr double MAX_CURRENT_BEFORE_OVF = (CURRENT_LSB * 32767.0);
constexpr double MAX_SHUNT_VOLTAGE_BEFORE_OVF =
    (MAX_CURRENT_BEFORE_OVF * R_SHUNT);
constexpr double MAX_POWER_BEFORE_OVF =
    (MAX_CURRENT_BEFORE_OVF * MAX_BUS_VOLTAGE);

INA219::INA219(i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
  const i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = i2c_addr,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags{.disable_ack_check = false},
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_config, &this->dev));
  reset();
}

float INA219::read_current() {
  return (float)read_reg(regs::CURRENT) / (float)CURRENT_LSB;
}

void INA219::reset() {
  ESP_LOGI(TAG, "Resetting...");
  for (int i = 0; i < 2; i++) {
    esp_err_t result = write_reg_checked(regs::CONFIGURATION,
                                         regs::Configuration{
                                             .reset = true,
                                             .bus_voltage_range = 0,
                                             .pga_gain = 0,
                                             .bus_adc_mode = 0,
                                             .shunt_adc_mode = 0,
                                             .mode = 0,
                                         }
                                             .raw_value);
    if (result == ESP_ERR_INVALID_STATE && i == 0) {
      continue;
    } else if (result == ESP_OK) {
      break;
    } else {
      ESP_ERROR_CHECK(result);
    }
  }
  while (
      regs::Configuration{.raw_value = read_reg(regs::CONFIGURATION)}.reset) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_LOGI(TAG, "Reset");
  write_reg(
      regs::CONFIGURATION,
      regs::Configuration{
          .reset = false,
          .bus_voltage_range = 0,   // 16 V bus voltage FSR (powered at 3.3 V)
          .pga_gain = 3,            // ±320 mV shunt voltage FSR
          .bus_adc_mode = 0,        // Unused
          .shunt_adc_mode = 0b1111, // 128 12-bit sample average, every 68.1 ms
          .mode = 0b101,            // Shunt only
      }
          .raw_value);
  write_reg(regs::CALIBRATION, CAL_VALUE);
}

uint16_t INA219::read_reg(const uint8_t reg_addr) {
  ESP_LOGD(TAG, "Reading from %02X", reg_addr);
  const std::array<uint8_t, 1> write_data{reg_addr};
  std::array<uint8_t, 2> read_data{};
  ESP_ERROR_CHECK(i2c_master_transmit_receive(
      this->dev, write_data.data(), write_data.size(), read_data.data(),
      read_data.size(), this->transfer_timeout_ms));
  const uint16_t result = ((uint16_t)read_data[0] << 8) | read_data[0];
  ESP_LOGD(TAG, "Read from %02X: %04X", reg_addr, result);
  return result;
}

esp_err_t INA219::write_reg_checked(const uint8_t reg_addr,
                                    const uint16_t data) {
  ESP_LOGI(TAG, "Writing %04X to %02X", data, reg_addr);
  const std::array<uint8_t, 3> write_data{reg_addr, (uint8_t)(data >> 8),
                                          (uint8_t)data};
  return i2c_master_transmit(this->dev, write_data.data(), write_data.size(),
                             this->transfer_timeout_ms);
}

void INA219::write_reg(const uint8_t reg_addr, const uint16_t data) {
  ESP_ERROR_CHECK(write_reg_checked(reg_addr, data));
}
