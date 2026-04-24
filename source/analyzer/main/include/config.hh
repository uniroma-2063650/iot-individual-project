#pragma once

#include "fft/analysis.hh"
#include <cstdint>
#include <sdkconfig.h>

#ifdef CONFIG_MAIN_DEVICE_KIND_HELTEC_V3
constexpr bool LORA_DEVICE_IS_HELTEC_V3 = true;
#else
constexpr bool LORA_DEVICE_IS_HELTEC_V3 = false;
#endif

#ifdef CONFIG_MAIN_TX_KIND_MQTT
constexpr bool USE_MQTT = true;
#else
constexpr bool USE_MQTT = false;
#endif

constexpr uint8_t LORA_PORT = CONFIG_MAIN_TX_LORA_PORT;

#ifdef CONFIG_MAIN_SWITCH_TO_OPTIMAL_FREQ
constexpr bool SWITCH_TO_OPTIMAL = true;
#else
constexpr bool SWITCH_TO_OPTIMAL = false;
#endif

#ifdef CONFIG_MAIN_WAVE_SOURCE_DUMMY
constexpr bool USE_DUMMY_WAVE = true;
#else
constexpr bool USE_DUMMY_WAVE = false;
#endif

constexpr uint8_t DUMMY_WAVE_INDEX = CONFIG_MAIN_DUMMY_WAVE_INDEX;

#ifdef CONFIG_MAIN_FFT_NOISE_FILTER_HAMPEL
constexpr fft_analysis::FFTAnalysisFilter FFT_FILTER =
    fft_analysis::FFTAnalysisFilter::Hampel;
#elif defined(CONFIG_MAIN_FFT_NOISE_FILTER_ZSCORE)
constexpr fft_analysis::FFTAnalysisFilter FFT_FILTER =
    fft_analysis::FFTAnalysisFilter::ZScore;
#elif defined(CONFIG_MAIN_FFT_NOISE_FILTER_ZSCORE_WINDOWING)
constexpr fft_analysis::FFTAnalysisFilter FFT_FILTER =
    fft_analysis::FFTAnalysisFilter::ZScoreWindowing;
#else
constexpr fft_analysis::FFTAnalysisFilter FFT_FILTER =
    fft_analysis::FFTAnalysisFilter::None;
#endif

#ifdef CONFIG_MAIN_FFT_AGGREGATION_TUMBLING_WINDOW
constexpr fft_analysis::FFTAnalysisAggregation FFT_AGGREGATION =
    fft_analysis::FFTAnalysisAggregation::TumblingWindow;
#else
constexpr fft_analysis::FFTAnalysisAggregation FFT_AGGREGATION =
    fft_analysis::FFTAnalysisAggregation::SlidingWindow;
#endif

constexpr size_t FFT_AGGREGATION_WINDOW_SIZE =
    CONFIG_MAIN_FFT_AGGREGATION_WINDOW_SIZE;
