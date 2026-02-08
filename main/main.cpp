#include <Arduino.h>
#include <set>
#include <Adafruit_SHT4x.h>
#include <esp_phy_init.h>
#include <cstring>
#include <esp_wifi.h>
#include <WiFi.h>
#include <map>
#include <nvs.h>
#include <bitset>
#include "adc.hpp"
#include <cstdint>
#include <driver/rtc_io.h>
String password = "Mumia!24";
String ssid = "foxnet253";
#include <HTTPClient.h>
#include <GxEPD2_3C.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "adc.hpp"
#include "sync.hpp"
#include <ctime>
// Pins
#define CS_PIN    35
#define FRAM_CS   5
#define DC_PIN    17
#define RST_PIN   16
#define BUSY_PIN  4

#define BTN_LEFT     39
#define BTN_DOWN   36
#define BTN_UP   38
#define BTN_RIGHT  37
#define BTN_ENTER  6
#define INCLUDE_vTaskDelete 1

#include "dbgprint.h"
#include <Adafruit_FRAM_SPI.h>
#include "time.hpp"
#include "structs.hpp"
#include "tokens.hpp"
#include "connect.hpp"
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));

int is_manual;
void loop(void* arg);
void setup(void* arg);
extern "C" [[noreturn]] void app_main(void)
{
    initArduino();

    gpio_reset_pin(GPIO_NUM_40);
    gpio_reset_pin(GPIO_NUM_41);
    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT);
    is_manual = gpio_get_level(GPIO_NUM_40);


    gpio_reset_pin(GPIO_NUM_41);
    gpio_set_direction(GPIO_NUM_41, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT_OD);
    gpio_pullup_dis(GPIO_NUM_41);
    gpio_pullup_dis(GPIO_NUM_21);

    gpio_set_level(GPIO_NUM_21, 0); //HOLD
    gpio_set_level(GPIO_NUM_41, 0); //DONE



    TaskHandle_t setupHandle = nullptr;
    xTaskCreate(
            setup,
            "arduino_setup",
            20480, //8k might be enough?
            xTaskGetCurrentTaskHandle(),
            5,
            &setupHandle
    );
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (true) {
        xTaskCreate(
                loop,
                "arduino_loop",
                20480, //8k might be enough?
                xTaskGetCurrentTaskHandle(),
                5,
                &setupHandle
        );
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(1);
    }
}

auto sht4 = Adafruit_SHT4x();
auto fram = Adafruit_FRAM_SPI(FRAM_CS);

Preferences prefs;

//Manual or first poweron of battery pack
void manual()
{
    DBG_PRINTLN("Manual!");

    //Fetch fresh data
    networkSync();
    // User mode....


    //...

    //User interactive mode ending - do a final update to everything before shutdown
    networkSync();
    saveToCache();
    prefs.putUShort("bootsLeft",prefs.getUShort("bootsTillSync"));
    end();
}

void automatic()
{
    DBG_PRINTLN("Auto!");

    //Check if need to sync
    auto boots_left = prefs.getUShort("bootsLeft");
    if(boots_left==0)
    {
        //If yes: fetch new content overriding the cache and reset boot counter
        networkSync();
        saveToCache();

        prefs.getBytes("lastTime", &t, sizeof(t));


        prefs.putUShort("bootsLeft",prefs.getUShort("bootsTillSync"));

    }
    else
    {
        //If not: Load and display cached data, decrement boot counter
        // "approx. now time"
        time_t t;
        prefs.getBytes("lastTime", &t, sizeof(t));
        //Boots ~1 min apart

        int boot_count = prefs.getUShort("bootsTillSync")-boots_left;

        t+= boot_count * 60;

        tm local = localtime(&t);

        loadFromCache();

        auto now = getTime();
        auto now_t = std::mktime(&now)
        prefs.putBytes("lastTime", &now_t, sizeof(now_t));

    }
    // Print
    Serial.print("Automatic boot - boots till sync #:");
    Serial.println(boots_left);
    Serial.print("Current time: ");
    Serial.print(tm.hour);
    Serial.print(":");
    Serial.print(tm.min);
    Serial.print(":");
    Serial.print(tm.sec);
    //Turn everything off
    end();

}
void end()
{
    prefs.end();
    DBG_PRINTLN("Buhbye!");
    gpio_set_level(GPIO_NUM_41,1); //DONE
    gpio_set_level(GPIO_NUM_21,1); //stop hold
}



std::multimap<Date, Event> event_map;
std::map<Date, Forecast> forecast_map;

void setup(void *arg) {
    wifi_retries = 20;
    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT); //drain cap
    gpio_pulldown_en(GPIO_NUM_40);
    Serial.begin(115200);
    DBG_PRINTLN("Woken up!");
    DBG_PRINT("Pin 40 (WAKEREASON): ");
    DBG_PRINTLN(is_manual);
    prefs.begin("main",false);

    fram.begin();

    if(!prefs.isKey("bootsTillSync"))
    {
        prefs.putUShort("bootsTillSync", 5);
    }
    if(!prefs.isKey("bootsLeft"))
    {
        prefs.putUShort("bootsLeft", 0);
    }
    if(!prefs.isKey("lastTime"))
    {
        auto now = getTime();
        auto now_t = std::mktime(&now)
        prefs.putBytes("lastTime", &now_t, sizeof(now_t));

    }

    // BRANCH: WAKEREASON
    if(is_manual) manual();
    else automatic();


    //TEST SPACE!
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);


    //test();

    if(is_manual)
    {
        refreshToken();
    }

    networkSync();
    Serial.println(event_map.size());

    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    end();
    auto creator = static_cast<TaskHandle_t>(arg);
    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);

}
void test()
{



    pinMode(35,OUTPUT);
    pinMode(16,OUTPUT);
    pinMode(17,OUTPUT);

    DBG_PRINTLN("Adafruit SHT4x test");
    if (! sht4.begin()) {
        DBG_PRINTLN("Couldn't find SHT4x");

    }
    DBG_PRINTLN("Found SHT4x sensor");
    DBG_PRINT("Serial number 0x");
    DBG_PRINTLN(sht4.readSerial(), HEX);

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);

    sensors_event_t humidity, temp;

    //uint32_t timestamp = millis();
    sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    //timestamp = millis() - timestamp;
    DBG_PRINT("Temperature: "); DBG_PRINT(temp.temperature); DBG_PRINTLN(" degrees C");
    DBG_PRINT("Humidity: "); DBG_PRINT(humidity.relative_humidity); DBG_PRINTLN("% rH");

    DBG_PRINT("Read duration (ms): ");

    if (!fram.begin())
    {
        DBG_PRINT("FRAM NOT FOUND!!!!!!!!");

    }
    auto timeinfo = getTime();

    fram.exitSleep();
    fram.writeEnable(true);
    uint32_t addr = 12;
    uint8_t valtowrite = 12;
    fram.write8(addr,valtowrite);
    uint8_t val ;
    val = fram.read8(addr);
    fram.enterSleep();

    // measure bat voltage
    pinMode(2,OUTPUT_OPEN_DRAIN);

    digitalWrite(2,0);
    delay(200);
    float v_batt = battery_voltage();

    digitalWrite(2,1); // stop current through divider

    display.init(115200, true, 2, false);
    display.setRotation(0); // optional, 0-3 depending on orientation

    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 0);
    display.setTextColor(GxEPD_BLACK);
// sda 8 scl 9
    display.setTextSize(2);
    display.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    display.print(temp.temperature ) ;
    display.println("deg C");
    display.print(humidity.relative_humidity); display.println(" %rH");
    display.print("Val: "); display.println((int)val);
    display.print("Vbat: "); display.print(v_batt); display.println("V");
    display.display();
    display.hibernate();
}
void loop(void *arg) {

    auto creator = static_cast<TaskHandle_t>(arg);
    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);   // delete this task when done
}
