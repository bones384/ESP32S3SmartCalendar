#include <Arduino.h>
#include <Adafruit_FRAM_SPI.h>
#define CONFIG_FREERTOS_UNICORE 1 // is this still required?
#include <Adafruit_SHT4x.h>
#include <esp_phy_init.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <map>
#include <nvs.h>
#include <driver/rtc_io.h>
#include <esp_adc/adc_oneshot.h>
String password = "Mumia!24";
String ssid = "foxnet253";
#include <HTTPClient.h>
#include <GxEPD2_3C.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ctime>
// Pins
#define CS_PIN    35
#define FRAM_CS   5
#define DC_PIN    17
#define RST_PIN   16
#define BUSY_PIN  4

#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#define DBG_PRINTF(...)
#endif
#define BTN_LEFT     39
#define BTN_DOWN   36
#define BTN_UP   38
#define BTN_RIGHT  37
#define BTN_ENTER  6

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));
void printStackUsage(const char* tag)
{
    UBaseType_t hw = uxTaskGetStackHighWaterMark(nullptr);
    DBG_PRINTF("[%s] Stack high water mark: %u bytes\n", tag, hw * sizeof(StackType_t));
}
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

const char* ntpServer = "pool.ntp.org";
constexpr long  gmtOffset_sec = 3600;
constexpr int   daylightOffset_sec = 3600;

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool cali_enabled = false;

void adc_init()
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            ADC_CHANNEL_0,   // GPIO1
            &chan_cfg
        )
    );

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle) == ESP_OK) {
        cali_enabled = true;
    }
}

int adc_read_mv()
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &raw));

    if (cali_enabled) {
        int mv;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));
        return mv;
    }

    /* Fallback if calibration unavailable */
    return (raw * 3900) / 4095;
}

float battery_voltage()
{
    const int adc_mv = adc_read_mv();
    return (adc_mv/1000.f) * 2.0f;
}

//Manual or first poweron of battery pack
void manual()
{
    DBG_PRINTLN("Manual!");
}

void automatic()
{
    DBG_PRINTLN("Auto!");
}
void end()
{
    prefs.end();
    DBG_PRINTLN("Buhbye!");
    gpio_set_level(GPIO_NUM_41,1); //DONE
    gpio_set_level(GPIO_NUM_21,1); //stop hold
}
constexpr uint8_t wifi_retries = 20;
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
                return false;
            }
        }
        DBG_PRINTLN("");
        DBG_PRINTLN("WiFi connected.");
    }
    return true;
}

constexpr char CLIENT_ID[] = "CLIENT_ID_HERE";
constexpr char CLIENT_SECRET[] = "CLIENT_SECRET_HERE"; //! THIS SHOULD BE ENCRYPTED AND STORED IN NVS!!!
constexpr char regenurl[] = "https://oauth2.googleapis.com/token";

bool regenTokenPair()
{

    DBG_PRINTLN("Google API connection expired or not initialised. Please enter your access code: ");
    String AUTHORIZATION_CODE;
    AUTHORIZATION_CODE.reserve(256);


    while (!Serial.available()) {delay(10);}
    while (true)
    {
        if (Serial.available()){
            char c = Serial.read();
            if (c == '\r')
            {
                break;
            }
            AUTHORIZATION_CODE+=c;
            DBG_PRINT(c);

        }
        delay(10);
    }

    DBG_PRINTLN();
    DBG_PRINT("Got code:");
    DBG_PRINTLN(AUTHORIZATION_CODE);
    DBG_PRINTLN("Token generation beginning.");
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - token regeneration aborted.");
        return false;
    }
    // Send get for code
    String payload;
    payload.reserve(256);
     payload = String("client_id=") + CLIENT_ID +
                            "&client_secret=" + CLIENT_SECRET +
                            "&redirect_uri=urn:ietf:wg:oauth:2.0:oob"
                            "&code=" + AUTHORIZATION_CODE +
                            "&grant_type=authorization_code";
    HTTPClient http;
    http.setReuse(false);
    http.begin(regenurl);
    DBG_PRINT("Requesting URL ... ");
    DBG_PRINT("Requesting URL .1.. ");

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(payload);
    DBG_PRINT("Requesting URL ..2. ");

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
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];
            const char* refresh_token = doc["refresh_token"];

            DBG_PRINT("AT: "); DBG_PRINTLN(access_token);
            DBG_PRINT("RT: "); DBG_PRINTLN(refresh_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            prefs.putString("REFRESH_TOKEN",refresh_token);
            http.end();
            return true;
        }

    }
    else {
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
    }

    // Free resources
    http.end();
    return false;

}

constexpr char refreshurl[] = "https://oauth2.googleapis.com/token";
bool refreshToken()
{
    //? If this is called, we have a valid refresh token in NVS.
    DBG_PRINTLN("Token refresh beginning.");
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - token refresh aborted.");
        return false;
    }
    // Send post for refresh
    String payload;
    payload.reserve(256);
    payload = String("client_id=") + CLIENT_ID +
                            "&client_secret=" + CLIENT_SECRET +
                            "&refresh_token=" + prefs.getString("REFRESH_TOKEN") +
                            "&grant_type=refresh_token";
    HTTPClient http;
    http.setReuse(false);
    printStackUsage("before");
    http.begin(refreshurl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(payload);
    printStackUsage("afta");
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
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];

            DBG_PRINT("AT: "); DBG_PRINTLN(access_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            http.end();
            return true;
        }

    }
    else {
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
    }

    // Free resources
    http.end();
    return false;
}
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

#define INCLUDE_vTaskDelete 1
void refreshTask(void *arg)
{
    auto creator = static_cast<TaskHandle_t>(arg);
    refreshToken();
    DBG_PRINTLN("Task finished");
    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);   // delete this task when done

}
String getAccessToken()
{
    //? Check if we have an access token in NVS.
    if(prefs.isKey("ACCESS_TOKEN"))
    {
        //? We do -> return it.
        return prefs.getString("ACCESS_TOKEN");
    }
    //? We don't. check if we have a refresh token in NVS.
    DBG_PRINTLN("Access token expired.");
    if(prefs.isKey("REFRESH_TOKEN"))
    {
        DBG_PRINTLN("Renewing with refresh token...");
        //? We do -> use it to generate a new access token.
        if(refreshToken()) return prefs.getString("ACCESS_TOKEN");
        //? Refresh failed!
        DBG_PRINTLN("Renewal failed! Regenerate token pair!");
        return "";
    }
    //? No token to return
    DBG_PRINTLN("No access token stored! Regenerate token pair!");
    return "";
}
bool timeset = false;
tm getTime()
{
    if (!timeset) syncTime();

    time_t now;
    time(&now);

    tm local_tm{};
    localtime_r(&now, &local_tm);
    return local_tm;
}
constexpr char calendarurl[] = "https://www.googleapis.com/calendar/v3/";
constexpr char listurl[] = "calendars/primary/events?";

void fixTZ(char *buf) {
    if (size_t len = strlen(buf); len > 2) {
        memmove(buf + len - 1, buf + len - 2, 3);
        buf[len - 2] = ':';
    }
}

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
            return lhs.year < rhs.year;
        }
        if (lhs.month!=rhs.month )return lhs.month < rhs.month;
        return lhs.month < rhs.month;

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
        event.summary = item["summary"].as<String>();
        event.description = item["summary"].as<String>();
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
void setup(void *arg) {

    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT); //drain cap
    gpio_pulldown_en(GPIO_NUM_40);
    Serial.begin(115200);
    DBG_PRINTLN("Woken up!");
    DBG_PRINT("Pin 40 (WAKEREASON): ");
    DBG_PRINTLN(is_manual);
    printStackUsage("beginning of setup");
    prefs.begin("main",false);
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

    getTime();
    loadEvents(getCalendarEvents());
    for (auto event : events)
    {
        DBG_PRINTLN(event.summary);
        DBG_PRINTLN(event.description);
        DBG_PRINTLN(&event.startTime, "%A, %B %d %Y %H:%M:%S");
        DBG_PRINTLN(&event.endTime, "%A, %B %d %Y %H:%M:%S");
        DBG_PRINTLN();
    }
    // Send HTTP GET request
    loadForecast();
    for (auto &forecast : forecasts)
    {
        DBG_PRINTLN(forecast.weather_code_daily);
    }

    //We now have fresh forecast data and fresh event data - merge it on a per date basis
    std::multimap<Date, Event> event_map;
    std::multimap<Date, Forecast> forecast_map;
    for(auto &event : events)
    {

       event_map.insert(std::make_pair(Date(event.startTime.tm_year,event.startTime.tm_mon,event.startTime.tm_mday),event));
    }
    auto n = getTime();

    //! fix to handle new year case
    int idx = 0;
    for(auto forecast : forecasts)
    {
        idx++;
        tm copy = n;
        tm* t = &copy;
        copy.tm_mday+=idx;
        auto time_t_now = mktime(&copy);
        t = localtime(&time_t_now);

        forecast_map.insert(std::make_pair(Date(t->tm_year,t->tm_mon,t->tm_mday),forecast));
    }

    for(auto &x: event_map)
    {
        DBG_PRINTLN();
        DBG_PRINTLN(x.first.year+1900);
        DBG_PRINTLN(x.first.month);

        DBG_PRINTLN(x.first.day);

        DBG_PRINTLN(x.second.summary);
        DBG_PRINTLN(x.second.description);
        DBG_PRINTLN(x.second.startTime.tm_hour);
        DBG_PRINTLN(x.second.endTime.tm_hour);

    }
    for(auto &x: forecast_map)
    {
        DBG_PRINTLN();
        DBG_PRINTLN(x.first.year+1900);
        DBG_PRINTLN(x.first.month+1);

        DBG_PRINTLN(x.first.day);

        DBG_PRINTLN(x.second.temperature_daily_max);
        DBG_PRINTLN(x.second.temperature_daily_min);
        DBG_PRINTLN(x.second.weather_code_daily);
    }
    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);


    end();
    auto creator = static_cast<TaskHandle_t>(arg);

    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);   }
void test()
{

    // Init and get the time
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    tm timeinfo;
    while(!getLocalTime(&timeinfo)){
        DBG_PRINTLN("Failed to obtain time");
        delay(100);
    }
    DBG_PRINTLN(&timeinfo, "%A, %B %d %Y %H:%M:%S");

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
    DBG_PRINTLN(timestamp);

    if (!fram.begin())
    {
        DBG_PRINT("FRAM NOT FOUND!!!!!!!!");

    }

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
    adc_init();

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
