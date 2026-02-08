//
// Created by mkowa on 08.02.2026.
//

#pragma once

#include <map>

extern Adafruit_FRAM_SPI fram;

extern std::multimap<Date, Event> event_map;
extern std::map<Date, Forecast> forecast_map;

void loadFromCache();
void saveToCache();

