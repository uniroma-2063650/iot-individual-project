#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>

using Sample = float;

struct AggregationDataPacketHeader {
  float end_seconds;
  float seconds_per_value;
  uint32_t values;
};

struct AggregationData {
  float end_seconds;
  float seconds_per_value;
  uint32_t size;
  Sample *values;

  size_t packed_size() const {
    return sizeof(AggregationDataPacketHeader) + size * sizeof(float);
  }

  void pack(uint8_t buffer[]) const {
    *(AggregationDataPacketHeader *)buffer = AggregationDataPacketHeader{
        .end_seconds = end_seconds,
        .seconds_per_value = seconds_per_value,
        .values = std::byteswap(size),
    };
    for (size_t i = 0; i < size; i++) {
      ((float *)((AggregationDataPacketHeader *)buffer + 1))[i] = values[i];
    }
  }
};
