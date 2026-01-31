#include <Arduino.h>
#include <Adafruit_FRAM_SPI.h>
#define CONFIG_FREERTOS_UNICORE 1 // is this still required?
#include <Adafruit_SHT4x.h>
#include <esp_phy_init.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <map>
#include <nvs.h>
#include <nvs_flash.h>
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

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN));
void printStackUsage(const char* tag)
{
    UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("[%s] Stack high water mark: %u bytes\n", tag, hw * sizeof(StackType_t));
}
int is_manual;
extern "C" void app_main(void)
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
    gpio_set_level(GPIO_NUM_41, 0);



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

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);

Preferences prefs;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool cali_enabled = false;

void adc_init(void)
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

int adc_read_mv(void)
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

float battery_voltage(void)
{
    int adc_mv = adc_read_mv();
    return (adc_mv/1000.f) * 2.0f;
}

//Manual or first poweron of battery pack
void manual()
{
    Serial.println("Manual!");
}

void automatic()
{
    Serial.println("Auto!");
}
void end()
{
    prefs.end();
    Serial.println("Buhbye!");
    gpio_set_level(GPIO_NUM_41,1); //DONE
    gpio_set_level(GPIO_NUM_21,1); //stop hold
}
constexpr uint8_t wifi_retries = 20;
bool connect()
{
    if(WiFi.status()!=WL_CONNECTED)
    {
        char retries = wifi_retries;
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            retries -= 1;
            if (retries == 0) {
                Serial.println("");
                Serial.print("!!!WiFi connection failed after "); Serial.print(wifi_retries); Serial.println(" tries.");
                return false;
            }
        }
        Serial.println("");
        Serial.println("WiFi connected.");
    }
    return true;
}

constexpr char CLIENT_ID[] = "CLIENT_ID_HERE";
constexpr char CLIENT_SECRET[] = "CLIENT_SECRET_HERE"; //! THIS SHOULD BE ENCRYPTED AND STORED IN NVS!!!
constexpr char regenurl[] = "https://oauth2.googleapis.com/token";

bool regenTokenPair()
{

    Serial.println("Google API connection expired or not initialised. Please enter your access code: ");
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
            Serial.print(c);

        }
        delay(10);
    }

    Serial.println();
    Serial.print("Got code:");
    Serial.println(AUTHORIZATION_CODE);
    Serial.println("Token generation beginning.");
    if(!connect())
    {
        Serial.println("WiFi connection failed - token regeneration aborted.");
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
    Serial.print("Requesting URL ... ");
    Serial.print("Requesting URL .1.. ");

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(payload);
    Serial.print("Requesting URL ..2. ");

    if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        http.end();
        Serial.println(response);

        StaticJsonDocument<480> doc;

        DeserializationError error = deserializeJson(doc, response);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            http.end();
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];
            const char* refresh_token = doc["refresh_token"];

            Serial.print("AT: "); Serial.println(access_token);
            Serial.print("RT: "); Serial.println(refresh_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            prefs.putString("REFRESH_TOKEN",refresh_token);
            http.end();
            return true;
        }

    }
    else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }

    // Free resources
    http.end();
    return false;

}

const char refreshurl[] = "https://oauth2.googleapis.com/token";
bool refreshToken()
{
    //? If this is called, we have a valid refresh token in NVS.
    Serial.println("Token refresh beginning.");
    if(!connect())
    {
        Serial.println("WiFi connection failed - token refresh aborted.");
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
    if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        http.end();
        Serial.println(response);

        StaticJsonDocument<480> doc;

        DeserializationError error = deserializeJson(doc, response);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            http.end();
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];

            Serial.print("AT: "); Serial.println(access_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            http.end();
            return true;
        }

    }
    else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }

    // Free resources
    http.end();
    return false;
}
#define INCLUDE_vTaskDelete 1
void refreshTask(void *arg)
{
    TaskHandle_t creator = static_cast<TaskHandle_t>(arg);
    refreshToken();
    Serial.println("Task finished");
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
    Serial.println("Access token expired.");
    if(prefs.isKey("REFRESH_TOKEN"))
    {
        Serial.println("Renewing with refresh token...");
        //? We do -> use it to generate a new access token.
        if(refreshToken()) return prefs.getString("ACCESS_TOKEN");
        //? Refresh failed!
        Serial.println("Renewal failed! Regenerate token pair!");
        return "";
    }
    //? No token to return
    Serial.println("No access token stored! Regenerate token pair!");
    return "";
}



void setup(void *arg) {

    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT); //drain cap
    gpio_pulldown_en(GPIO_NUM_40);
    Serial.begin(115200);
    Serial.println("Woken up!");
    Serial.print("Pin 40 (WAKEREASON): ");
    Serial.println(is_manual);
    printStackUsage("beginning of setup")
    prefs.begin("main",false);
    // BRANCH: WAKEREASON
    if(is_manual) manual();
    else automatic();

    //TEST SPACE!
    //TODO: MOVE PAIRGEN TO TASK TOO
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);


    //test();

    if(is_manual)
    {
       //if (!refreshToken()) {regenTokenPair();}
//        TaskHandle_t oauthHandle = nullptr;
//        xTaskCreate(
//            refreshTask,
//            "oauth",
//            16384, //8k might be enough?
//            xTaskGetCurrentTaskHandle(),
//            5,
//            &oauthHandle
//        );
//
//        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

refreshToken()

    }

    Serial.println(getAccessToken());

    // Send HTTP GET request


    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);


    end();
    TaskHandle_t creator = static_cast<TaskHandle_t>(arg);

    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);   // delete this task when done

    return;
}
void test()
{
    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    while(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        delay(100);
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    pinMode(35,OUTPUT);
    pinMode(16,OUTPUT);
    pinMode(17,OUTPUT);

    Serial.println("Adafruit SHT4x test");
    if (! sht4.begin()) {
        Serial.println("Couldn't find SHT4x");

    }
    Serial.println("Found SHT4x sensor");
    Serial.print("Serial number 0x");
    Serial.println(sht4.readSerial(), HEX);

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);

    sensors_event_t humidity, temp;

    uint32_t timestamp = millis();
    sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    timestamp = millis() - timestamp;
    Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
    Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

    Serial.print("Read duration (ms): ");
    Serial.println(timestamp);

    if (!fram.begin())
    {
        Serial.print("FRAM NOT FOUND!!!!!!!!");

    }

    fram.exitSleep();
    fram.writeEnable(1);
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

    TaskHandle_t creator = static_cast<TaskHandle_t>(arg);
    xTaskNotifyGive(creator);
    vTaskDelete(nullptr);   // delete this task when done
}
