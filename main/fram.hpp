//
// Created by mkowa on 08.02.2026.
//

#pragma once

#include <map>
#include <Adafruit_FRAM_SPI.h>
#include "structs.hpp"
#include "defines.h"
inline Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);
extern std::multimap<Date, Event> event_map;
extern std::map<Date, Forecast> forecast_map;

void loadFromCache();
void saveToCache();

