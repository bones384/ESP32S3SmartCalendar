//
// Created by jaygee on 08.02.26.
//

#pragma once
#include <Arduino.h>
#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#define DBG_PRINTF(...)
#endif
