#include "data.hh"
#include "fft/analysis.hh"
#include "lora.hh"
#include "mqtt.hh"
#include "waves.hh"
#include <FreeRTOS/FreeRTOS.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <esp_adc/adc_continuous.h>
#include <soc/adc_channel.h>
#include <soc/soc_caps.h>

constexpr const char *TAG = "main";

template <typename F, typename T> constexpr T ceil_const(F num) {
  return (static_cast<F>(static_cast<T>(num)) == num)
             ? static_cast<T>(num)
             : static_cast<T>(num) + ((num > 0) ? 1 : 0);
}

template <typename T>
T lowest_multiple_above(T num, T threshold, uint16_t *factor) {
  T result = num;
  uint16_t factor_ = 1;
  while (result < threshold) {
    result += num;
    factor_++;
  }
  if (factor)
    *factor = factor_;
  return result;
}

// Initialize by running at 16.384 kHz with a 16384-sample window
// This means taking:
// - 512 B of ADC sample memory (128 samples * 4 B/sample for
//   adc_continuous_data_t)
// - 1 KiB for ADC DMA buffers (256 samples * 4 B/sample)
// - 128 KiB of float sample memory ((16384 samples * 4 B/sample) * double
// buffer)
// - 64 KiB of additional FFT processing memory (16384 floats for complex parts)
// - 1 s to collect the starting samples
// However, it supports frequencies in the 1 Hz-8 kHz range (the highest note
// on a piano is 4186 Hz according to
// https://en.wikipedia.org/wiki/Audio_frequency)
constexpr double SAMPLE_RATE_INIT = 16384.0;
constexpr double MIN_HZ = 1.0;
constexpr size_t ADC_WINDOW_SIZE_MAX = 128;
constexpr size_t WINDOW_SIZE_INIT =
    std::bit_ceil(ceil_const<double, size_t>(SAMPLE_RATE_INIT / MIN_HZ));

std::array<adc_continuous_data_t, ADC_WINDOW_SIZE_MAX> adc_data_buffer{};
std::array<std::array<Sample, WINDOW_SIZE_INIT + fft_analysis::PREV_DATA_SIZE +
                                  fft_analysis::NEXT_DATA_SIZE>,
           2>
    buffer{};
std::array<Sample, WINDOW_SIZE_INIT> buffer_complex{};

struct FFTState {
  fft_analysis::FFTAnalysis<Sample> analysis{fft_analysis::FFTAnalysisOptions{
      .sample_rate_hz = SAMPLE_RATE_INIT,
      .window_size = WINDOW_SIZE_INIT,
      .filter = fft_analysis::FFTAnalysisFilter::Hampel,
      .aggregation = fft_analysis::FFTAnalysisAggregation::TumblingWindow,
      .aggregation_window_size = 128,
  }};
  Sample sample_rate_hz = SAMPLE_RATE_INIT;
  uint16_t window_size = WINDOW_SIZE_INIT;

  void process(size_t buffer_i) {
    memcpy(buffer[buffer_i ^ 1].data(), buffer[buffer_i].data() + window_size,
           (fft_analysis::PREV_DATA_SIZE + fft_analysis::NEXT_DATA_SIZE) *
               sizeof(Sample));
    analysis.set_options(fft_analysis::FFTAnalysisOptions{
        .sample_rate_hz = sample_rate_hz,
        .window_size = window_size,
        .filter = fft_analysis::FFTAnalysisFilter::Hampel,
        .aggregation = fft_analysis::FFTAnalysisAggregation::TumblingWindow,
        .aggregation_window_size = 128,
    });
    analysis.process(buffer[buffer_i].data(),
                     buffer[buffer_i].data() + fft_analysis::PREV_DATA_SIZE,
                     buffer[buffer_i].data() + fft_analysis::PREV_DATA_SIZE +
                         window_size,
                     buffer_complex.data());
  }

  void update_sample_rate() {
    const size_t optimal_window_size =
        std::max(std::bit_ceil(ceil_const<Sample, size_t>(
                     analysis.result.optimal_sample_rate_hz / MIN_HZ)),
                 (size_t)64);
    sample_rate_hz = analysis.result.optimal_sample_rate_hz;
    window_size = optimal_window_size;
    ESP_LOGI(TAG, "Using sample rate of %f Hz and window size of %zu samples",
             sample_rate_hz, window_size);
  }
};

constexpr bool USE_MQTT = false;
constexpr bool SWITCH_TO_OPTIMAL = false;
constexpr uint8_t WAVE_INDEX = 0;

static FFTState fft_state;

static Mqtt *mqtt = nullptr;
static LoRa *lora = nullptr;
static TaskHandle_t fft_task;
static TaskHandle_t collect_task;

void process_fft(void *args) {
  size_t buffer_i = 0;
  for (size_t window_i = 0;; window_i++, buffer_i ^= 1) {
    if (window_i != 0 || !SWITCH_TO_OPTIMAL) {
      xTaskNotifyGive(collect_task);
    }
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Processing buffer %zu", buffer_i);
    fft_state.process(buffer_i);
    if (!fft_state.analysis.result.aggregation_results.empty()) {
      ESP_LOGI(TAG, "Sending aggregation results to publisher thread");

      const size_t size = fft_state.analysis.result.aggregation_results.size();
      Sample *const aggregation_results_copy =
          (Sample *)malloc(size * sizeof(Sample));
      assert(aggregation_results_copy);
      memcpy(aggregation_results_copy,
             fft_state.analysis.result.aggregation_results.data(),
             size * sizeof(Sample));

      const float end_seconds = pdTICKS_TO_MS(xTaskGetTickCount()) /
                                1000.0; // TODO: Use esp_timer instead;
      const AggregationData data{
          .end_seconds = end_seconds,
          .seconds_per_value =
              1.0f / fft_state.sample_rate_hz *
              fft_state.analysis.options.aggregation_window_size,
          .size = (uint32_t)size,
          .values = aggregation_results_copy};

      if (mqtt) {
        mqtt->send_aggregate_data(data);
      }
      if (lora) {
        lora->send_aggregate_data(data);
      }
    }
    if (window_i == 0 && SWITCH_TO_OPTIMAL) {
      fft_state.update_sample_rate();
      ESP_LOGI(TAG, "Processing done for buffer %zu", buffer_i);
      xTaskNotifyGive(collect_task);
    } else {
      ESP_LOGI(TAG, "Processing done for buffer %zu", buffer_i);
    }
  }
}

adc_continuous_handle_t init_adc(float sample_rate_hz, uint16_t buffer_size) {
  const adc_continuous_handle_cfg_t adc_handle_config = {
      .max_store_buf_size =
          (uint32_t)buffer_size * 2 *
          SOC_ADC_DIGI_DATA_BYTES_PER_CONV, // Keep at least twice the amount
                                            // that can be processed at once
      .conv_frame_size = SOC_ADC_DIGI_DATA_BYTES_PER_CONV,
      .flags = {
          .flush_pool = false,
      }};
  adc_continuous_handle_t adc_handle;
  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_handle_config, &adc_handle));
  adc_digi_pattern_config_t adc_patterns[] = {adc_digi_pattern_config_t{
      .atten = ADC_ATTEN_DB_12,
      .channel = ADC1_GPIO1_CHANNEL,
      .unit = ADC_UNIT_1,
      .bit_width = ADC_BITWIDTH_12, // Input in the 0 V-3.3 V voltage range
  }};
  const adc_continuous_config_t adc_config = {
      .pattern_num = 1,
      .adc_pattern = adc_patterns,
      .sample_freq_hz = (uint32_t)round(sample_rate_hz),
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2};
  ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &adc_config));
  return adc_handle;
}

void collect_adc(void *args) {
  buffer[0].fill(0);
  buffer[1].fill(0);

  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

  uint16_t sample_rate_factor = 0;
  float fft_sample_rate_hz = fft_state.sample_rate_hz;
  uint16_t fft_window_size = fft_state.window_size;
  float sample_rate_hz = lowest_multiple_above(
      fft_sample_rate_hz, (float)SOC_ADC_SAMPLE_FREQ_THRES_LOW,
      &sample_rate_factor);
  uint16_t buffer_size = std::min<uint16_t>(
      fft_state.window_size * sample_rate_factor, ADC_WINDOW_SIZE_MAX);
  ESP_LOGI(TAG, "ADC settings: %f Hz (%ux FFT), %zu B buffer", sample_rate_hz,
           sample_rate_factor, buffer_size);

  adc_continuous_handle_t adc_handle = init_adc(sample_rate_hz, buffer_size);
  ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

  size_t read_samples = 0;
  size_t buffer_i = 0;
  size_t skipped_samples = 0;
  ESP_LOGI(TAG, "Collecting for buffer %zu", buffer_i);
  for (;;) {
    uint32_t new_samples;
    uint32_t max_remaining = std::min<uint32_t>(
        buffer_size, (fft_window_size - read_samples) * sample_rate_factor -
                         skipped_samples);
    esp_err_t result = adc_continuous_read_parse(
        adc_handle, adc_data_buffer.data(), max_remaining, &new_samples,
        pdMS_TO_TICKS(2000.0 / MIN_HZ));
    if (result == ESP_ERR_TIMEOUT) {
      ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
      ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
      continue;
    }
    ESP_ERROR_CHECK(result);
    for (size_t i = 0; i < new_samples; i++) {
      if (++skipped_samples < sample_rate_factor) {
        continue;
      }
      skipped_samples = 0;
      adc_continuous_data_t data = adc_data_buffer[i];
      buffer[buffer_i][read_samples + fft_analysis::PREV_DATA_SIZE +
                       fft_analysis::NEXT_DATA_SIZE] =
          data.valid
              ? (Sample)((int16_t)data.raw_data - (1 << 11)) / (Sample)(1 << 11)
              : 0.0;
      read_samples++;
    }
    if (read_samples >= fft_window_size) {
      ESP_LOGI(TAG, "Collecting done for buffer %zu", buffer_i);
      xTaskNotifyGive(fft_task);
      ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
      if (fft_state.sample_rate_hz != fft_sample_rate_hz ||
          fft_state.window_size != fft_window_size) {
        fft_sample_rate_hz = fft_state.sample_rate_hz;
        fft_window_size = fft_state.window_size;
        sample_rate_hz = lowest_multiple_above(
            fft_sample_rate_hz, (float)SOC_ADC_SAMPLE_FREQ_THRES_LOW,
            &sample_rate_factor);
        buffer_size = std::min<uint16_t>(
            fft_state.window_size * sample_rate_factor, ADC_WINDOW_SIZE_MAX);
        skipped_samples = 0;
        ESP_LOGI(TAG, "ADC settings: %f Hz (%ux FFT), %zu B buffer",
                 sample_rate_hz, sample_rate_factor, buffer_size);
        ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
        ESP_ERROR_CHECK(adc_continuous_deinit(adc_handle));
        adc_handle = init_adc(sample_rate_hz, buffer_size);
        ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
      }
      buffer_i ^= 1;
      read_samples = 0;
      ESP_LOGI(TAG, "Collecting for buffer %zu", buffer_i);
    }
  }
}

void collect_dummy(void *args) {
  srand(0);
  buffer[0].fill(0);
  buffer[1].fill(0);

  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

  float sample_rate_hz = fft_state.sample_rate_hz;
  uint16_t window_size = fft_state.window_size;

  void (*waves[])(Sample buffer[], size_t window_size, float sample_rate,
                  float bias, float amplitude, float offset) = {
      waves::generate_dummy_data_clean_a<Sample>,
      waves::generate_dummy_data_clean_b<Sample>,
      waves::generate_dummy_data_clean_c<Sample>,
      waves::generate_dummy_data_noisy<Sample>,
  };

  float offset = 0.0;
  size_t buffer_i = 0;
  TickType_t start = xTaskGetTickCount();
  for (;;) {
    ESP_LOGI(TAG, "Collecting for buffer %zu", buffer_i);
    ESP_LOGI(TAG, "Generating at %f Hz, %zu window size, offset %f s",
             sample_rate_hz, window_size, offset);
    waves[WAVE_INDEX](buffer[buffer_i].data() + fft_analysis::PREV_DATA_SIZE +
                          fft_analysis::NEXT_DATA_SIZE,
                      window_size, sample_rate_hz, 127.5 * 0.825, 16.0 * 0.825,
                      offset);
    offset += (float)window_size / sample_rate_hz;
    ESP_LOGI(TAG, "Collecting done for buffer %zu", buffer_i);

    xTaskNotifyGive(fft_task);
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    if (fft_state.sample_rate_hz != sample_rate_hz ||
        fft_state.window_size != window_size) {
      sample_rate_hz = fft_state.sample_rate_hz;
      window_size = fft_state.window_size;
    }
    buffer_i ^= 1;

    xTaskDelayUntil(&start, pdMS_TO_TICKS(1000));
  }
}

extern "C" void app_main(void) {
  if constexpr (USE_MQTT) {
    mqtt = new Mqtt();
  } else {
    lora = new LoRa(0);
  }
  xTaskCreate(collect_dummy, "collect", 4096, nullptr, 5, &collect_task);
  xTaskCreate(process_fft, "fft", 4096, nullptr, 5, &fft_task);
  xTaskNotifyGive(collect_task);
}
