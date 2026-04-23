#include "waves.hh"
#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <array>
#include <cstdlib>
#include <driver/dac_continuous.h>
#include <esp_log.h>

constexpr const char *TAG = "main";

// Produce 1 s of 48 kHz samples at a time
constexpr uint32_t SAMPLE_RATE = 48000;
constexpr size_t BUFFER_SIZE = SAMPLE_RATE;

std::array<uint8_t, BUFFER_SIZE> buffer{};

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initializing DAC");

  dac_continuous_handle_t dac_handle;
  dac_continuous_config_t dac_config = {
      .chan_mask = DAC_CHANNEL_MASK_CH0,
      .desc_num = 8,
      .buf_size = 2048,
      .freq_hz = SAMPLE_RATE,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  ESP_ERROR_CHECK(dac_continuous_new_channels(&dac_config, &dac_handle));
  ESP_ERROR_CHECK(dac_continuous_enable(dac_handle));

  ESP_LOGI(TAG, "DAC initialized");

  uint8_t wave_index = 0;
  void (*waves[])(uint8_t buffer[], size_t window_size, float sample_rate,
                  float bias, float amplitude, float offset) = {
      waves::generate_dummy_data_clean_a<uint8_t>,
      waves::generate_dummy_data_clean_b<uint8_t>,
      waves::generate_dummy_data_clean_c<uint8_t>,
      waves::generate_dummy_data_noisy<uint8_t>,
  };

  ESP_LOGI(TAG, "Playing wave %u at %u Hz", wave_index, SAMPLE_RATE);

  for (size_t i = 0;; i++) {
    TickType_t start = xTaskGetTickCount();
    waves[wave_index](buffer.data(), BUFFER_SIZE, SAMPLE_RATE, 1.0, 16.0, i);
    ESP_ERROR_CHECK(dac_continuous_write(dac_handle, buffer.data(),
                                         buffer.size(), NULL, -1));
    xTaskDelayUntil(&start, pdMS_TO_TICKS(1000));
  }
}
