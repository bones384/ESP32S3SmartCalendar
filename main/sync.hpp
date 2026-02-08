//
// Created by mkowa on 08.02.2026.
//

#pragma once
#include <structs.hpp>
#include <map>

void networkSync();
extern std::multimap<Date, Event> event_map;
extern std::map<Date, Forecast> forecast_map;