#include "mqtt.hh"
#include "mqtt_client.h"

#include <bit>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <protocol_examples_common.h>

constexpr const char *TAG = "MQTT";

extern const uint8_t client_cert_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");
extern const uint8_t broker_cert_start[] asm("_binary_broker_crt_start");
extern const uint8_t broker_cert_end[] asm("_binary_broker_crt_end");

static void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
  }
}

Mqtt::Mqtt() {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  const esp_mqtt_client_config_t mqtt_cfg = {
      .broker = {.address =
                     {
                         .uri = "mqtts://172.20.10.2:8883",
                     },
                 .verification = {.certificate =
                                      (const char *)broker_cert_start}},
      .credentials = {
          .authentication =
              {
                  .certificate = (const char *)client_cert_start,
                  .key = (const char *)client_key_start,
              },
      }};
#pragma GCC diagnostic pop
  ESP_LOGI(TAG, "Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY,
                                 Mqtt::handle_event_static, this);
  esp_mqtt_client_start(client);
}

void Mqtt::handle_event_static(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  return ((Mqtt *)handler_args)->handle_event(base, event_id, event_data);
}

void Mqtt::handle_event(esp_event_base_t base, int32_t event_id,
                        void *event_data) {
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32,
           base, event_id);
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ",
             event->msg_id, (uint8_t)*event->data);
    break;

  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls",
                           event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack",
                           event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",
                           event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(TAG, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }

    break;

  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

struct PacketHeader {
  float seconds;
  uint32_t values;
};

void Mqtt::send_aggregate_data(float seconds, const float values[],
                               uint32_t size) {
  const size_t packet_size = sizeof(PacketHeader) + size * sizeof(uint32_t);
  ESP_LOGI(TAG, "Sending %zu aggregate values (%zu B) at %f s", size,
           size * sizeof(float), seconds);

  packet_buffer.resize(packet_size);
  *(PacketHeader *)packet_buffer.data() = PacketHeader{
      .seconds = seconds,
      .values = std::byteswap(size),
  };
  for (size_t i = 0; i < size; i++) {
    ((float *)((PacketHeader *)packet_buffer.data() + 1))[i] = values[i];
  }

  esp_mqtt_client_publish(client, "topic/aggregate",
                          (const char *)packet_buffer.data(), packet_size, 2,
                          true);
}
