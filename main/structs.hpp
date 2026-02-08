//
// Created by jaygee on 08.02.26.
//

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ctime>
#include <array>
struct Event
{
    String summary = "Unknown summary";
    String description = "Unknown description";
    tm startTime ;
    tm endTime;

};
struct Date
{
    int year;
    int month;
    int day;
    friend bool operator< (const Date& rhs, const Date& lhs)
    {
        if (lhs.year != rhs.year)
        {
            return lhs.year > rhs.year;
        }
        if (lhs.month!=rhs.month )return lhs.month > rhs.month;
        return lhs.day > rhs.day;

    }
};

enum Weather : byte
{
    UNKNOWN = 0,
    CLEAR_SKIES,
    CLOUDY,
    OVERCAST,
    FOGGY,
    DRIZZLE,
    RAINY,
    SNOWY,
    STORMY
};

inline Weather toWeather(byte weather_code)
    {
        switch(weather_code)
        {
            case 0:
                return CLEAR_SKIES;
            case 1:
            case 2:
                return CLOUDY;
            case 3:
                return OVERCAST;
            case 45:
            case 48:
                return FOGGY;
            case 51:
            case 53:
            case 55:
            case 56:
            case 57:
                return DRIZZLE;
            case 61:
            case 63:
            case 65:
            case 66:
            case 67:
            case 80:
            case 81:
            case 82:
                return RAINY;
            case 71:
            case 73:
            case 75:
            case 77:
            case 85:
            case 86:
                return SNOWY;
            case 95:
            case 96:
            case 99:
                return STORMY;
            default:
                return UNKNOWN;

        }
    }

//per day
constexpr short FORECAST_DAYS = 7;
struct Forecast
{
    //(i) Hourly & Daily values are stored
    //(i) Daily max and min temp
    float temperature_daily_max; //4 bytes
    float temperature_daily_min; //4 bytes
   // //(i) Daily humidity
   // //byte humidity_daily; //byte
    //(i) Daily weather code
    Weather weather_code_daily; //byte

    //(i)Hourly temp
    std::array<float,24> temperature_hourly; // 4 bytes * 24 = 96 bytes
    //(i)Hourly weather code
    std::array<Weather,24> weather_code_hourly; // 96 bytes
    // ~202 bytes per day
    // ~1414 bytes per 7 day forecast
    // ~~1.4kB?
};