#pragma once

#include <array>
#include <cmath>
#include <esp_log.h>

namespace waves {

constexpr const char *TAG = "Gen";

template <typename T>
void generate_dummy_data_clean_a(T buffer[], size_t window_size,
                                 float sample_rate, float bias, float amplitude,
                                 float offset) {
  const std::array<float, 2> a{2.0, 2.0};
  const std::array<float, 2> f{3.2, 5.6};
  ESP_LOGI(TAG,
           "Generating sample data: (%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t)",
           a[0], f[0], a[1], f[1]);
  for (size_t i = 0; i < window_size; i++) {
    const float t = (float)i / sample_rate + offset;
    float sample = 0.0;
    for (int j = 0; j < a.size(); j++) {
      sample += a[j] * sin(f[j] * (2.0 * M_PI) * t);
    }
    buffer[i] = sample * amplitude + bias;
  }
}

template <typename T>
void generate_dummy_data_clean_b(T buffer[], size_t window_size,
                                 float sample_rate, float bias, float amplitude,
                                 float offset) {
  const std::array<float, 4> a{4.0, 3.0, 2.0, 7.0};
  const std::array<float, 4> f{100.0, 1000.0, 2000.0, 4000.0};
  ESP_LOGI(TAG,
           "Generating sample data: (%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t) + "
           "(%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t)",
           a[0], f[0], a[1], f[1], a[2], f[2], a[3], f[3]);
  for (size_t i = 0; i < window_size; i++) {
    const float t = (float)i / sample_rate + offset;
    float sample = 0.0;
    for (int j = 0; j < a.size(); j++) {
      sample += a[j] * sin(f[j] * (2.0 * M_PI) * t);
    }
    buffer[i] = sample * amplitude + bias;
  }
}

template <typename T>
void generate_dummy_data_clean_c(T buffer[], size_t window_size,
                                 float sample_rate, float bias, float amplitude,
                                 float offset) {
  const std::array<float, 4> a{3.0, 2.0, 5.0, 3.0};
  const std::array<float, 4> f{0.5, 1.0, 4999.0, 8100.0};
  ESP_LOGI(TAG,
           "Generating sample data: (%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t) + "
           "(%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t)",
           a[0], f[0], a[1], f[1], a[2], f[2], a[3], f[3]);
  for (size_t i = 0; i < window_size; i++) {
    const float t = (float)i / sample_rate + offset;
    float sample = 0.0;
    for (int j = 0; j < a.size(); j++) {
      sample += a[j] * sin(f[j] * (2.0 * M_PI) * t);
    }
    buffer[i] = sample * amplitude + bias;
  }
}

float rand_uniform(float a, float b) {
  return a + ((float)rand() / (float)RAND_MAX) * (b - a);
}

float rand_gaussian(float sigma) {
  // Based on https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
  const float u = rand_uniform(0, 1);
  const float v = rand_uniform(0, 1);
  // Standard normal distribution (sigma = 1 and mu = 0)
  const float x = (float)(sqrt(-2 * log(u)) * cos((2.0 * M_PI) * v));
  return x * sigma;
}

template <typename T>
void generate_dummy_data_noisy(T buffer[], size_t window_size,
                               float sample_rate, float bias, float amplitude,
                               float offset) {
  const std::array<float, 2> a{2.0, 4.0};
  const std::array<float, 2> f{3.2, 5.6};
  const float gaussian_noise_sigma = 0.2;
  const float random_spike_prob = 0.02;
  const float random_spike_min_amp = 5;
  const float random_spike_max_amp = 15;
  ESP_LOGI(TAG,
           "Generating sample data: (%f)sin(2pi*(%f)t) + (%f)sin(2pi*(%f)t) + "
           "n(t) + A(t)",
           a[0], f[0], a[1], f[1]);
  ESP_LOGI(TAG, "n(t) = N(0, %f)", gaussian_noise_sigma * gaussian_noise_sigma);
  ESP_LOGI(TAG, "A(t) = Bern(%f) * (2 * Bern(0.5) - 1) * U(%f, %f)",
           random_spike_prob, random_spike_min_amp, random_spike_max_amp);
  for (size_t i = 0; i < window_size; i++) {
    const float t = (float)i / sample_rate + offset;
    float sample = 0.0;
    for (int j = 0; j < a.size(); j++) {
      sample += a[j] * sin(f[j] * (2.0 * M_PI) * t);
    }
    sample += rand_gaussian(gaussian_noise_sigma);
    if (rand_uniform(0, 1) < random_spike_prob) {
      const float spike_amplitude =
          rand_uniform(random_spike_min_amp, random_spike_max_amp);
      if (rand_uniform(0, 1) < 0.5) {
        sample -= spike_amplitude;
      } else {
        sample += spike_amplitude;
      }
    }
    buffer[i] = sample * amplitude + bias;
  }
}

} // namespace waves
