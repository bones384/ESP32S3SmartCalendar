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
#include "defines.h"
#include <cstdint>
#include <driver/rtc_io.h>
#include <misc/lv_color.h>
String password = "Mumia!25";
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

#include <lvgl.h>
#include "dbgprint.h"
#include <Adafruit_FRAM_SPI.h>
#include "time.hpp"
#include "fram.hpp"
#include "structs.hpp"
#include "tokens.hpp"
#include "connect.hpp"
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));

lv_style_t style;
lv_style_t weatherstyle;


LV_FONT_DECLARE(myfont);
LV_FONT_DECLARE(weather);

static bool btn_pressed(int gpio)
{
    return gpio_get_level((gpio_num_t)gpio) == 0;
}


 constexpr uint16_t DISP_W = 400;
 constexpr uint16_t DISP_H = 300;

 uint8_t fb[((DISP_W * DISP_H)/8)+16];

 lv_display_t* disp;
 lv_obj_t* label;
#define LV_FONT_UNSCII_16 1

static void epd_flush(lv_display_t* disp,
                      const lv_area_t* area,
                      uint8_t* px_map)
{
    display.setFullWindow();

   // delay(500);
    //return;
    uint8_t px_map_cpy [((DISP_H * DISP_W)/8)+16];
    for (size_t i = 8; i < ((DISP_H * DISP_W)/8)+16; i++)
        px_map_cpy[i] = ~px_map[i];
    uint8_t *ptr = px_map_cpy;
    ptr+=8;
    display.firstPage();
    do {
        // GxEPD expects 1 = black, LVGL uses 1 = white
        display.drawBitmap(
           0,
           0,
            ptr,
            400,
            300,
            GxEPD_BLACK
        );
    } while (display.nextPage());

    lv_display_flush_ready(disp);
}
static lv_indev_t* keypad_indev;
static uint32_t last_key = 0;

static void keypad_read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    data->state = LV_INDEV_STATE_RELEASED;
    data->key   = last_key;

    if (btn_pressed(BTN_UP)) {
        last_key = LV_KEY_UP;
    }
    else if (btn_pressed(BTN_DOWN)) {
        last_key = LV_KEY_PREV;
    }
    else if (btn_pressed(BTN_LEFT)) {
        last_key = LV_KEY_NEXT;
    }
    else if (btn_pressed(BTN_RIGHT)) {
        last_key = LV_KEY_RIGHT;
    }
    else if (btn_pressed(BTN_ENTER)) {
        last_key = LV_KEY_ENTER;
    }
    else
    {
        return;
    }
    data->state=LV_INDEV_STATE_PRESSED;
    data->key=last_key;
}

void lv_port_indev_init(void)
{

    keypad_indev = lv_indev_create();
    if (keypad_indev==nullptr) Serial.println("KEYPADNULL!!!!");
    lv_indev_set_type(keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad_indev, keypad_read_cb);
}
lv_group_t* group;
void lv_focus_init(void)
{
    group = lv_group_create();
    lv_group_set_default(group);
    lv_group_set_editing(group,false);
    lv_indev_set_group(keypad_indev, group);
}

static void style_focus_init(void)
{
    static lv_style_t style_focus;
    lv_style_init(&style_focus);
    lv_style_set_outline_width(&style_focus, 2);
    lv_style_set_outline_pad(&style_focus, 2);
    lv_style_set_outline_color(&style_focus, lv_color_black());

    lv_obj_add_style(lv_scr_act(), &style_focus, LV_STATE_FOCUSED);
}

void lv_port_disp_init()
{
    lv_init();

    lv_style_init(&style);
    lv_style_set_text_font(&style, &myfont);
    lv_style_init(&weatherstyle);


    lv_style_set_text_font(&weatherstyle, &weather);
    disp = lv_display_create(DISP_W, DISP_H);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(
        disp,
        fb,
        nullptr,
        sizeof(fb),
        LV_DISPLAY_RENDER_MODE_DIRECT
    );
    lv_display_set_flush_cb(disp, epd_flush);

    lv_port_indev_init();
    lv_focus_init();

}

static lv_style_t style_btn;
static lv_style_t style_btn_focused;

static void init_styles(void)
{
    /* ---------- Normal button ---------- */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_white());
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn, lv_color_black());
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_color(&style_btn, lv_color_black());
    lv_style_set_radius(&style_btn, 2);

    /* ---------- Focused button ---------- */
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_color_black());
    lv_style_set_bg_opa(&style_btn_focused, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_focused, lv_color_white());
    lv_style_set_border_width(&style_btn_focused, 1);
    lv_style_set_border_color(&style_btn_focused, lv_color_black());
}

#define ICON_UNKNOWN       "\xEE\xA9\xB6"
#define ICON_CLEAR_SKIES   "\xEE\xA6\xBA"
#define ICON_CLOUDY        "\xEE\xA5\xA4"
#define ICON_OVERCAST      "\xEE\xA5\xA6"
#define ICON_FOGGY         "\xEE\xA6\xAE"
#define ICON_DRIZZLE       "\xEE\xA4\xAA"
#define ICON_RAINY         "\xEE\xA6\xB1"
#define ICON_SNOWY         "\xEE\xA5\xBB"
#define ICON_STORMY        "\xEE\xA8\x92"

const char* icons[9] = {ICON_UNKNOWN,ICON_CLEAR_SKIES, ICON_CLOUDY, ICON_OVERCAST,
                ICON_FOGGY, ICON_DRIZZLE, ICON_RAINY,
                ICON_SNOWY, ICON_STORMY};

static void create_ui(void)
{
    init_styles();
    /* Root screen */
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* ---------------- Header ---------------- */

    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_PCT(10));
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_set_style_pad_all(header, 4, 0);

    lv_obj_t* header_label = lv_label_create(header);
    lv_label_set_text(header_label, "Sample Header Text");
    lv_obj_set_style_text_font(header_label, &myfont, 0);
    lv_obj_center(header_label);

    /* ---------------- Body ---------------- */

    lv_obj_t* body = lv_obj_create(scr);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, LV_PCT(90));
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_t* btn;
    /* Padding from screen edges */
    lv_obj_set_style_pad_all(body, 6, 0);

    /* ---------------- Grid container ---------------- */

    lv_obj_t* grid = lv_obj_create(body);

    /* Grid takes 80% of body in both dimensions */
    lv_obj_set_size(grid, LV_PCT(80), LV_PCT(80));
    lv_obj_center(grid);

    /* Internal spacing */
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_set_style_pad_row(grid, 4, 0);
    lv_obj_set_style_pad_column(grid, 4, 0);

    /* Define 3x3 grid (all equal, percentage-based) */
    static int32_t col_dsc[] = {
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static int32_t row_dsc[] = {
            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    /* ---------------- Buttons ---------------- */

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            btn = lv_button_create(grid);
            lv_obj_set_grid_cell(
                    btn,
                    LV_GRID_ALIGN_STRETCH, c, 1,
                    LV_GRID_ALIGN_STRETCH, r, 1
            );
            /* Base style */
            lv_obj_add_style(btn, &style_btn, 0);
            lv_group_add_obj(group,btn);
            /* Focused style */
            lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
                lv_obj_t* lbl = lv_label_create(btn);
            lv_obj_set_style_text_font(lbl, &weather, 0);
            lv_label_set_text(lbl,icons[r*3+c]);
            lv_obj_center(lbl);
        }
    }

}



#define LV_COLOR_DEPTH              1
#define LV_MEM_SIZE                 (128 * 1024)
#define LV_DISP_DEF_REFR_PERIOD     500
#define LV_USE_ANIMATION            0
#define LV_USE_GPU                  0
#define LV_USE_SHADOW               0
#define LV_USE_GRADIENT             0

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
    tm now;
    //Check if need to sync
    auto boots_left = prefs.getUShort("bootsLeft");
    if(boots_left==0)
    {
        //If yes: fetch new content overriding the cache and reset boot counter
        networkSync();
        saveToCache();

        now = getTime();
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

    display.init(115200, true, 2, false);
    display.setRotation(0); // optional, 0-3 depending on orientation

    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 0);
    display.setTextColor(GxEPD_BLACK);
    // sda 8 scl 9
    display.setTextSize(2);
    display.println(&now, "%A, %B %d %Y %H:%M:%S");
    display.display();
    display.hibernate();

    Serial.print("Automatic boot - boots till sync #:");
    Serial.println(boots_left);
    Serial.print("Current time: ");
    Serial.print(now.tm_hour);
    Serial.print(":");
    Serial.print(now.tm_min);
    Serial.print(":");
    Serial.print(now.tm_sec);
    delay(500);
    //Turn everything off
    end();

}
void end()
{
    prefs.end();
    DBG_PRINTLN("Buhbye!");
    gpio_set_level(GPIO_NUM_41,1); //DONE
    gpio_set_level(GPIO_NUM_21,1); //stop hold
    while(true) delay(500);
}



std::multimap<Date, Event> event_map;
std::map<Date, Forecast> forecast_map;

void setup(void *arg)
{
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
        auto now_t = std::mktime(&now);
        prefs.putBytes("lastTime", &now_t, sizeof(now_t));

    }
    pinMode(35,OUTPUT);
    pinMode(16,OUTPUT);
    pinMode(17,OUTPUT);
    //lvgl test
    display.init(115200, true, 2, false);
    display.setRotation(0); // optional, 0-3 depending on orientation
    //display.invertDisplay(true);
    lv_port_disp_init();

    //TEST SPACE!
    pinMode(BTN_UP,    INPUT);
    pinMode(BTN_DOWN,  INPUT);
    pinMode(BTN_LEFT,  INPUT);
    pinMode(BTN_RIGHT, INPUT);
    pinMode(BTN_ENTER, INPUT);
    create_ui();
    style_focus_init();

    /* One initial render */
    while (true)
    {
        lv_tick_inc(50);
        lv_timer_handler();
        delay(50);
    }



    end();

    // BRANCH: WAKEREASON
    if(is_manual) manual();
    else automatic();
    end();




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
