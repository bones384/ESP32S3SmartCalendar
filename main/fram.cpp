//
// Created by mkowa on 08.02.2026.
//

#include "fram.hpp"

void fram_write_string(uint32_t &addr,const String& str)
{

    for (const char c : str)
    {
        fram.write8(addr,c);
        addr++;
    }
    fram.write8(addr++,'\n');
}
String fram_read_string(uint32_t &addr)
{
    String back;
    while (true)
    {

        const char c = static_cast<char>(fram.read8(addr++));
        if (c=='\n') break;
        back += c;
    }
    return back;
}
void fram_write_byte(uint32_t &addr, const uint8_t &byte)
{
    fram.write8(addr,byte);
    addr++;
}
uint8_t fram_read_byte(uint32_t &addr)
{
    return fram.read8(addr++);
}
void fram_write_float(uint32_t& addr, float f)
{
    fram.write(addr, reinterpret_cast<uint8_t*>(&f), sizeof(f));
    addr += sizeof(f);
}

float fram_read_float(uint32_t& addr)
{
    float f;
    fram.read(addr, reinterpret_cast<uint8_t*>(&f), sizeof(f));
    addr += sizeof(f);
    return f;
}
template <typename T>
void fram_write(uint32_t& addr, T t)
{
    fram.write(addr, reinterpret_cast<uint8_t*>(&t), sizeof(t));
    addr += sizeof(t);
}
template <typename T>
T fram_read(uint32_t& addr)
{
    T t;
    fram.read(addr, reinterpret_cast<uint8_t*>(&t), sizeof(t));
    addr += sizeof(t);
    return t;
}
void fram_write_forecast(uint32_t &addr, const Forecast &forecast)
{
    fram_write_float(addr,forecast.temperature_daily_max);
    fram_write_float(addr,forecast.temperature_daily_min);
    fram_write_byte(addr,forecast.weather_code_daily);
    for (auto &temp : forecast.temperature_hourly)
    {
        fram_write_float(addr,temp);
    }
    for (auto &weather : forecast.weather_code_hourly)
    {
        fram_write_byte(addr,weather);
    }
}
Forecast fram_read_forecast(uint32_t &addr)
{
    Forecast forecast = {
            .temperature_daily_max = fram_read_float(addr),
            .temperature_daily_min = fram_read_float(addr),
            .weather_code_daily = static_cast<Weather>(fram_read_byte(addr)),

    };
    for ( auto &temp : forecast.temperature_hourly)
    {
        temp = fram_read_float(addr);
    }

    for ( auto &weather : forecast.weather_code_hourly)
    {
        weather = static_cast<Weather>(fram_read_byte(addr));
    }
    return forecast;
}

void fram_write_event(uint32_t &addr, Event &event)
{
    fram_write_string(addr,event.summary);
    fram_write_string(addr,event.description);

    time_t start = mktime( &event.startTime);
    time_t end = mktime( &event.endTime);

    fram_write<time_t>(addr, start);
    fram_write<time_t>(addr, end);

}

Event fram_read_event(uint32_t &addr)
{
    Event event =
            {
                    .summary = fram_read_string(addr),
                    .description = fram_read_string(addr)
            };
    const auto start_t = fram_read<time_t>(addr);;
    const auto end_t = fram_read<time_t>(addr);;

    event.startTime = *localtime(&start_t);
    event.endTime = *localtime(&end_t);

    return event;
}
//! Call fram.begin before any fram functions!
void saveToCache()
{
    fram.exitSleep();
    fram.writeEnable(true);
    uint32_t addr = 0;
    std::set<Date> dates;
    for (const auto& [key, val] : event_map)
    {
        dates.insert(key);
    }
    for (int i = 0; i < 14; i++)
    {
        tm copy = getTime();
        tm* t = &copy;
        copy.tm_mday+=i;
        auto time_t_now = mktime(&copy);
        t = localtime(&time_t_now);
        Date d = {t->tm_year,t->tm_mon,t->tm_mday};
        dates.insert(d);
    }
    // Save date count
    fram_write_byte(addr,dates.size());
    for(auto d : dates)
    {
        // Save date
        fram_write(addr,d);

        if (forecast_map.contains(d))
        {
            // Save if there is a forecast or not

            fram_write(addr,static_cast<byte>(1));
            fram_write(addr,forecast_map.at(d));

        }
        else
        {
            fram_write(addr,static_cast<byte>(0));
        }
        if (event_map.contains(d))
        {
            fram_write(addr,static_cast<byte>(event_map.count(d)));
            auto [fst, snd] = event_map.equal_range(d);
            for (auto it = fst; it != snd; ++it)
            {
                fram_write_event(addr,it->second);
            }

        }
        else
        {
            fram_write(addr,static_cast<byte>(0));
        }

    }
    fram.writeEnable(false);
    fram.enterSleep();
}
void loadFromCache()
{
    fram.exitSleep();

    uint32_t addr = 0;

    // Read date count
    delay(100);
    byte count = fram_read<byte>(addr);
    DBG_PRINTLN("COUNT OF DATES IN CACHE:");
    DBG_PRINTLN(count);
    delay(100);

    for (byte i = 0; i < count; i++)
    {
        Date d = fram_read<Date>(addr);
        Serial.print(d.day),Serial.print(" "),Serial.print(d.month),Serial.print(" "),Serial.println(d.year);

        byte has_forecast = fram_read<byte>(addr);
        Serial.println(has_forecast);
        if(has_forecast!=0)
        {
            auto f = fram_read<Forecast>(addr);
            Serial.println(f.temperature_daily_max);
            forecast_map.insert({d,f});
        }
        // Read event count
        byte event_count = fram_read<byte>(addr);
        Serial.println(event_count);
        for (int j = 0; j < event_count; j++)
        {
            Event e = fram_read_event(addr);
            Serial.println(e.summary);

            event_map.insert({d,e});
        }
    }
    fram.enterSleep();
}