//
// Created by mkowa on 08.02.2026.
//

#include "connect.hpp"
#include <Arduino.h>
#include "dbgprint.h"
bool connect()
{
    if(WiFi.status()!=WL_CONNECTED)
    {
        char retries = wifi_retries;
        DBG_PRINT("Connecting to ");
        DBG_PRINTLN(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            DBG_PRINT(".");
            retries -= 1;
            if (retries == 0) {
                DBG_PRINTLN("");
                DBG_PRINT("!!!WiFi connection failed after "); DBG_PRINT(wifi_retries); DBG_PRINTLN(" tries.");
                WiFi.mode(WIFI_OFF);
                return false;
            }
        }
        DBG_PRINTLN("");
        DBG_PRINTLN("WiFi connected.");
    }
    return true;
}
