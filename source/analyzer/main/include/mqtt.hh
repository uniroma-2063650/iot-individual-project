#pragma once

#include <stdint.h>
#include <esp_event.h>

struct Mqtt {
    Mqtt();

private:
    static void handle_event_static(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void handle_event(esp_event_base_t base, int32_t event_id, void *event_data);
};
