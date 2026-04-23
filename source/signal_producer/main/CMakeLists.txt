set(
    srcs
    "src/main.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_dac
)
