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

#include <GxEPD2_3C.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <ArduinoJson.h>
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

struct Getter
{
    WiFiClient client;
    bool connected = false;
    std::string url;
    void connect(const std::string& URL, const int port = 80)
    {
        client.connect(URL.c_str(),port);
        url=URL;
    }
    void sendHeader(const std::string& name,const std::string& value)
    {
        client.println((name + ": " + value).c_str());
    }
    JsonDocument get(const std::string& resource_path)
    {
        client.println(("GET " + resource_path + " HTTP/1.1").c_str());
        client.println(("Host: " + url).c_str());
        client.println("Accept: */*");
        client.println();

        std::string head, body;
        std::map<std::string,std::string> headers;
        std::string start_line = client.readStringUntil('\n').c_str();

        while (true)
        {
            if (client.available())
            {
                std::string temp;
                temp = client.readStringUntil('\n').c_str();
                if (temp=="\r")
                {
                    break;
                }
                head+=temp;
                Serial.print(temp.c_str());
                auto pos = temp.find(':');
                headers[temp.substr(0,pos)] = temp.substr(pos+2,temp.length()-pos-3);
            };

        }
        bool chunked = false;
        if (headers.find("Content-Length")==headers.end())
        {
            if (headers.find("Transfer-Encoding")==headers.end())
            {
                Serial.println("Invalid Head! No content length nor transfer encoding header!");
                //throw std::runtime_error("Invalid header!");
            }
            if (headers["Transfer-Encoding"]!="chunked")
            {
                Serial.println(("Transfer-Encoding values other than chunked are not supported! Got: " + headers.at("Transfer-Encoding")).c_str());
                while (true) {}


            }
            chunked = true;
        }

        if (!chunked) {
            int cl = stoi(headers.at("Content-Length"));

            char* buf = new char[cl];
            client.readBytes(buf,cl);
            body.assign(buf,cl);
            delete [] buf;
        }
        else
        {
            while (true)
            {
                if (client.available())
                {


                        Serial.println("loop");
                        String ass = client.readStringUntil('\n');
                        //String ass2 = client.readStringUntil('\n');

                        int chunkSize = std::strtoul(ass.c_str(), nullptr, 16);
                        if (chunkSize == 0) break;
                        char* test;
                        test = new char[chunkSize];
                        client.readBytes(test, chunkSize);
                        String ass2;
                        ass2.concat(test, chunkSize);
                        delete [] test;
                        Serial.println(ass);
                        Serial.println(chunkSize);
                        Serial.println("mid");
                        Serial.println(ass2);
                        Serial.println("end");
                        body += ass2.c_str();



                }
            }
        }
        JsonDocument JSON;
        deserializeJson(JSON,body);
        return JSON;

    }
};
int x;
extern "C" void app_main(void)
{
    initArduino();

    gpio_reset_pin(GPIO_NUM_40);
    gpio_reset_pin(GPIO_NUM_41);
    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT);
    x = gpio_get_level(GPIO_NUM_40);


    gpio_reset_pin(GPIO_NUM_41);
    gpio_set_direction(GPIO_NUM_41, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT_OD);
    gpio_pullup_dis(GPIO_NUM_41);
    gpio_pullup_dis(GPIO_NUM_21);
   // gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);

    gpio_set_level(GPIO_NUM_21, 0); //HOLD
    //delay(1000);
    gpio_set_level(GPIO_NUM_41, 0);
    //delay(50);
    //delay(5000);
   // gpio_set_level(GPIO_NUM_21, 1); // stop hold

    setup();

    while (true) {
        loop();
        vTaskDelay(1);
    }
}

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);


void printLocalTime()
{
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("Day of week: ");
    Serial.println(&timeinfo, "%A");
    Serial.print("Month: ");
    Serial.println(&timeinfo, "%B");
    Serial.print("Day of Month: ");
    Serial.println(&timeinfo, "%d");
    Serial.print("Year: ");
    Serial.println(&timeinfo, "%Y");
    Serial.print("Hour: ");
    Serial.println(&timeinfo, "%H");
    Serial.print("Hour (12 hour format): ");
    Serial.println(&timeinfo, "%I");
    Serial.print("Minute: ");
    Serial.println(&timeinfo, "%M");
    Serial.print("Second: ");
    Serial.println(&timeinfo, "%S");

    Serial.println("Time variables");
    char timeHour[3];
    strftime(timeHour,3, "%H", &timeinfo);
    Serial.println(timeHour);
    char timeWeekDay[10];
    strftime(timeWeekDay,10, "%A", &timeinfo);
    Serial.println(timeWeekDay);
    Serial.println();
}
volatile char flag = 'x';

static volatile uint32_t isr_count = 0;
static volatile int64_t last_isr_time = 0;

static void IRAM_ATTR button_isr(void *arg)
{
    int64_t now = esp_timer_get_time(); // µs
    if (now - last_isr_time > 30000) {  // 30 ms debounce
        last_isr_time = now;
        isr_count++;
    }
}
void IRAM_ATTR handleButtonUp() { flag=('U'); }
void IRAM_ATTR handleButtonDown(void* arg) { flag=('D'); }
void IRAM_ATTR handleButtonLeft() { flag=('L'); }
void IRAM_ATTR handleButtonRight() { flag=('R'); }
void IRAM_ATTR handleButtonEnter() { flag=('E'); }
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
void setup() {
   //


   // pinMode(41, OUTPUT);
    //pinMode(21, OUTPUT_OPEN_DRAIN);
    //digitalWrite(21,0); //hold true
    //gpio_set_level(GPIO_NUM_41, 0);
    //gpio_set_level(GPIO_NUM_21, 0); //HOLD

    gpio_set_direction(GPIO_NUM_40,GPIO_MODE_INPUT); //drain cap
    gpio_pulldown_en(GPIO_NUM_40);
    //digitalWrite(41, 0); //done
    Serial.begin(115200);

    Serial.println("Woken up!");

    Serial.print("Pin 40: ");
    Serial.println(x);
    Serial.println(x);

    pinMode(BTN_UP,     INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);




    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    gpio_set_level(GPIO_NUM_41,1); // DONE

    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    //printLocalTime();
    struct tm timeinfo;
    while(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
delay(100);
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

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

    }fram.exitSleep();
    fram.writeEnable(1);
    uint32_t addr = 12;
    uint8_t valtowrite = 12;
    fram.write8(addr,valtowrite);
    uint8_t val ;
    val = fram.read8(addr);
fram.enterSleep();

    // measure bat voltage
    //analogReadResolution(12);
    //analogSetAttenuation(ADC_11db);

    //adc1_config_width(ADC_WIDTH_BIT_12);
    //adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_12);
    //adc1_get_raw(ADC1_CHANNEL_0);
    pinMode(2,OUTPUT_OPEN_DRAIN);
    adc_init();

    digitalWrite(2,0);
    delay(200);
    //uint16_t raw = analogRead(1);
    //float v_adc = (raw) * (3.9 / 4095.0);
    //float v_batt = v_adc * 2.0; // 500k/500k divider
    float v_batt = battery_voltage();

    digitalWrite(2,1);

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



    //delay(5000);
    Serial.println("Buhbye!");
    gpio_set_level(GPIO_NUM_21,1); //stop hold
   // attachInterrupt(BTN_UP, handleButtonUp, FALLING);
    //attachInterrupt(BTN_DOWN, handleButtonDown, FALLING);
    //attachInterrupt(BTN_LEFT, handleButtonLeft, FALLING);
    //attachInterrupt(BTN_RIGHT, handleButtonRight, FALLING);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_NUM_36,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,   // FALLING
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(GPIO_NUM_36, button_isr, NULL);

    //attachInterrupt(BTN_ENTER, handleButtonEnter, FALLING);
    return;
    pinMode(35, OUTPUT);
    pinMode(38,  OUTPUT);
    digitalWrite(38,0);
    pinMode(16, OUTPUT);
    pinMode(17, OUTPUT);
   // while (!Serial)
       // delay(10);     // will pause Zero, Leonardo, etc until serial console opens

    Serial.println("Adafruit SHT4x test");
    if (! sht4.begin()) {
        Serial.println("Couldn't find SHT4x");
       // while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");
    Serial.print("Serial number 0x");
    Serial.println(sht4.readSerial(), HEX);

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    switch (sht4.getPrecision()) {
    case SHT4X_HIGH_PRECISION:
        Serial.println("High precision");
        break;
    case SHT4X_MED_PRECISION:
        Serial.println("Med precision");
        break;
    case SHT4X_LOW_PRECISION:
        Serial.println("Low precision");
        break;
    }

    Serial.println("Initialising wifi...");

    WiFi.begin(ssid,password);
    Serial.println("Connecting...");

    while (WiFi.status()!=WL_CONNECTED)
    {
        if (WiFi.status()==WL_CONNECT_FAILED)
        {
            Serial.println("Failed to connect!");
            while (true) {};
        }
    }
    Serial.println("Connected!");
    if (psramFound()) {
        Serial.println("PSRAM is enabled!");
        Serial.printf("Total PSRAM: %lu bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM:  %lu bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("PSRAM not available!");
    }
//https://
    Getter getter1;
    getter1.connect("api.open-meteo.com");
    JsonDocument JSON = getter1.get("/v1/forecast?latitude=50.348&longitude=18.9328&daily=weather_code,temperature_2m_min,temperature_2m_max,apparent_temperature_min,apparent_temperature_max,sunset,sunrise,uv_index_max,rain_sum,showers_sum,snowfall_sum,precipitation_sum,precipitation_hours,precipitation_probability_max,wind_speed_10m_max&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation_probability,precipitation,weather_code,surface_pressure,wind_speed_10m,wind_gusts_10m,visibility,rain,showers,snowfall,snow_depth,cloud_cover,is_day&current=weather_code,temperature_2m,is_day,precipitation,wind_speed_10m,wind_direction_10m&forecast_days=14&timezone=Europe/Warsaw");
    String a;
    serializeJson(JSON, a);

    Serial.println(a);

    //display.init(115200, true, 2, false);
    //display.setRotation(0); // optional, 0-3 depending on orientation

    /*display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 0);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(2);
    display.println("turning off now");
    display.display();
    display.hibernate();
*/
}

void loop() {
// write your code here
    //sensors_event_t humidity, temp;

    /*uint32_t timestamp = millis();
    sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    timestamp = millis() - timestamp;
    Serial.println(PIN_SDA)
    Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
    Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

    Serial.print("Read duration (ms): ");
    Serial.println(timestamp);
*/
    static uint32_t last_count = 0;
    if (isr_count != last_count) {
        Serial.printf("Button press count = %lu", isr_count);
        last_count = isr_count;
    }
    vTaskDelay(pdMS_TO_TICKS(10));   // delay(1000);
    //esp_deep_sleep_start();

}
