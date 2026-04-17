file(GLOB_RECURSE SRC_FILES
        "*.c"
        "*.cpp"
)
idf_component_register(SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES 
        arduino
        ArduinoJson
        GxEPD2
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        espressif__network_provisioning
        Adafruit_SHT4X
        Adafruit_FRAM_SPI
        fonts
        lvgl)
