#pragma once

#include "arduinoFFT.h"
#include "fft/aggregation.hh"
#include "fft/noise_reduction.hh"
#include <algorithm>
#include <cstdlib>
#include <esp_log.h>
#include <vector>

namespace fft_analysis {

constexpr const char *TAG = "FFT";

// template <typename T>
// std::array<T, 3> solve3x3(const std::array<std::array<T, 3>, 3> a,
//                           const std::array<T, 3> b) {
//   // Based on
//   //
//   https://en.wikipedia.org/wiki/Invertible_matrix#Inversion_of_3_%C3%97_3_matrices
//   const T adj00 = a[1][1] * a[2][2] - a[1][2] * a[2][1];
//   const T adj01 = a[1][2] * a[2][0] - a[1][0] * a[2][2];
//   const T adj02 = a[1][0] * a[2][1] - a[1][1] * a[2][0];
//   const T adj10 = a[0][2] * a[2][1] - a[0][1] * a[2][2];
//   const T adj11 = a[0][0] * a[2][2] - a[0][2] * a[2][0];
//   const T adj12 = a[0][1] * a[2][0] - a[0][0] * a[2][1];
//   const T adj20 = a[0][1] * a[1][2] - a[0][2] * a[1][1];
//   const T adj21 = a[0][2] * a[1][0] - a[0][0] * a[1][2];
//   const T adj22 = a[0][0] * a[1][1] - a[0][1] * a[1][0];
//   // Use Laplace expansion across the first column
//   const T inv_det = 1.0 / (a[0][0] * adj00 + a[1][0] * adj10 + a[2][0] *
//   adj20); return {
//       (b[0] * adj00 + b[1] * adj10 + b[2] * adj20) * inv_det,
//       (b[0] * adj01 + b[1] * adj11 + b[2] * adj21) * inv_det,
//       (b[0] * adj02 + b[1] * adj12 + b[2] * adj22) * inv_det,
//   };
// }

// template <typename T>
// std::array<T, 3> solve_parabola(const std::array<T, 3> x,
//                                 const std::array<T, 3> y) {
// //   return solve3x3(
// //       {
// //           std::array{x[0] * x[0], x[0], (T)1},
// //           std::array{x[1] * x[1], x[1], (T)1},
// //           std::array{x[2] * x[2], x[2], (T)1},
// //       },
// //       y);

//   // Optimized version of solve3x3 considering:
//   //     | x0^2   x0    1   |       | y0 |
//   // a = | x1^2   x1    1   |,  b = | y1 |
//   //     | x2^2   x2    1   |       | y2 |
//   std::array<T, 3> x2;
//   for (size_t i = 0; i < x.size(); i++) {
//     x2[i] = x[i] * x[i];
//   }
//   const T adj00 = x[1] - x[2];
//   const T adj01 = x2[2] - x2[1];
//   const T adj02 = x2[1] * x[2] - x[1] * x2[2];
//   const T adj10 = x[2] - x[0];
//   const T adj11 = x2[0] - x2[2];
//   const T adj12 = x[0] * x2[2] - x2[0] * x[2];
//   const T adj20 = x[0] - x[1];
//   const T adj21 = x2[1] - x2[0];
//   const T adj22 = x2[0] * x[1] - x[0] * x2[1];
//   // Use Laplace expansion across the last column
//   const T inv_det = 1.0 / (adj02 + adj12 + adj22);
//   return {
//       (y[0] * adj00 + y[1] * adj10 + y[2] * adj20) * inv_det,
//       (y[0] * adj01 + y[1] * adj11 + y[2] * adj21) * inv_det,
//       (y[0] * adj02 + y[1] * adj12 + y[2] * adj22) * inv_det,
//   };
// }

template <typename T>
T find_parabola_vertex_x(const std::array<T, 3> x, const std::array<T, 3> y) {
  // y = ax^2 + bx + c =
  //   = a(x^2 + (b/a)x + c/a) =
  //   = a(x^2 + 2(b/2a)x + (b/2a)^2 + (c/a - (b/2a)^2)) =
  //   = a(x + (b/2a))^2 + (c - ((b/2)^2)/a)
  // Therefore, vertex_x = -b/2a
  // (We don't care about the magnitude of the vertex, only its X coordinate)

  //   const auto [a, b, c] = solve_parabola(x, y);
  //   ESP_LOGI(TAG, "Parabola: (%f)x^2 + (%f)x + (%f)", a, b, c);
  //   // A convex parabola isn't what we're looking for (peaks form concave
  //   ones) if (a >= 0)
  //     return -1;
  //   return -b / (2.0 * a);

  // Further optimized version of solve_parabola since c is unused:
  std::array<T, 3> x2;
  for (size_t i = 0; i < x.size(); i++) {
    x2[i] = x[i] * x[i];
  }
  const T adj00 = x[1] - x[2];
  const T adj01 = x2[2] - x2[1];
  const T adj10 = x[2] - x[0];
  const T adj11 = x2[0] - x2[2];
  const T adj20 = x[0] - x[1];
  const T adj21 = x2[1] - x2[0];
  // Use Laplace expansion across the first column
  const T inv_det = 1.0 / (x2[0] * adj00 + x2[1] * adj10 + x2[2] * adj20);
  const T a = (y[0] * adj00 + y[1] * adj10 + y[2] * adj20) * inv_det;
  const T b = (y[0] * adj01 + y[1] * adj11 + y[2] * adj21) * inv_det;
  ESP_LOGI(TAG, "Parabola: (%f)x^2 + (%f)x + ...", a, b);
  // A convex parabola isn't what we're looking for (peaks form concave ones);
  // this shouldn't happen but check for it anyway
  if (a >= 0)
    return -1;
  return -b / (2.0 * a);
}

template <typename T> T find_last_peak_freq(T buffer[], size_t size) {
  T avg = 0.0;
  T max = 0.0;
  for (size_t i = 0; i < size; i++) {
    const T value = buffer[i];
    avg += value;
    max = max > value ? max : value;
  }
  avg /= size;
  ESP_LOGI(TAG, "FFT avg: %f, max: %f", avg, max);
  T threshold = avg + (max - avg) * 0.1;

  for (size_t i = size; i-- > 0;) {
    const T value = buffer[i];
    if (value < threshold)
      continue;
    const T prev = i == 0 ? 0 : buffer[i - 1];
    const T next = i >= size - 1 ? 0 : buffer[i + 1];
    if (value < prev || value < next)
      continue;

    // Peak found, apply parabolic interpolation (y = ax^2 + bx + c, y = a(x^2 +
    // 2(b/2a)x + (c/a))
    if (i == size - 1 || i == 0) {
      ESP_LOGI(TAG, "Peak found at %zu", i);
      return i;
    } else {
      ESP_LOGI(TAG, "Peak found around %zu: (%zu, %f) (%zu, %f) (%zu, %f)", i,
               i - 1, prev, i, value, i + 1, next);
      T peak_x = find_parabola_vertex_x({(T)(i - 1), (T)i, (T)(i + 1)},
                                        std::array{prev, value, next});
      return peak_x > 0 && peak_x < size - 1 ? peak_x : i;
    }
  }
  return -1;
}

enum class FFTAnalysisFilter {
  None,
  ZScore,
  Hampel,
};

enum class FFTAnalysisAggregation {
  TumblingWindow,
  SlidingWindow,
};

struct FFTAnalysisOptions {
  float sample_rate_hz;
  uint16_t window_size;
  FFTAnalysisFilter filter;
  FFTAnalysisAggregation aggregation;
  uint16_t aggregation_window_size;
};

template <typename T> struct FFTAnalysisResult {
  std::vector<T> aggregation_results;
  T last_peak_hz;
  T optimal_sample_rate_hz;
};

constexpr uint8_t HAMPEL_WINDOW_SIZE = 9;
constexpr float Z_SCORE_THRESHOLD = 1.75f;
constexpr float HAMPEL_THRESHOLD = 1.0f;
constexpr uint8_t PREV_DATA_SIZE = HAMPEL_WINDOW_SIZE / 2;
constexpr uint8_t NEXT_DATA_SIZE = HAMPEL_WINDOW_SIZE / 2;

template <typename T> struct FFTAnalysis {
  FFTAnalysisOptions options;
  FFTAnalysisResult<T> result;
  fft_aggregation::TumblingWindowAverage<T> tumbling_window_avg;
  fft_aggregation::SlidingWindowAverage<T> sliding_window_avg;

  FFTAnalysis(FFTAnalysisOptions options)
      : options(options), tumbling_window_avg(options.aggregation_window_size),
        sliding_window_avg(options.aggregation_window_size) {}

  void set_options(FFTAnalysisOptions options) {
    if (options.aggregation_window_size !=
        this->options.aggregation_window_size) {
      tumbling_window_avg.set_window_size(options.aggregation_window_size);
      sliding_window_avg.set_window_size(options.aggregation_window_size);
    }
    if (options.sample_rate_hz != this->options.sample_rate_hz ||
        options.aggregation != this->options.aggregation) {
      tumbling_window_avg.clear();
      sliding_window_avg.clear();
    }
    this->options = options;
  }

  const FFTAnalysisResult<T> &process(const T prev_data[PREV_DATA_SIZE],
                                      T data[],
                                      const T next_data[NEXT_DATA_SIZE],
                                      T buffer_complex[]) {
    ESP_LOGI(TAG, "Window size: %zu, sample rate: %f Hz", options.window_size,
             options.sample_rate_hz);

    if (false && options.window_size < 256) {
      for (size_t i = 0; i < options.window_size; i++) {
        ESP_LOGI(TAG, "Raw: %f, %f", (T)i / options.sample_rate_hz, data[i]);
      }
    }

    ArduinoFFT<T> fft;

    switch (options.filter) {
    case FFTAnalysisFilter::None:
      break;
    case FFTAnalysisFilter::ZScore: {
      fft_noise_reduction::ZScoreFilter<T, uint16_t> filter;
      filter.calc(data, options.window_size, Z_SCORE_THRESHOLD);
      break;
    }
    case FFTAnalysisFilter::Hampel: {
      fft_noise_reduction::HampelFilter<T, uint16_t, HAMPEL_WINDOW_SIZE> filter;
      filter.calc(prev_data, data, next_data, options.window_size,
                  HAMPEL_THRESHOLD);
      break;
    }
    }

    if (false && options.window_size < 256) {
      for (size_t i = 0; i < options.window_size; i++) {
        ESP_LOGI(TAG, "Filtered: %f, %f", (T)i / options.sample_rate_hz,
                 data[i]);
      }
    }

    result.aggregation_results.clear();
    switch (options.aggregation) {
    case FFTAnalysisAggregation::TumblingWindow: {
      tumbling_window_avg.calc(result.aggregation_results, data, options.window_size);
      break;
    }
    case FFTAnalysisAggregation::SlidingWindow: {
      sliding_window_avg.calc(result.aggregation_results, data, options.window_size);
      break;
    }
    }

    ESP_LOGI(TAG, "Average output size: %zu", result.aggregation_results.size());
    if (false && options.window_size < 256) {
      for (size_t i = 0; i < result.aggregation_results.size(); i++) {
        ESP_LOGI(TAG, "Averaged: %f, %f", (T)i / options.sample_rate_hz,
                 result.aggregation_results[i]);
      }
    }

    std::fill_n(buffer_complex, options.window_size, 0);
    fft.dcRemoval(data, options.window_size);
    // Apply windowing to minimize spectral leakage:
    // https://www.ti.com/content/dam/videos/external-videos/ja-jp/2/3816841626001/5834902778001.mp4/subassets/adcs-fast-fourier-transforms-and-windowing-presentation-quiz.pdf
    fft.windowing(data, options.window_size, FFTWindow::Hann,
                  FFTDirection::Forward, nullptr, false);
    fft.compute(data, buffer_complex, options.window_size,
                FFTDirection::Forward);
    fft.complexToMagnitude(data, buffer_complex, options.window_size);

    const T last_peak = find_last_peak_freq(data, options.window_size / 2 + 1) *
                        options.sample_rate_hz / options.window_size;
    if (last_peak < 0) {
      ESP_LOGI(TAG, "Couldn't find any peaks");
      result.last_peak_hz = NAN;
      result.optimal_sample_rate_hz = NAN;
      return result;
    }
    ESP_LOGI(TAG, "Last peak: %f Hz", last_peak);
    result.last_peak_hz = last_peak;

    // Due to the Nyquist–Shannon sampling theorem, a sample rate of strictly
    // more than (last_peak * 2) Hz covers last_peak Hz of bandwidth
    result.optimal_sample_rate_hz = last_peak * 2.0 + 0.1;
    ESP_LOGI(TAG, "Optimal sample rate: %f Hz", result.optimal_sample_rate_hz);
    if (false && options.window_size < 256) {
      for (int i = 0; i < options.window_size / 2 + 1; i++) {
        ESP_LOGI(TAG, "FFT: %f, %f, %f",
                 (T)i * options.sample_rate_hz / options.window_size, data[i],
                 buffer_complex[i]);
      }
    }

    return result;
  }
};

} // namespace fft_analysis
