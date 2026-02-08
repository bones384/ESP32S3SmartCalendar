//
// Created by mkowa on 08.02.2026.
//

#pragma once
#include <cstdint>
#include <esp_wifi.h>
#include <WiFi.h>
inline uint8_t wifi_retries;
extern String password,ssid;
bool connect();
