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
    void nextDay() {
        tm t = {};
        t.tm_year = year;
        t.tm_mon  = month;
        t.tm_mday = day;
        t.tm_hour = 12; // avoid DST issues
        time_t tt = mktime(&t);
        tt += 24*60*60; // add 1 day
        tm *new_t = localtime(&tt);
        year  = new_t->tm_year;
        month = new_t->tm_mon;
        day   = new_t->tm_mday;
    }

    // Decrement date by 1 day
    void prevDay() {
        tm t = {};
        t.tm_year = year;
        t.tm_mon  = month;
        t.tm_mday = day;
        t.tm_hour = 12;
        time_t tt = mktime(&t);
        tt -= 24*60*60; // subtract 1 day
        tm *new_t = localtime(&tt);
        year  = new_t->tm_year;
        month = new_t->tm_mon;
        day   = new_t->tm_mday;
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

    //(i) Daily weather code
    Weather weather_code_daily; //byte

    //(i)Hourly temp
    std::array<float,24> temperature_hourly; // 4 bytes * 24 = 96 bytes
    //(i)Hourly weather code
    std::array<Weather,24> weather_code_hourly; // 96 bytes

};