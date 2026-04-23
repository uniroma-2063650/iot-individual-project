#pragma once

#include <esp_log.h>
#include <utility>

namespace fft_noise_reduction {

constexpr const char *TAG = "FFT";

template <typename T, typename Size> struct ZScoreFilter {
private:
  // std::vector<Size> outliers;

public:
  void calc(T buffer[], Size size, const T z_score_threshold = 3) {
    // outliers.empty();
    T avg = 0.0;
    for (Size i = 0; i < size; i++) {
      avg += buffer[i];
    }
    avg /= size;
    T square_dev_sum = 0.0;
    for (Size i = 0; i < size; i++) {
      const T dev = buffer[i] - avg;
      square_dev_sum += dev * dev;
    }
    const T stddev = sqrt(square_dev_sum / size);
    ESP_LOGI(TAG, "Mean: %f, stddev: %f", avg, stddev);
    const T threshold = z_score_threshold * stddev;
    const T min = avg - threshold;
    const T max = avg + threshold;
    for (Size i = 0; i < size; i++) {
      const T value = buffer[i];
      if (value > min && value < max)
        continue;
      // outliers.push_back(i);
      // Replace with average of the closest non-outlier left and right values
      Size ri = i;
      for (; ri < size - 1; ri++) {
        const T rvalue = buffer[ri + 1];
        if (rvalue > min && rvalue < max)
          break;
      }
      T left, right;
      if (i == 0) {
        right = ri >= size - 1 ? avg : buffer[ri + 1];
        left = right;
      } else {
        left = buffer[i - 1];
        right = ri >= size - 1 ? left : buffer[ri + 1];
      }
      ESP_LOGV(TAG, "Replacing outliers %zu...%zu", i, ri);
      const T replacement = (left + right) * 0.5;
      for (Size j = i; j <= ri; j++) {
        buffer[j] = replacement;
      }
      i = ri;
    }
    // return outliers;
  }
};

template <typename T, typename Size, size_t window_size> struct HampelFilter {
private:
  // std::vector<Size> outliers;

  // From https://bertdobbelaere.github.io/sorting_networks.html
  static void sort(T values[9]) {
#define SWAP(i, j)                                                             \
  if (values[i] > values[j]) {                                                 \
    std::swap(values[i], values[j]);                                           \
  }
    switch (window_size) {
    case 5: {
      SWAP(0, 3) SWAP(1, 4) SWAP(0, 2) SWAP(1, 3) SWAP(0, 1) SWAP(2, 4);
      SWAP(1, 2) SWAP(3, 4) SWAP(2, 3);
      break;
    }
    case 7: {
      SWAP(0, 6) SWAP(2, 3) SWAP(4, 5) SWAP(0, 2) SWAP(1, 4) SWAP(3, 6);
      SWAP(0, 1) SWAP(2, 5) SWAP(3, 4) SWAP(1, 2) SWAP(4, 6) SWAP(2, 3);
      SWAP(4, 5) SWAP(1, 2) SWAP(3, 4) SWAP(5, 6);
      break;
    }
    case 9: {
      SWAP(0, 3) SWAP(1, 7) SWAP(2, 5) SWAP(4, 8) SWAP(0, 7) SWAP(2, 4);
      SWAP(3, 8) SWAP(5, 6) SWAP(0, 2) SWAP(1, 3) SWAP(4, 5) SWAP(7, 8);
      SWAP(1, 4) SWAP(3, 6) SWAP(5, 7) SWAP(0, 1) SWAP(2, 4) SWAP(3, 5);
      SWAP(6, 8) SWAP(2, 3) SWAP(4, 5) SWAP(6, 7) SWAP(1, 2) SWAP(3, 4);
      SWAP(5, 6);
      break;
    }
    case 11: {
      SWAP(0, 9) SWAP(1, 6) SWAP(2, 4) SWAP(3, 7) SWAP(5, 8) SWAP(0, 1);
      SWAP(3, 5) SWAP(4, 10) SWAP(6, 9) SWAP(7, 8) SWAP(1, 3) SWAP(2, 5);
      SWAP(4, 7) SWAP(8, 10) SWAP(0, 4) SWAP(1, 2) SWAP(3, 7) SWAP(5, 9);
      SWAP(6, 8) SWAP(0, 1) SWAP(2, 6) SWAP(4, 5) SWAP(7, 8) SWAP(9, 10);
      SWAP(2, 4) SWAP(3, 6) SWAP(5, 7) SWAP(8, 9) SWAP(1, 2) SWAP(3, 4);
      SWAP(5, 6) SWAP(7, 8) SWAP(2, 3) SWAP(4, 5) SWAP(6, 7);
      break;
    }
    }
#undef SWAP
  }

  static bool replace_hampel(T window[window_size], T *value,
                             const T mad_threshold) {
    const T x = window[window_size / 2];
    sort(window);
    const T median = window[window_size / 2];
    for (Size i = 0; i < window_size; i++) {
      window[i] -= median;
      if (window[i] < 0) {
        window[i] = -window[i];
      }
    }
    sort(window);
    const T mad = window[window_size / 2];
    if (abs(x - median) > mad_threshold * mad) {
      *value = median;
      return true;
    } else {
      return false;
    }
  }

public:
  void calc(const T prev[window_size / 2], T buffer[],
            const T next[window_size / 2], Size size,
            const T mad_threshold = 3) {
    // outliers.empty();
    T window[window_size];
    for (Size i = 0; i < window_size / 2; i++) {
      const Size window_prev_end = window_size / 2 - i;
      memcpy(window, prev + i, window_prev_end * sizeof(T));
      memcpy(window + window_prev_end, buffer,
             (window_size - window_prev_end) * sizeof(T));
      if (replace_hampel(window, &buffer[i], mad_threshold)) {
        ESP_LOGV(TAG, "Replacing outlier -%zu", window_size - 1 - i);
        // outliers.push_back(i - (window_size - 1));
      }
    }
    for (Size i = 0; i <= size - window_size; i++) {
      memcpy(window, buffer + i, window_size * sizeof(T));
      if (replace_hampel(window, &buffer[i + window_size / 2], mad_threshold)) {
        ESP_LOGV(TAG, "Replacing outlier %zu", i + window_size / 2);
        // outliers.push_back(i);
      }
    }
    for (Size i = 0; i < window_size / 2; i++) {
      const Size window_buffer_end = window_size - (i + 1);
      memcpy(window, buffer + size - window_buffer_end,
             window_buffer_end * sizeof(T));
      memcpy(window + window_buffer_end, next + i,
             (window_size - window_buffer_end) * sizeof(T));
      if (replace_hampel(window, &buffer[size - window_size / 2 + i],
                         mad_threshold)) {
        ESP_LOGV(TAG, "Replacing outlier -%zu", size - window_size / 2 + i);
        // outliers.push_back(i - (window_size - 1));
      }
    }
    // return outliers;
  }
};

} // namespace fft_noise_reduction
