#pragma once

#include <esp_event.h>
#include <mqtt_client.h>
#include <stdint.h>
#include <vector>

struct Mqtt {
  Mqtt();

  void send_aggregate_data(float seconds, const float values[], uint32_t size);

private:
  esp_mqtt_client_handle_t client;
  std::vector<uint8_t> packet_buffer;

  static void handle_event_static(void *handler_args, esp_event_base_t base,
                                  int32_t event_id, void *event_data);
  void handle_event(esp_event_base_t base, int32_t event_id, void *event_data);
};
