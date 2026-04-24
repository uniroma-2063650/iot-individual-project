set(
    srcs
    "src/main.cc"
    "src/mqtt.cc"
    "src/lora.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES mqtt lmic arduinoFFT esp_adc esp_driver_i2c esp_wifi esp_event nvs_flash
        esp_driver_gpio esp_pm
)
