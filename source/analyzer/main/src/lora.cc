#include "lora.hh"
#include "config.hh"
#include "data.hh"
#include "lmic/lmic.h"
#include "lmic/lmic_bandplan.h"
#include "lora_utils.hh"
#include <driver/gpio.h>
#include <esp_log.h>
#include <lmic/lmic_eu_like.h>
#include <lmic_hal/boards.hh>
#include <lmic_hal/hal.hh>
#include <protocol_examples_common.h>
#include <utility>

constexpr const char *TAG = "LoRa";

static uint8_t APP_EUI[]{
#include "../../lora_keys/app_eui.key"
};
static uint8_t DEV_EUI[]{
#include "../../lora_keys/dev_eui.key"
};
static uint8_t APP_KEY[]{
#include "../../lora_keys/app_key.key"
};

void os_getDevEui(u1_t *buf) { os_copyMem(buf, DEV_EUI, sizeof(DEV_EUI)); }
void os_getArtEui(u1_t *buf) { os_copyMem(buf, APP_EUI, sizeof(APP_EUI)); }
void os_getDevKey(u1_t *buf) { os_copyMem(buf, APP_KEY, sizeof(APP_KEY)); }

LoRa::LoRa(uint8_t core) {
  ESP_LOGI(TAG, "Starting task...");
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  queue = xQueueCreate(4, sizeof(AggregationData));
  assert(queue);
  assert(xTaskCreatePinnedToCore((void (*)(void *))run_static, "lora", 4096,
                                 this, configMAX_PRIORITIES - 1, &task,
                                 core) == pdTRUE);
}

static void reverse_bits(uint8_t buf[], size_t size) {
  for (size_t i = 0; i < size; i++) {
    uint8_t value = 0;
    for (size_t j = 0; j < 8; j++) {
      value |= ((buf[i] >> j) & 1) << (7 - j);
    }
    buf[i] = value;
  }
}

static void reverse_bytes(uint8_t buf[], size_t size) {
  for (size_t i = 0; i < size / 2; i++) {
    std::swap(buf[i], buf[size - 1 - i]);
  }
}

void LoRa::run_static(void *args) {
  LoRa *self = (LoRa *)args;

  // reverse_bytes(APP_EUI, sizeof(APP_EUI) / sizeof(uint8_t));
  // reverse_bytes(DEV_EUI, sizeof(DEV_EUI) / sizeof(uint8_t));
  // reverse_bytes(APP_KEY, sizeof(APP_KEY) / sizeof(uint8_t));

  ESP_LOGI(TAG, "Configuring...");

  const ESP_IDF_LMIC::HalConfig config =
      LORA_DEVICE_IS_HELTEC_V3
          ? ESP_IDF_LMIC::HalConfig{.board =
                                        *ESP_IDF_LMIC::
                                            get_board_config_heltec_lora32_v3()}
          : ESP_IDF_LMIC::HalConfig{
                .board = *ESP_IDF_LMIC::get_board_config_heltec_lora32_v2()};
  if (!os_init_ex(&config)) {
    ESP_LOGE(TAG, "Failed to initialize LMIC");
    vTaskDelete(nullptr);
  }

  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK),
                    BAND_MILLI); // g2-band

  ESP_LOGI(TAG, "Registering event callback...");

  LMIC_registerEventCb(LoRa::handle_event_static, self);
  LMICcore_setDrJoin(DRCHG_SET, DR_SF12);

  ESP_LOGI(TAG, "Starting joining...");

  LMIC_startJoining();

  ESP_LOGI(TAG, "Started");

  for (;;) {
    {
      AggregationData data;
      if (!LMIC_queryTxReady()) {
        while (xQueueReceive(self->queue, &data, 0) == pdTRUE) {
          ESP_LOGI(
              TAG,
              "Sending %zu aggregate values (%zu B, %f s per value) at %f s",
              data.size, data.size * sizeof(float), data.seconds_per_value,
              data.end_seconds);

          const size_t packet_size = data.packed_size();
          self->packet_buffer.resize(packet_size);
          data.pack(self->packet_buffer.data());

          if (!LMIC_queryTxReady()) {
            ESP_LOGI(TAG, "Waiting for TX to be ready...");
            ESP_LOGI(TAG, "TX ready");
          }
          LMIC_TX_ERROR_CHECK(LMIC_setTxData2_strict(
              LORA_PORT, (u1_t *)self->packet_buffer.data(), packet_size,
              false));
        }
      }
    }
    os_runloop_once();
    if (!LMIC_queryTxReady()) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

  vTaskDelete(nullptr);
}

void LoRa::handle_event_static(void *args, ev_t event) {
  const char *event_name = event_type_to_name(event);
  if (event_name) {
    ESP_LOGI(TAG, "Received event of type %s", event_name);
  } else {
    ESP_LOGI(TAG, "Received event of unknown type %02X", event);
  }
  switch (event) {
  case EV_JOINING:
    break;
  case EV_JOINED:
    break;
  case EV_JOIN_FAILED:
    break;
  case EV_REJOIN_FAILED:
    break;
  case EV_TXCOMPLETE:
    break;
  case EV_RXCOMPLETE:
    break;
  case EV_SCAN_TIMEOUT:
    break;
  case EV_BEACON_FOUND:
    break;
  case EV_BEACON_TRACKED:
    break;
  case EV_BEACON_MISSED:
    break;
  case EV_LOST_TSYNC:
    break;
  case EV_RESET:
    break;
  case EV_LINK_DEAD:
    break;
  case EV_LINK_ALIVE:
    break;
  case EV_SCAN_FOUND:
    break;
  case EV_TXSTART:
    break;
  case EV_TXCANCELED:
    break;
  case EV_RXSTART:
    break;
  case EV_JOIN_TXCOMPLETE:
    break;
  default: {
    ESP_LOGW(TAG, "Unknown event type: %02X", event);
    break;
  }
  }
}

void LoRa::send_aggregate_data(const AggregationData &data) {
  xQueueSend(queue, &data, pdMS_TO_TICKS(100));
}
