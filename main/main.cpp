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
#include <WiFiProv.h>
#include "defines.h"
#include <cstdint>
#include <driver/rtc_io.h>
#include <misc/lv_color.h>
#include <HTTPClient.h>
#include <GxEPD2_3C.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "adc.hpp"
#include "sync.hpp"
#include <ctime>

#include <lvgl.h>
#include "dbgprint.h"
#include <Adafruit_FRAM_SPI.h>
#include "time.hpp"
#include "fram.hpp"
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
void end();
Preferences prefs;

//Manual or first poweron of battery pack

void end()
{
    display.hibernate();
    prefs.end();
    DBG_PRINTLN("Buhbye!");
    gpio_set_level(GPIO_NUM_41,1); //DONE
    gpio_set_level(GPIO_NUM_21,1); //stop hold
    while(true) delay(500);
}

float vbat = 0.00f;
float humidity = 0.0f;
float temperature = 0.0f;
std::multimap<Date, Event> event_map;
std::map<Date, Forecast> forecast_map;

int selected = 1;           // 0 = header, 1 = body (body default now)
int header_section = 0;      // 0 = H2, 1 = H3
int partial_count = 0;
const int PARTIAL_LIMIT = 30;

bool btn_pressed(int gpio) {
    return digitalRead(gpio) == LOW;
}
tm dateToTm(const Date &d) {
    tm t = {};
    t.tm_year = d.year;
    t.tm_mon  = d.month;
    t.tm_mday = d.day;
    return t;
}
bool viewingEvents = false;
int eventIndex = 0;

void drawHeader(bool invert, int highlight_section = -1, tm now = getTime()) {
    int headerH = display.height() * 16 / 100; // now 16% of display height

    uint16_t fg = GxEPD_WHITE;
    uint16_t bg = GxEPD_BLACK;

    // Dynamic content
    char timeStr[6]; // HH:MM
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.tm_hour, now.tm_min);

    char envStr[20]; // temperature + humidity
    snprintf(envStr, sizeof(envStr), "%.1f""C %.0f%%RH", temperature, humidity);

    String h2Str;
    String h3Str;
    if(selected == 0) { // header selected -> show alternate text
        h2Str = "PROVISIONING";
        h3Str = "OATH RFSH";
    } else { // normal display
        if(WiFi.status() == WL_CONNECTED) h2Str = WiFi.SSID();
        else h2Str = "Offline";
        char vbatStr[12];
        snprintf(vbatStr, sizeof(vbatStr), "VBAT: %.2fV", vbat);
        h3Str = String(vbatStr);
    }

    // Widths
    int charW = 6 * 2;  // textSize=2
    int h1W = 5 * charW + charW;       // HH:MM
    int h3W = h3Str.length() * charW + charW;
    int totalW = display.width();
    int h2W = totalW - h1W - h3W;

    display.setPartialWindow(0, 0, totalW, headerH);
    display.firstPage();
    do {
        // H1: time (top line)
        display.fillRect(0, 0, h1W, headerH / 2, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(2, 2); // top of header
        display.setTextSize(2);
        display.print(timeStr);

        // H1 second line: temperature + humidity
        display.setCursor(2, headerH / 2 + 4);
        display.setTextSize(2);
        display.print(envStr);

        // H2: SSID / PROVISIONING
        bool invert_section = (highlight_section == 0 && invert); // only H2 is highlightable now
        display.fillRect(h1W, 0, h2W, headerH/2, invert_section ? bg : fg);
        display.setTextColor(invert_section ? fg : bg);
        display.setCursor(h1W + 2, 2);
        display.setTextSize(2);

        String h2Display = h2Str;
        int maxChars = h2W / charW;
        if(h2Display.length() > maxChars) h2Display = h2Display.substring(0, maxChars);
        display.print(h2Display);

        // H3: VBAT / OATH RFSH
        invert_section = (highlight_section == 1 && invert); // H3 highlightable
        display.fillRect(h1W + h2W, 0, h3W, headerH, invert_section ? bg : fg);
        display.setTextColor(invert_section ? fg : bg);
        display.setCursor(h1W + h2W + 2, 2);
        display.setTextSize(2);
        display.print(h3Str);

    } while(display.nextPage());
}
// Helper: format tm into a string
String formatDate(const tm &t) {
    // Example: "Monday, January 15 2026"
    char buf[32];
    strftime(buf, sizeof(buf), "%A, %B %d %Y", &t);
    return String(buf);
}
void drawBody(const Date &d, tm now = getTime()) {
    int headerH = display.height() * 16 / 100;
    int bodyH = display.height() - headerH;

    int leftW = display.width() * 10 / 100;
    int midW  = display.width() * 80 / 100;
    int rightW = display.width() - leftW - midW;

    display.setPartialWindow(0, headerH, display.width(), bodyH);
    display.firstPage();
    do {
        // Left section
        display.fillRect(0, headerH, leftW, bodyH, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(5, headerH + bodyH / 2 - 4);
        display.setTextSize(2);
        display.print("<-");

        // Right section
        display.fillRect(leftW + midW, headerH, rightW, bodyH, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(leftW + midW + 5, headerH + bodyH / 2 - 4);
        display.setTextSize(2);
        display.print("->");

        // Middle section
        display.fillRect(leftW, headerH, midW, bodyH, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(leftW + 5, headerH);
        display.setTextSize(2);

        // Convert Date to tm
        tm nowTm = dateToTm(d);
        String dateStr = formatDate(nowTm);
        display.println(dateStr);

        if (!viewingEvents) {
            // --- FORECAST DISPLAY ---
            auto it = forecast_map.find(d);
            if (it != forecast_map.end()) {
                const Forecast &fc = it->second;

                display.print("Weather: ");
                switch(fc.weather_code_daily) {
                    case CLEAR_SKIES: display.println("Clear"); break;
                    case CLOUDY: display.println("Cloudy"); break;
                    case OVERCAST: display.println("Overcast"); break;
                    case FOGGY: display.println("Foggy"); break;
                    case DRIZZLE: display.println("Drizzle"); break;
                    case RAINY: display.println("Rain"); break;
                    case SNOWY: display.println("Snow"); break;
                    case STORMY: display.println("Storm"); break;
                    default: display.println("Unknown"); break;
                }
                display.printf("Min: %.1f C Max: %.1f C\n", fc.temperature_daily_min, fc.temperature_daily_max);

                // Hourly table as before (rows, two columns)
                display.println();
                display.setTextSize(1);
                tm currTime = now;
                bool isToday = (d.year == currTime.tm_year &&
                                d.month == currTime.tm_mon &&
                                d.day == currTime.tm_mday);
                int startHour = isToday ? currTime.tm_hour : 0;
                int rowHeight = 10;
                int leftColX = leftW + 5;
                int rightColX = leftW + midW / 2 + 5;
                auto starty = display.getCursorY();
                for (int h = startHour; h < 24; ++h) {
                    int colX = (h < 12) ? leftColX : rightColX;
                    int row = ((h) % 12);
                    int y = starty + row * rowHeight;

                    bool invert = isToday && h == currTime.tm_hour;
                    uint16_t fg = invert ? GxEPD_WHITE : GxEPD_BLACK;
                    uint16_t bg = invert ? GxEPD_BLACK : GxEPD_WHITE;

                    display.fillRect(colX, y, midW / 2 - 10, rowHeight, bg);
                    display.setTextColor(fg);
                    display.setCursor(colX, y);

                    char hourStr[15];
                    snprintf(hourStr, sizeof(hourStr), "%02d:00", h);
                    display.print(hourStr);
                    display.print(" | ");
                    display.printf("%.1f C | ", fc.temperature_hourly[h]);

                    switch(fc.weather_code_hourly[h]) {
                        case CLEAR_SKIES: display.println("Clear"); break;
                        case CLOUDY: display.println("Cloudy"); break;
                        case OVERCAST: display.println("Overcast"); break;
                        case FOGGY: display.println("Foggy"); break;
                        case DRIZZLE: display.println("Drizzle"); break;
                        case RAINY: display.println("Rain"); break;
                        case SNOWY: display.println("Snow"); break;
                        case STORMY: display.println("Storm"); break;
                        default: display.println("Unknown"); break;
                    }
                }
            }

            // --- EVENT COUNT DISPLAY ---
            auto range = event_map.equal_range(d);
            int eventCount = std::distance(range.first, range.second);
            if (eventCount > 0) {
                display.println();
                display.setTextSize(2);
                display.print(eventCount);
                display.println(" Events on this day. Press ENTER to browse.");
            }

        } else {  auto range = event_map.equal_range(d);
            int eventCount = std::distance(range.first, range.second);

            if (eventCount == 0) {
                display.println("No events to display.");
            } else {
                // Advance iterator to current eventIndex
                auto it = range.first;
                std::advance(it, eventIndex % eventCount);
                const Event &ev = it->second;

                display.setTextSize(2);

                // --- Extra blank line before summary ---
                display.println();

                // --- Center the text horizontally ---
                int bodyWidth = midW - 10; // margin
                int cursorX = leftW + 5;

                // Helper lambda to center text
                auto printCentered = [&](const String &s) {
                    int16_t x1, y1;
                    uint16_t w, h;
                    display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
                    display.setCursor(cursorX + (bodyWidth - w)/2, display.getCursorY());
                    display.println(s);
                };

                // Summary
                printCentered("Summary:");
                printCentered(ev.summary);

                // Description
                printCentered("Description:");
                printCentered(ev.description);

                // Start / End times
                char buf[32];
                strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &ev.startTime);
                printCentered(String("Start: ") + buf);
                strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &ev.endTime);
                printCentered(String("End:   ") + buf);

                // Event counter [current/total] below
                display.println();
                display.setTextSize(2);
                printCentered("[" + String(eventIndex+1) + "/" + String(eventCount) + "]");

                // Exit label at the bottom
                display.println();
                printCentered("Press ENTER to exit");
            }

        }

    } while(display.nextPage());
}

// Full refresh
void fullRefresh(Date &d) {
    display.setFullWindow();
    display.firstPage();
    do {
        drawHeader(selected == 0, selected == 0 ? header_section : -1);
        drawBody(d);
    } while(display.nextPage());
    partial_count = 0;
}
// Helper: format tm into a string

int lastMinute = -1; // global/static, stores last displayed minute

void updateTimePartial() {
    tm now = getTime();
    if(now.tm_min != lastMinute) {
        lastMinute = now.tm_min;

        // Read temperature & humidity
        sensors_event_t humidityEvent, tempEvent;
        if (sht4.getEvent(&humidityEvent, &tempEvent)) {
            humidity = humidityEvent.relative_humidity;
            temperature = tempEvent.temperature;
        }

        // Prepare HH:MM string
        char timeStr[6];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.tm_hour, now.tm_min);

        // Prepare temperature + humidity string
        char envStr[20];
        snprintf(envStr, sizeof(envStr), "%.1f""C %.0f%%RH", temperature, humidity);

        // Calculate H1 width (same as in drawHeader)
        int headerH = display.height() * 16 / 100; // updated header height
        int charW = 6 * 2; // textSize=2
        int h1W = 5 * charW + charW;

        // Partial update for H1 only
        display.setPartialWindow(0, 0, h1W, headerH);
        display.firstPage();
        do {
            display.fillRect(0, 0, h1W, headerH, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);

            // Top line: time
            display.setCursor(2, 2);
            display.setTextSize(2);
            display.print(timeStr);

            // Second line: temp + humidity
            display.setCursor(2, headerH / 2 + 4);
            display.setTextSize(2);
            display.print(envStr);

        } while(display.nextPage());
    }
}


void provisioning()
{
    Serial.println("Starting SoftAP WiFi provisioning (Arduino WiFiProv)");

    // Stop any previous session and disconnect WiFi
    WiFiProv.endProvision();
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, true);
    }

    // --- Display instructions on EPAPER ---
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println("WiFi Provisioning");
    display.println();
    display.println("1. Open provisioning app");
    display.println("2. Follow instructions to connect device");
    display.println("Press any button to cancel");
    display.display(true); // full refresh

    // BLE device name shown on phone
    const char *service_name = "CALENDAR";
    const char *pop = nullptr; // optional proof-of-possession

    // Start provisioning (SoftAP)
    WiFiProv.beginProvision(
            NETWORK_PROV_SCHEME_SOFTAP,
            NETWORK_PROV_SCHEME_HANDLER_NONE,
            NETWORK_PROV_SECURITY_0,
            pop,
            service_name,
            0, 0, true
    );

    Serial.println("Waiting for provisioning to complete...");

    // --- Wait for WiFi connection or cancellation ---
    bool canceled = false;
    while (WiFi.status() != WL_CONNECTED && !canceled) {
        delay(100);

        // Check buttons
        if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN) ||
            btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT) || btn_pressed(BTN_ENTER)) {
            Serial.println("Provisioning canceled by user");
            canceled = true;
            break;
        }
    }

    // End provisioning session cleanly
    WiFiProv.endProvision();

    if (!canceled) {
        Serial.println("Provisioning complete");
    }

            String new_ssid = prefs.getString("sta_ssid", "");
            String new_pass = prefs.getString("sta_pass", "");

            prefs.end();
            prefs.begin("main", false);

            Serial.print("SSID: ");
            Serial.println(new_ssid);
            Serial.print("Password: ");
            Serial.println(new_pass);
            WiFiProv.endProvision();
            // Update UI
        }


void refresh() {
    Serial.println("Getting fresh token pair...");

    // Clear the display
    display.fillScreen(GxEPD_WHITE);

        display.setCursor(10, 20);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);
        display.print("Listening for access code on serial...");
    display.display();

    // Regenerate token pair (user-implemented)
    regenTokenPair();

    // Trigger full refresh of UI

    Serial.println("Refresh complete");
}

void staticDraw(const Date &d, tm now) {
    // Set full window for e-paper
    selected = 1;
    viewingEvents=0;
    display.setFullWindow();
    display.firstPage();
    do {
        // Header: dynamic like normal
        drawHeader(selected == 0, selected == 0 ? header_section : -1,now);

        // Body: date-specific
        drawBody(d,now);



    } while(display.nextPage());
}

void loopUI(Date currentDate) {
    unsigned long idleStart = millis();      // start idle timer
    unsigned long downHoldStart = 0;         // track BTN_DOWN hold
    const unsigned long idleTimeout = 30000; // 30 seconds
    const unsigned long holdTimeout = 3000;  // 3 seconds

    while (true)
    {
        unsigned long nowMillis = millis();
        updateTimePartial(); // keep time updated
        bool updated = false;

        bool anyButtonPressed = false;

        // Navigate header/body
        if (btn_pressed(BTN_UP)) {
            selected = 0;
            updated = true;
            anyButtonPressed = true;
        }
        if (btn_pressed(BTN_DOWN)) {
            selected = 1;
            updated = true;
            anyButtonPressed = true;

            // Handle BTN_DOWN hold for exit
            if (downHoldStart == 0) downHoldStart = nowMillis;
            else if (nowMillis - downHoldStart >= holdTimeout) {
                Serial.println("Exit: BTN_DOWN held 3s");
                return; // exit loop
            }
        } else {
            downHoldStart = 0; // reset hold if button released
        }
        if (viewingEvents) {
            auto range = event_map.equal_range(currentDate);
            int eventCount = std::distance(range.first, range.second);

            if (btn_pressed(BTN_LEFT)) {
                if (eventCount > 0) eventIndex = (eventIndex - 1 + eventCount) % eventCount;
                updated = true;
            }
            if (btn_pressed(BTN_RIGHT)) {
                if (eventCount > 0) eventIndex = (eventIndex + 1) % eventCount;
                updated = true;
            }
            if (selected == 1 && btn_pressed(BTN_ENTER)) {
                viewingEvents = false;
                updated = true;
            }
        }
        else
        {
            if (selected == 1 && btn_pressed(BTN_ENTER)) {
                viewingEvents = true;
                updated = true;
            }
        }
        // Body left/right changes date
        if (selected == 1) {
            if (btn_pressed(BTN_LEFT) && !viewingEvents) {
                currentDate.prevDay();
                updated = true;
                anyButtonPressed = true;
            }
            if (btn_pressed(BTN_RIGHT) && !viewingEvents ){
                currentDate.nextDay();
                updated = true;
                anyButtonPressed = true;
            }
        }
        else { // header navigation
            if (btn_pressed(BTN_LEFT)) {
                header_section=0;
                updated = true;
                anyButtonPressed = true;
            }
            if (btn_pressed(BTN_RIGHT)) {
                header_section=1;
                updated = true;
                anyButtonPressed = true;
            }
        }

        // Header actions (provisioning, refresh)
        if (selected == 0 && btn_pressed(BTN_ENTER)) {
            anyButtonPressed = true;
            if (header_section == 0) provisioning();
            else if (header_section == 1) refresh();
            fullRefresh(currentDate);
            updated = true;
        }

        // Body ENTER: events view toggle
        if (selected == 1 && btn_pressed(BTN_ENTER)) {
            anyButtonPressed = true;
            // toggle viewing events for the day

            fullRefresh(currentDate);
            updated = true;
        }

        // Update idle timer
        if (anyButtonPressed) idleStart = nowMillis;

        // Partial or full redraw
        if (updated) {
            partial_count++;
            if (partial_count >= PARTIAL_LIMIT) {
                display.setFullWindow();
                display.firstPage();
                do {
                    drawHeader(selected == 0, selected == 0 ? header_section : -1);
                    drawBody(currentDate);
                } while(display.nextPage());
                partial_count = 0;
            } else {
                drawHeader(selected == 0, selected == 0 ? header_section : -1);
                drawBody(currentDate);
            }
            updated = false;
        }



        // --- Exit conditions ---
        if (nowMillis - idleStart >= idleTimeout) {
            Serial.println("Exit: idle timeout 30s");
            return; // exit loop
        }

        delay(10); // small delay to avoid busy loop
    }
}
void manual()
{
    DBG_PRINTLN("Manual!");

    //TEST SPACE!
    pinMode(BTN_UP,    INPUT);
    pinMode(BTN_DOWN,  INPUT);
    pinMode(BTN_LEFT,  INPUT);
    pinMode(BTN_RIGHT, INPUT);
    pinMode(BTN_ENTER, INPUT);
    bool updated=false;

    auto now = getTime();
    if (now.tm_year<120)
    {
        time_t t;
        prefs.getBytes("lastTime", &t, sizeof(t));
        //Boots ~1 min apart
        auto boots_left = prefs.getUShort("bootsLeft");

        auto boot_count = prefs.getUShort("bootsTillSync")-boots_left+1;

        t+= boot_count * 60;
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
        tzset();
        now = *localtime(&t);
    }
    networkSync();
    saveToCache();

    auto nowdate = Date(now.tm_year,now.tm_mon,now.tm_mday);
    // Initial render
    drawBody(nowdate);
    drawHeader(selected == 0, selected == 0 ? header_section : -1);
    loopUI(nowdate);
    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    staticDraw(nowdate,now);
    auto now_t = std::mktime(&now);

    prefs.putBytes("lastTime", &now_t, sizeof(now_t));

    prefs.putUShort("bootsLeft",prefs.getUShort("bootsTillSync"));
    end();
}

void automatic()
{
    DBG_PRINTLN("Auto!");
    tm now;
    //Check if need to sync
    auto boots_left = prefs.getUShort("bootsLeft");
    if(boots_left==0)
    {
        //If yes: fetch new content overriding the cache and reset boot counter
        networkSync();
        saveToCache();

        now = getTime();
        //disconnect WiFi as it's no longer needed
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        auto now_t = std::mktime(&now);
        prefs.putBytes("lastTime", &now_t, sizeof(now_t));


        prefs.putUShort("bootsLeft",prefs.getUShort("bootsTillSync"));

    }
    else
    {
        //If not: Load and display cached data, decrement boot counter
        // "approx. now time"
        time_t t;
        prefs.getBytes("lastTime", &t, sizeof(t));
        //Boots ~1 min apart

        auto boot_count = prefs.getUShort("bootsTillSync")-boots_left+1;

        t+= boot_count * 60;
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
        tzset();
        now = *localtime(&t);

        loadFromCache();
        prefs.putUShort("bootsLeft",boots_left-1);


    }
    // Print



    Serial.print("Automatic boot - boots till sync #:");
    Serial.println(boots_left);
    Serial.print("Current time: ");
    Serial.print(now.tm_hour);
    Serial.print(":");
    Serial.print(now.tm_min);
    Serial.print(":");
    Serial.print(now.tm_sec);
    auto nowdate = Date(now.tm_year,now.tm_mon,now.tm_mday);
    staticDraw(nowdate, now);

    //Turn everything off
    end();

}
void setup(void *arg)
{
    wifi_retries = 30;
    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT); //drain cap
    gpio_pulldown_en(GPIO_NUM_40);

    vbat = battery_voltage();
    if (vbat<5.8)
    {
        end();
    }

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
        auto now_t = std::mktime(&now);
        prefs.putBytes("lastTime", &now_t, sizeof(now_t));

    }
    pinMode(35,OUTPUT);
    pinMode(16,OUTPUT);
    pinMode(17,OUTPUT);

    display.init(115200, true, 2, false);
    display.setRotation(0); // optional, 0-3 depending on orientation
    display.clearScreen();
    display.display();
    //display.invertDisplay(true);

    sht4.begin();
    sensors_event_t humidityEvent, tempEvent;
    if (sht4.getEvent(&humidityEvent, &tempEvent)) {
        humidity = humidityEvent.relative_humidity;
        temperature = tempEvent.temperature;
    }

    // BRANCH: WAKEREASON
    if(is_manual) manual();
    else automatic();
    end();

    auto creator = static_cast<TaskHandle_t>(arg);
    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);

}
void test()
{





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
