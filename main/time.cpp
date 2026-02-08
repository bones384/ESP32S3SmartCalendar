//
// Created by mkowa on 08.02.2026.
//

#include "time.hpp"

const char* ntpServer = "pool.ntp.org";
constexpr long  gmtOffset_sec = 3600;
constexpr int   daylightOffset_sec = 3600;

bool syncTime()
{
    // Init and get the time
    if (!connect())
    {
        DBG_PRINTLN("WiFi connection failed - time sync aborted.");
        return false;
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    tm timeinfo{};
    while(!getLocalTime(&timeinfo)){
        DBG_PRINTLN("Failed to obtain time");
        return false;
    }
    DBG_PRINTLN(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    return true;
}

bool timeset = false;
tm getTime()
{
    if (!timeset) timeset = syncTime();

    time_t now;
    time(&now);

    tm local_tm{};
    localtime_r(&now, &local_tm);
    return local_tm;
}