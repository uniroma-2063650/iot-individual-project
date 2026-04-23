#pragma once

#include "data.hh"
#include <FreeRTOS/FreeRTOS.h>
#include <esp_event.h>
#include <lmic/lmic.h>
#include <stdint.h>
#include <vector>

struct LoRa {
  LoRa(uint8_t core);

  void send_aggregate_data(const AggregationData &data);

private:
  TaskHandle_t task;
  QueueHandle_t queue;
  std::vector<uint8_t> packet_buffer;

  static void run_static(void *args);
  static void handle_event_static(void *args, ev_t event);
};
