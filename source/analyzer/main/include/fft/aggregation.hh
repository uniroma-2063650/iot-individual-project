#pragma once

#include <cstddef>
#include <cstring>
#include <esp_log.h>
#include <vector>

namespace fft_aggregation {

constexpr const char *TAG = "FFT";

template <typename T> struct SlidingWindowAverage {
private:
  size_t window_size;
  T current_sum;
  std::vector<T> history;
  size_t history_write_pos;
  size_t history_size;

public:
  SlidingWindowAverage(size_t window_size)
      : window_size(window_size), current_sum(0.), history_write_pos(0),
        history_size(0) {
    history.resize(window_size);
  }

  void clear() {
    current_sum = 0;
    history.resize(window_size);
    history_write_pos = 0;
    history_size = 0;
  }

  size_t get_window_size() { return window_size; }

  void set_window_size(size_t window_size) {
    if (window_size < this->window_size) {
      current_sum = 0;
      history.resize(window_size);
      history_write_pos = 0;
      history_size = 0;
      this->window_size = window_size;
      // TODO
    } else if (window_size > this->window_size) {
      history.resize(window_size);
      const size_t diff = window_size - this->window_size;
      const size_t history_read_pos =
          history_size > history_write_pos
              ? history_write_pos + window_size - history_size
              : history_write_pos - history_size;
      memmove(history.data() + history_read_pos,
              history.data() + history_read_pos + diff,
              (this->window_size - history_read_pos) * sizeof(T));
      this->window_size = window_size;
    }
  }

  void calc(std::vector<T> &out, const T buffer[], size_t size) {
    for (size_t i = 0; i < size; i++) {
      if (history_size >= window_size) {
        current_sum -= history[history_write_pos];
        history_size--;
      }
      current_sum += buffer[i];
      history[history_write_pos++] = buffer[i];
      if (history_write_pos >= window_size) {
        history_write_pos = 0;
      }
      history_size++;
      if (history_size >= window_size) {
        out.push_back(current_sum / window_size);
      }
    }
  }
};

template <typename T> struct TumblingWindowAverage {
private:
  size_t window_size;
  T current_sum;
  size_t history_size;

public:
  TumblingWindowAverage(size_t window_size)
      : window_size(window_size), current_sum(0), history_size(0) {}

  void clear() {
    current_sum = 0;
    history_size = 0;
  }

  size_t get_window_size() { return window_size; }

  void set_window_size(size_t window_size) {
    if (window_size < this->window_size && history_size > window_size) {
      current_sum = 0;
      history_size = 0;
    }
    this->window_size = window_size;
  }

  void calc(std::vector<T> &out, const T buffer[], size_t size) {
    if (history_size >= window_size) {
      out.push_back(current_sum / history_size);
      current_sum = 0;
      history_size = 0;
    }
    for (size_t i = 0; i < size; i++) {
      current_sum += buffer[i];
      history_size++;
      if (history_size >= window_size) {
        out.push_back(current_sum / history_size);
        current_sum = 0;
        history_size = 0;
      }
    }
  }
};

} // namespace fft_aggregation
