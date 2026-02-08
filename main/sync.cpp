//
// Created by mkowa on 08.02.2026.
//

#include "sync.hpp"
#include <cstdint>
#include <Arduino.h>
#include "dbgprint.h"
#include <ArduinoJson.h>
#include "time.hpp"
#include "tokens.hpp"
#include "sync.hpp"
#include <HTTPClient.h>
#include "connect.hpp"
constexpr char calendarurl[] = "https://www.googleapis.com/calendar/v3/";
constexpr char listurl[] = "calendars/primary/events?";

void fixTZ(char *buf) {
    if (size_t len = strlen(buf); len > 2) {
        memmove(buf + len - 1, buf + len - 2, 3);
        buf[len - 2] = ':';
    }
}

JsonDocument getCalendarEvents()
{
    DBG_PRINTLN("Event retrieval beginning.");

    DBG_PRINTLN("Current day:");
    auto time = getTime();

    struct tm start_tm = time;
    start_tm.tm_hour = 0;
    start_tm.tm_min  = 0;
    start_tm.tm_sec  = 0;

    struct tm end_tm = start_tm;
    // end_tm.tm_mday += 1;  // tomorrow 00:00
    end_tm.tm_year +=1;

    time_t start_t = mktime(&start_tm);
    time_t end_t   = mktime(&end_tm);

    char start_buf[32];
    char end_buf[32];

    strftime(start_buf, sizeof(start_buf),
             "%Y-%m-%dT%H:%M:%S%z", localtime(&start_t));
    fixTZ(start_buf);
    DBG_PRINTLN(start_buf);
    strftime(end_buf, sizeof(end_buf),
             "%Y-%m-%dT%H:%M:%S%z", localtime(&end_t));
    DBG_PRINTLN("Tomorrow:");
    fixTZ(end_buf);
    DBG_PRINTLN(end_buf);
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - event retrieval aborted.");
        return JsonDocument();
    }
    // Send post for refresh
    String payload;
    payload.reserve(256);
    payload = String(calendarurl) + listurl + "singleEvents=True&orderBy=startTime";
    payload += String("&timeMin=") + String(start_buf);
    payload += String("&timeMax=") + String(end_buf);
    DBG_PRINTLN(payload);
    payload.replace("+","%2B");
    HTTPClient http;
    http.setReuse(false);
    http.setAuthorizationType("Bearer");
    http.setAuthorization(getAccessToken().c_str());
    http.begin(payload);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.GET();
    if (httpResponseCode==200) {
        DBG_PRINT("HTTP Response code: ");
        DBG_PRINTLN(httpResponseCode);
        String response = http.getString();
        http.end();
        DBG_PRINTLN(response);

        JsonDocument doc;

        if (DeserializationError error = deserializeJson(doc, response)) {
            DBG_PRINT("deserializeJson() failed: ");
            DBG_PRINTLN(error.c_str());
            http.end();
            return JsonDocument();
        } else {
            // Extract values

            http.end();
            return doc["items"];
        }

    }
    else {
        if (httpResponseCode==401) {if (refreshToken()) return getCalendarEvents();}
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
        return JsonDocument();
    }


}
std::vector<Forecast> forecasts;
constexpr char forecasturl[] = "http://api.open-meteo.com/v1/forecast?latitude=50.33&longitude=18.9&daily=temperature_2m_max,weather_code,temperature_2m_min,precipitation_probability_max&hourly=temperature_2m,weather_code&timezone=auto";
JsonDocument getForecast()
{
    DBG_PRINTLN("Forecast retrieval beginning.");
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - forecast retrieval aborted.");
        return JsonDocument();
    }
    // Send post for refresh
    String payload;
    payload.reserve(256);
    payload = String(forecasturl);
    HTTPClient http;
    http.setReuse(false);
    http.begin(payload);
    int httpResponseCode = http.GET();
    if (httpResponseCode==200) {
        DBG_PRINT("HTTP Response code: ");
        DBG_PRINTLN(httpResponseCode);
        String response = http.getString();
        http.end();
        DBG_PRINTLN(response);

        JsonDocument doc;

        if (DeserializationError error = deserializeJson(doc, response)) {
            DBG_PRINT("deserializeJson() failed: ");
            DBG_PRINTLN(error.c_str());
            http.end();
            return JsonDocument();
        } else {
            // Extract values
            http.end();
            return doc;
        }

    }
    else {
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
        return JsonDocument();
    }
}
void loadForecast()
{
    //1 Get all weather data (1-week for now)
    auto doc = getForecast();
    //2 seperate into arrays

    auto weather_code_daily = doc["daily"]["weather_code"].as<JsonArray>();
    auto temperature_daily_min = doc["daily"]["temperature_2m_min"].as<JsonArray>();
    auto temperature_daily_max = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    auto temperature_hourly_temp = doc["hourly"]["temperature_2m"].as<JsonArray>();
    auto weather_code_hourly_temp =  doc["hourly"]["weather_code"].as<JsonArray>();
    std::array<std::array<float,24>,7> temperature_hourly{};
    std::array<std::array<Weather,24>,7> weather_code_hourly{};
    for(int i = 0; i < FORECAST_DAYS; i ++)
    {

        for(int j = 0; j < 24 ; j++)
        {
            temperature_hourly[i][j] = temperature_hourly_temp[i*24+j];
            weather_code_hourly[i][j] = toWeather(weather_code_hourly_temp[i*24+j]);
        }

    }
    // For all days...
    for (int i = 0 ; i < FORECAST_DAYS; i ++) {
        Forecast f = {
                .temperature_daily_max = temperature_daily_max[i],
                .temperature_daily_min = temperature_daily_min[i],
                .weather_code_daily = toWeather(weather_code_daily[i]),

                .temperature_hourly = temperature_hourly[i],
                .weather_code_hourly = weather_code_hourly[i]
        };
        forecasts.push_back(f);
    }
}

tm rfc3339ToTm(const char *rfc3339)
{
    struct tm tm = {};
    char datetime[20];   // YYYY-MM-DDTHH:MM:SS
    char tzsign;
    int tzh, tzm;

    // Copy datetime part
    memcpy(datetime, rfc3339, 19);
    datetime[19] = '\0';

    // Parse base time
    if (!strptime(datetime, "%Y-%m-%dT%H:%M:%S", &tm)) {
        return tm;
    }

    // Parse timezone
    if (sscanf(rfc3339 + 19, "%c%2d:%2d", &tzsign, &tzh, &tzm) != 3) {
        return (tm);
    }

    int offset = (tzh * 3600) + (tzm * 60);
    if (tzsign == '+') offset = -offset;

    // Convert as UTC
    time_t t = mktime(&tm);
    t -= offset;
    localtime_r(&t, &tm);
    return tm;
}

std::vector<Event> events;
void loadEvents(JsonDocument doc)
{
    DBG_PRINTLN("Loading events...");
    for (JsonVariant item : doc.as<JsonArray>())
    {
        Event event;
        event.summary = item["summary"].as<String>() ;
        event.description = item["summary"].as<String>() ;
        if (item["start"]["date"].is<JsonVariant>())
        {
            //Whole day event.
            //! May last multiple days?
            String s = item["start"]["date"].as<String>() + "T00:00:00+00:00";
            auto ts = rfc3339ToTm( item["start"]["date"]);
            event.startTime = ts;
            ts.tm_hour=23;
            ts.tm_min=59;
            event.endTime = ts;
        }
        else
        {

            //Single day event
            event.startTime = rfc3339ToTm( item["start"]["dateTime"]);
            event.endTime = rfc3339ToTm( item["end"]["dateTime"]);
        }
        events.push_back(event);
    }
}



void networkSync()
{
    getTime();
    loadEvents(getCalendarEvents());
    // Send HTTP GET request
    loadForecast();

    //We now have fresh forecast data and fresh event data - merge it on a per date basis

    for(auto &event : events)
    {
        event_map.insert(std::make_pair(Date(event.startTime.tm_year,event.startTime.tm_mon,event.startTime.tm_mday),event));
    }
    auto n = getTime();

    //! fix to handle new year case
    int idx = 0;
    for(auto forecast : forecasts)
    {
        tm copy = n;
        tm* t = &copy;
        copy.tm_mday+=idx;
        auto time_t_now = mktime(&copy);
        t = localtime(&time_t_now);
        idx++;

        forecast_map.insert(std::make_pair(Date(t->tm_year,t->tm_mon,t->tm_mday),forecast));


    }
}