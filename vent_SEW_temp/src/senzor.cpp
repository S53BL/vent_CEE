// senzor.cpp
#define ENABLE_WEB_SERVER

#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>
#include <TouchDrvCSTXXX.hpp>
#include <TFT_eSPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_SHT4x.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <vector>
#include "wifi_config.h" // Vključitev zunanje datoteke za Wi-Fi podatke
#ifdef ENABLE_WEB_SERVER
#include "web.h" // Vključitev spletnega strežnika
#endif

#define DEBUG_MODE true

// Pin definicije
#define PIN_LCD_BL                   38
#define PIN_LCD_D0                   39
#define PIN_LCD_D1                   40
#define PIN_LCD_D2                   41
#define PIN_LCD_D3                   42
#define PIN_LCD_D4                   45
#define PIN_LCD_D5                   46
#define PIN_LCD_D6                   47
#define PIN_LCD_D7                   48
#define PIN_POWER_ON                 15
#define PIN_LCD_RES                  5
#define PIN_LCD_CS                   6
#define PIN_LCD_DC                   7
#define PIN_LCD_WR                   8
#define PIN_LCD_RD                   9
#define BOARD_I2C_SDA                18
#define BOARD_I2C_SCL                17
#define BOARD_TOUCH_IRQ              16
#define BOARD_TOUCH_RST              21
#define PIN_LED                      44
#define BTN1_PIN                     0
#define BTN2_PIN                     14
#define MOTION_SENSOR_PIN            43

// Dimenzije zaslona
const uint16_t SCREEN_WIDTH = 170;
const uint16_t SCREEN_HEIGHT = 320;
const uint16_t TOP_SECTION_HEIGHT = 60;
const uint16_t MIDDLE_SECTION_HEIGHT = 160;
const uint16_t BOTTOM_SECTION_HEIGHT = 100;

// Časovni intervali
const unsigned long TOUCH_READ_INTERVAL = 250;
const unsigned long BUTTON_LONG_PRESS = 1000;
const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long TOP_UPDATE_INTERVAL = 60000;
const unsigned long MIDDLE_UPDATE_INTERVAL = 60000;
const unsigned long GRAPH_UPDATE_INTERVAL = 60000;
const unsigned long WIFI_TIMEOUT = 10000;
const unsigned long SENSOR_READ_INTERVAL = 30000;
const unsigned long DATA_SAVE_INTERVAL = 180000; // iz 30000 na 180000 ms (3 minute)
const unsigned long SERVICE_MIDDLE_UPDATE_INTERVAL = 60000;
const unsigned long SERVIS_TIMEOUT = 120000;
const unsigned long MOTION_TIMEOUT = 60000; // Skrajšano na 1 minuto
const int MAX_GRAPH_HOURS = 24;
const int MIN_GRAPH_HOURS = 1;
const int DATA_POINTS = MAX_GRAPH_HOURS * 65;
const int WIFI_MAX_ATTEMPTS = 3;
const int EEPROM_ADDR_HOURS = 0;
const int MAX_ENTRIES = 2880;

// Barve
#define TFT_GREEN     0x07E0
#define TFT_YELLOW    0xFFE0
#define MY_LIGHTGREEN tft.color565(144, 238, 144)
#define MY_DARKGREEN  tft.color565(0, 100, 0)
#define MY_DARKRED    tft.color565(139, 0, 0)
#define TFT_GRAY      tft.color565(128, 128, 128)
#define TFT_BLUE      0x001F
#define TFT_RED       0xF800
#define TFT_WHITE     0xFFFF

// Konstante za LED in nočni čas
const uint16_t NIGHT_LUX_THRESHOLD = 50; // Nastavljiv prag za noč
const unsigned long LED_BLINK_INTERVAL = 60000; // 1 minuta za omejitev utripov ob gibanju
const unsigned long LED_HOUR_INTERVAL = 3600000; // 1 ura za PWM prehod

// Struktura za podatke senzorjev
struct SensorData {
    float temp;
    float humidity;
    float pressure;
    float voc;
    uint16_t cct;
    uint16_t lux;
    bool valid;
    unsigned long timestamp;
    uint8_t motion;
};

// Globalne spremenljivke
float tempSHTSum = 0, tempBMESum = 0, humSHTSum = 0, humBMESum = 0, presSum = 0, vocSum = 0, cctSum = 0, luxSum = 0;
int measurementCount = 0;
float lastPressure = 0, lastVOC = 0, lastTempBME = 0, lastHumBME = 0;
float lastTempSHT = 0, lastHumSHT = 0;
uint16_t lastCCT = 0, lastLux = 0;
uint16_t lastR = 0, lastG = 0, lastB = 0;
bool sendingHTTP = false;
int httpStatus = 0;
unsigned long httpStatusTime = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // Zamik nastavljen na 0, časovni pas upravlja setenv

TFT_eSPI tft = TFT_eSPI();
TouchDrvCSTXXX touch;
Adafruit_BME680 bme;
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_60X);
Adafruit_SHT4x sht41 = Adafruit_SHT4x();
int16_t x[5], y[5];
uint8_t bme680Address = 0;
bool ledState = false;
bool mainScreen = true;
bool sht41Present = false;
bool bmePresent = false;
bool tcsPresent = false;
bool touchPresent = false;
bool motionDetected = false;
bool screenOn = true;
unsigned long lastTouchTime = 0, lastTouchReadTime = 0, lastTopUpdateTime = 0;
unsigned long lastMiddleUpdateTime = 0, lastGraphUpdateTime = 0, lastSensorReadTime = 0;
unsigned long lastDataSaveTime = 0, lastBtn1Press = 0, lastBtn2Press = 0;
unsigned long lastServiceMiddleUpdateTime = 0, lastWiFiCheckTime = 0, lastInteractionTime = 0;
unsigned long lastMotionTime = 0, lastLedBlinkTime = 0, lastLedHourTime = 0; // Dodani časovniki za LED
bool btn1Pressed = false, btn2Pressed = false;
int currentNetworkIndex = 0, wifiAttempts = 0, currentGraphSensor = 0, dataIndex = 0;
int graphHours = 2, lastGraphHours = -1, statusY = 0, httpSuccessCount = 0, httpAttemptCount = 0;
unsigned long lastGpio14Check = 0, bothPressedTime = 0, lastMillisCheck = 0;

const char* SERVER_URL = "http://192.168.2.190/data";
#define HISTORY_FILE "/history.bin"

// Deklaracije funkcij
void preglejI2C();
void narisiZgornjiDel();
void narisiSrednjiDel(bool initial = false);
void narisiSpodnjiDel(bool initial = false, bool forceUpdate = false);
void prikaziGlavniZaslon();
void narisiServisniZgornjiDel();
void narisiServisniSrednjiDel(bool initial = false);
void prikaziServisniZaslon();
String formatirajDatum(unsigned long epochTime);
void connectWiFi();
void readSensors(bool force = false);
void saveSensorData();
void sendData();
void calculateCCTandLux(uint16_t r, uint16_t g, uint16_t b, uint16_t c, uint16_t &cct, uint16_t &lux);
void loadGraphHoursFromEEPROM();
void saveGraphHoursToEEPROM();
void printStatus(const String& message);
bool checkI2CDevice(uint8_t address);
String replaceSpecialChars(String input);
float roundToNearestStep(float value, float step, bool roundDown);
void printMinMaxLabel(float value, int x, int y);
unsigned long getCurrentTime(); // Dodana za NTP sinhronizacijo

// Inicializacija
void setup() {
    tft.fillScreen(TFT_BLACK);
    Serial.begin(115200);
    printStatus("Zacetek nastavitve");

    if (!LittleFS.begin(true)) {
        printStatus("Napaka pri inicializaciji LittleFS");
        while (true) delay(1000);
    }
    printStatus("LittleFS inicializiran");

    // Ohrani obstoječo datoteko zgodovine
    if (!LittleFS.exists(HISTORY_FILE)) {
        fs::File file = LittleFS.open(HISTORY_FILE, FILE_WRITE);
        if (file) {
            file.close();
            printStatus("Ustvarjena nova datoteka zgodovine");
        } else {
            printStatus("Napaka pri ustvarjanju datoteke");
        }
    } else {
        printStatus("Datoteka zgodovine ze obstaja, podatki ohranjeni");
    }

    EEPROM.begin(4);
    loadGraphHoursFromEEPROM();
    printStatus("Nalozene nastavitve iz EEPROM");

    tft.init();
    tft.setRotation(0);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    pinMode(PIN_LCD_BL, OUTPUT);
    ledcSetup(0, 5000, 8); // PWM kanal 0, 5 kHz, 8-bitna resolucija
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 255); // Zaslon vklopljen ob zagonu
    pinMode(PIN_POWER_ON, OUTPUT); // Dodano: Nastavi PIN_POWER_ON (GPIO15) kot izhod
    digitalWrite(PIN_POWER_ON, HIGH); // Dodano: Nastavi PIN_POWER_ON na HIGH za vklop napajanja
    printStatus("Inicializacija zaslona TFT_eSPI");

    delay(500);
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Wire.setClock(50000);
    delay(500);
    printStatus("Inicializacija I2C na pinih SDA: 18, SCL: 17");

    touchPresent = false;
    if (checkI2CDevice(CST816_SLAVE_ADDRESS)) {
        touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_IRQ);
        if (touch.begin(Wire, CST816_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL)) {
            touchPresent = true;
            printStatus("Inicializacija dotika uspesna");
        } else {
            printStatus("Napaka pri inicializaciji CST816");
        }
    } else {
        printStatus("Dotik (CST816) ni dosegljiv na 0x" + String(CST816_SLAVE_ADDRESS, HEX));
    }

    bmePresent = false;
    for (uint8_t addr = 0x76; addr <= 0x77; addr++) {
        if (checkI2CDevice(addr)) {
            bme680Address = addr;
            if (bme.begin(bme680Address)) {
                bme.setTemperatureOversampling(BME680_OS_8X);
                bme.setHumidityOversampling(BME680_OS_2X);
                bme.setPressureOversampling(BME680_OS_4X);
                bme.setIIRFilterSize(BME680_FILTER_SIZE_1);
                bme.setGasHeater(350, 200);
                bmePresent = true;
                printStatus("Inicializacija BME680 uspesna na 0x" + String(bme680Address, HEX));
                break;
            }
        }
    }
    if (!bmePresent) printStatus("BME680 ni dosegljiv");

    tcsPresent = false;
    if (checkI2CDevice(0x29)) {
        if (tcs.begin()) {
            tcsPresent = true;
            printStatus("Inicializacija TCS34725 uspesna");
        } else {
            printStatus("Ni mogoce inicializirati TCS34725");
        }
    } else {
        printStatus("TCS34725 ni dosegljiv na 0x29");
    }

    sht41Present = false;
    if (checkI2CDevice(0x44)) {
        if (sht41.begin()) {
            sht41Present = true;
            printStatus("Inicializacija SHT41 uspesna");
        } else {
            printStatus("Ni mogoce inicializirati SHT41");
        }
    } else {
        printStatus("SHT41 ni dosegljiv na 0x44");
    }

    preglejI2C();
    printStatus("Pregled I2C naprav zakljucen");

    printStatus("Povezovanje na Wi-Fi...");
    connectWiFi();
    lastWiFiCheckTime = millis();

    #ifdef ENABLE_WEB_SERVER
    setupWebServer();
    if (DEBUG_MODE) Serial.println("Spletni strežnik zagnan na IP: " + WiFi.localIP().toString());
    #endif

    // Nastavitev časovnega pasu za Slovenijo (CET-1CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); // CET-1 (UTC+1), CEST od zadnje nedelje v marcu ob 2:00, nazaj na CET zadnjo nedeljo v oktobru ob 3:00
    tzset(); // Uveljavi nastavitev časovnega pasu

    timeClient.begin();
    printStatus("Zacetek NTP klienta");
    if (!timeClient.update()) {
        printStatus("Sinhronizacija NTP ni uspela");
    } else {
        printStatus("Sinhronizacija NTP uspesna");
    }

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    ledcSetup(1, 5000, 8); // PWM kanal 1 za LED (pin 44)
    ledcAttachPin(PIN_LED, 1);
    printStatus("LED inicializirana");

    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(MOTION_SENSOR_PIN, INPUT);
    printStatus("Gumba in senzor gibanja inicializirana");

    readSensors(true);
    saveSensorData();

    mainScreen = true;
    lastMotionTime = millis(); // Zaslon ostane vklopljen 1 minuto po zagonu
    narisiZgornjiDel();
    narisiSrednjiDel(true);
    narisiSpodnjiDel(true);
    statusY = 0;
    lastInteractionTime = millis();
}

// --- Konec prvega dela ---
// --- Začetek drugega dela: Pomožne funkcije ---

String replaceSpecialChars(String input) {
    input.replace("Č", "C");
    input.replace("Š", "S");
    input.replace("Ž", "Z");
    input.replace("č", "c");
    input.replace("š", "s");
    input.replace("ž", "z");
    return input;
}

void printStatus(const String& message) {
    String correctedMessage = replaceSpecialChars(message);
    tft.fillRect(0, statusY, SCREEN_WIDTH, 15, TFT_BLACK);
    tft.setCursor(0, statusY);
    tft.println(correctedMessage);
    if (DEBUG_MODE) Serial.println(correctedMessage);
    statusY += 15;
    if (statusY >= SCREEN_HEIGHT) {
        statusY = 0;
        tft.fillScreen(TFT_BLACK);
    }
}

bool checkI2CDevice(uint8_t address) {
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    return (error == 0);
}

void loadGraphHoursFromEEPROM() {
    graphHours = EEPROM.read(EEPROM_ADDR_HOURS);
    if (graphHours < MIN_GRAPH_HOURS || graphHours > MAX_GRAPH_HOURS || graphHours == 0xFF) {
        graphHours = 2;
        saveGraphHoursToEEPROM();
    }
    lastGraphHours = graphHours;
}

void saveGraphHoursToEEPROM() {
    if (graphHours != lastGraphHours) {
        EEPROM.write(EEPROM_ADDR_HOURS, graphHours);
        EEPROM.commit();
        lastGraphHours = graphHours;
    }
}

void connectWiFi() {
    WiFi.disconnect();
    for (int i = 0; i < numNetworks; i++) {
        if (!WiFi.config(localIP, gateway, subnet, dns)) {
            if (DEBUG_MODE) Serial.println("Napaka pri nastavitvi statičnega IP-ja");
        }
        WiFi.begin(ssidList[currentNetworkIndex], passwordList[currentNetworkIndex]);
        unsigned long startTime = millis();
        while (millis() - startTime < WIFI_TIMEOUT && WiFi.status() != WL_CONNECTED) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            if (DEBUG_MODE) Serial.println("Povezan na Wi-Fi: " + String(ssidList[currentNetworkIndex]) + " z IP: " + WiFi.localIP().toString());
            return;
        }
        currentNetworkIndex = (currentNetworkIndex + 1) % numNetworks;
    }
    if (DEBUG_MODE) Serial.println("Povezava na Wi-Fi ni uspela");
}

void preglejI2C() {
    bool found = false;
    for (uint8_t naslov = 1; naslov < 127; naslov++) {
        Wire.beginTransmission(naslov);
        if (Wire.endTransmission() == 0) {
            found = true;
            if (DEBUG_MODE) Serial.println("Najdena naprava na 0x" + String(naslov, HEX));
            if (naslov == 0x76 || naslov == 0x77) bme680Address = naslov;
        }
    }
    if (!found && DEBUG_MODE) Serial.println("Ni najdenih naprav");
}

String formatirajDatum(unsigned long epochTime) {
    time_t surovCas = epochTime;
    struct tm * infoCas = localtime(&surovCas);
    char pufer[11];
    strftime(pufer, sizeof(pufer), "%d.%m.%Y", infoCas);
    return String(pufer);
}

void readSensors(bool force) {
    if (force || (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL)) {
        bool validReading = false;

        if (bmePresent && bme.performReading()) {
            lastPressure = bme.pressure / 100.0;
            lastVOC = bme.gas_resistance / 1000.0;
            lastTempBME = bme.temperature;
            lastHumBME = bme.humidity;
            presSum += lastPressure;
            vocSum += lastVOC;
            tempBMESum += lastTempBME;
            humBMESum += lastHumBME;
            validReading = true;
            if (DEBUG_MODE) Serial.println("Tlak: " + String(lastPressure) + " hPa, VOC: " + String(lastVOC) + " kOhm");
        }

        if (sht41Present) {
            sensors_event_t hum, tmp;
            if (sht41.getEvent(&hum, &tmp)) {
                lastTempSHT = tmp.temperature;
                lastHumSHT = hum.relative_humidity;
                tempSHTSum += lastTempSHT;
                humSHTSum += lastHumSHT;
                validReading = true;
            }
        }

        if (tcsPresent) {
            ledcWrite(1, 0); // Ugasnemo LED pred meritvijo svetlobe
            uint16_t r, g, b, c;
            tcs.getRawData(&r, &g, &b, &c);
            calculateCCTandLux(r, g, b, c, lastCCT, lastLux);
            lastR = r; lastG = g; lastB = b;
            cctSum += lastCCT;
            luxSum += lastLux;
            validReading = true;
            if (DEBUG_MODE) Serial.println("Lux: " + String(lastLux) + ", CCT: " + String(lastCCT));
        }

        if (validReading) measurementCount++;
        lastSensorReadTime = millis();
    }
}

void saveSensorData() {
    if (millis() - lastDataSaveTime >= DATA_SAVE_INTERVAL && measurementCount > 0) {
        float tempToSave = sht41Present ? (tempSHTSum / measurementCount) : (tempBMESum / measurementCount);
        float humidityToSave = sht41Present ? (humSHTSum / measurementCount) : (humBMESum / measurementCount);
        
        SensorData data = {
            tempToSave,
            humidityToSave,
            bmePresent ? presSum / measurementCount : 0,
            bmePresent ? vocSum / measurementCount : 0,
            (uint16_t)(cctSum / measurementCount),
            (uint16_t)(luxSum / measurementCount),
            true,
            timeClient.getEpochTime(),
            motionDetected ? (uint8_t)1 : (uint8_t)0
        };

        if (DEBUG_MODE) {
            Serial.println("Shranjujem podatke: temp=" + String(data.temp) + 
                           ", humidity=" + String(data.humidity) + 
                           ", pressure=" + String(data.pressure) + 
                           ", voc=" + String(data.voc) + 
                           ", cct=" + String(data.cct) + 
                           ", lux=" + String(data.lux) + 
                           ", motion=" + String(data.motion) + 
                           ", timestamp=" + String(data.timestamp));
        }

        fs::File file = LittleFS.open(HISTORY_FILE, FILE_READ);
        if (file) {
            int totalEntries = file.size() / sizeof(SensorData);
            SensorData tempData;
            std::vector<SensorData> validData;
            unsigned long currentTime = timeClient.getEpochTime();
            while (file.available()) {
                file.read((uint8_t*)&tempData, sizeof(SensorData));
                if (currentTime - tempData.timestamp < 86400) {
                    validData.push_back(tempData);
                }
            }
            file.close();

            file = LittleFS.open(HISTORY_FILE, FILE_WRITE);
            if (file) {
                for (const auto& entry : validData) {
                    file.write((uint8_t*)&entry, sizeof(SensorData));
                }
                file.write((uint8_t*)&data, sizeof(SensorData));
                file.close();
            }
        } else {
            file = LittleFS.open(HISTORY_FILE, FILE_APPEND);
            if (file) {
                file.write((uint8_t*)&data, sizeof(SensorData));
                file.close();
            }
        }

        dataIndex = (dataIndex + 1) % DATA_POINTS;
        tempSHTSum = tempBMESum = humSHTSum = humBMESum = presSum = vocSum = cctSum = luxSum = 0;
        measurementCount = 0;
        motionDetected = false;
        sendData();
        lastDataSaveTime = millis();
    }
}

void sendData() {
    if (WiFi.status() == WL_CONNECTED) {
        fs::File file = LittleFS.open(HISTORY_FILE, FILE_READ);
        if (file) {
            int totalEntries = file.size() / sizeof(SensorData);
            SensorData lastData;
            file.seek((totalEntries - 1) * sizeof(SensorData));
            file.read((uint8_t*)&lastData, sizeof(SensorData));
            file.close();

            sendingHTTP = true;
            narisiZgornjiDel();
            delay(50);
            HTTPClient http;
            http.begin(SERVER_URL);
            http.addHeader("Content-Type", "application/json");
            String json = "{\"temp\":" + String(lastData.temp) +
                          ",\"humidity\":" + String(lastData.humidity) +
                          ",\"pressure\":" + String(lastData.pressure) +
                          ",\"voc\":" + String(lastData.voc) + "}";
            int httpCode = http.POST(json);
            httpAttemptCount++;
            if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
                httpSuccessCount++;
                httpStatus = 1;
            } else {
                httpStatus = -1;
            }
            http.end();
            if (httpAttemptCount >= 10) {
                httpAttemptCount = 0;
                httpSuccessCount = 0;
            }
            sendingHTTP = false;
            httpStatusTime = millis();
            narisiZgornjiDel();
        }
    }
}

void calculateCCTandLux(uint16_t r, uint16_t g, uint16_t b, uint16_t c, uint16_t &cct, uint16_t &lux) {
    if (r == 0 && g == 0 && b == 0 && c == 0) {
        cct = 0;
        lux = 0;
        return;
    }

    lux = (-0.32466 * r) + (1.57837 * g) + (-0.73191 * b);
    if (lux < 0) lux = 0;

    float X = (-0.14282 * r) + (1.54924 * g) + (-0.95641 * b);
    float Y = (-0.32466 * r) + (1.57837 * g) + (-0.73191 * b);
    float Z = (-0.68202 * r) + (0.77073 * g) + (0.56332 * b);
    if (X + Y + Z == 0) {
        cct = 0;
        return;
    }
    float x = X / (X + Y + Z);
    float y = Y / (X + Y + Z);
    float n = (x - 0.3320) / (0.1858 - y);
    cct = 449 * n * n * n + 3525 * n * n + 6823.3 * n + 5520.33;
    if (cct < 1000) cct = 1000;
    if (cct > 10000) cct = 10000;
}

float roundToNearestStep(float value, float step, bool roundDown) {
    if (roundDown) return floor(value / step) * step;
    return ceil(value / step) * step;
}

unsigned long getCurrentTime() {
    return timeClient.getEpochTime();
}

// --- Konec drugega dela ---
// --- Začetek tretjega dela: Funkcije zaslona (prva polovica) ---

void narisiZgornjiDel() {
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&rawTime);
    char timeBuffer[9];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", timeInfo);
    String formatiranCas = String(timeBuffer);
    String formatiranDatum = formatirajDatum(timeClient.getEpochTime());
    bool motion = digitalRead(MOTION_SENSOR_PIN) == HIGH;

    static String zadnjiFormatiranCas = "";
    static String zadnjiFormatiranDatum = "";
    static bool zadnjeMotion = false;
    if (formatiranCas != zadnjiFormatiranCas || formatiranDatum != zadnjiFormatiranDatum || motion != zadnjeMotion) {
        zadnjiFormatiranCas = formatiranCas;
        zadnjiFormatiranDatum = formatiranDatum;
        zadnjeMotion = motion;
        tft.fillRect(0, 0, SCREEN_WIDTH, TOP_SECTION_HEIGHT, TFT_BLACK);
        tft.setTextFont(4);
        tft.setTextColor(motion ? TFT_RED : MY_LIGHTGREEN, TFT_BLACK);
        tft.setCursor(10, 20);
        tft.println(formatiranCas);
        tft.setCursor(10, 40);
        tft.println(formatiranDatum);

        int rssi = WiFi.RSSI();
        uint8_t signalStrength = 3;
        if (rssi < -80) signalStrength = 1;
        else if (rssi < -67) signalStrength = 2;

        uint16_t wifiColor;
        if (sendingHTTP) {
            wifiColor = TFT_BLUE;
        } else if (httpStatus != 0 && millis() - httpStatusTime < 1000) {
            wifiColor = (httpStatus == 1) ? TFT_GREEN : TFT_RED;
        } else {
            wifiColor = (WiFi.status() == WL_CONNECTED) ? TFT_WHITE : TFT_GRAY;
        }

        if (WiFi.status() == WL_CONNECTED) {
            tft.fillCircle(150, 20, 3, signalStrength >= 1 ? wifiColor : TFT_GRAY);
            tft.drawCircle(150, 20, 6, signalStrength >= 2 ? wifiColor : TFT_GRAY);
            tft.drawCircle(150, 20, 10, signalStrength == 3 ? wifiColor : TFT_GRAY);
        } else {
            tft.fillCircle(150, 20, 3, TFT_GRAY);
            tft.drawCircle(150, 20, 6, TFT_GRAY);
            tft.drawCircle(150, 20, 10, TFT_GRAY);
            tft.drawLine(140, 10, 160, 30, TFT_RED);
            tft.drawLine(141, 10, 161, 30, TFT_RED);
            tft.drawLine(139, 10, 159, 30, TFT_RED);
            tft.drawLine(140, 30, 160, 10, TFT_RED);
            tft.drawLine(141, 30, 161, 10, TFT_RED);
            tft.drawLine(139, 30, 159, 10, TFT_RED);
        }
        lastTopUpdateTime = millis();
    }
}
void narisiSrednjiDel(bool initial) {
    static float zadnjaTempSHT = 0, zadnjaTempBME = 0, zadnjaVlagaSHT = 0, zadnjaVlagaBME = 0;
    static float zadnjiTlak = 0, zadnjiVOC = 0;
    static uint16_t zadnjiCCT = 0, zadnjiLux = 0;
    static uint16_t zadnjiR = 0, zadnjiG = 0, zadnjiB = 0;
    static int zadnjeMotionCount = -1;
    static String zadnjiMotionTime = "";

    int motionCount = 0;
    String predzadnjiMotionTime = "";
    fs::File file = LittleFS.open(HISTORY_FILE, FILE_READ);
    if (file) {
        int totalEntries = file.size() / sizeof(SensorData);
        unsigned long currentTime = timeClient.getEpochTime();
        unsigned long lastMotionTimestamp = 0;
        unsigned long predzadnjiMotionTimestamp = 0;
        SensorData tempData;
        for (int i = 0; i < totalEntries; i++) {
            file.seek(i * sizeof(SensorData));
            file.read((uint8_t*)&tempData, sizeof(SensorData));
            if (tempData.valid && (currentTime - tempData.timestamp) < 86400 && tempData.motion == 1) {
                motionCount++;
                predzadnjiMotionTimestamp = lastMotionTimestamp;
                lastMotionTimestamp = tempData.timestamp;
            }
        }
        file.close();
        if (predzadnjiMotionTimestamp > 0) {
            time_t ts = predzadnjiMotionTimestamp;
            struct tm* timeinfo = localtime(&ts);
            char timeStr[6];
            strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
            predzadnjiMotionTime = String(timeStr);
        } else {
            predzadnjiMotionTime = "N/A";
        }
    }

    bool changed = (lastTempSHT != zadnjaTempSHT || lastTempBME != zadnjaTempBME || lastHumSHT != zadnjaVlagaSHT || 
                    lastHumBME != zadnjaVlagaBME || lastPressure != zadnjiTlak || lastVOC != zadnjiVOC ||
                    lastCCT != zadnjiCCT || lastLux != zadnjiLux || lastR != zadnjiR || lastG != zadnjiG || 
                    lastB != zadnjiB || motionCount != zadnjeMotionCount || predzadnjiMotionTime != zadnjiMotionTime);

    if (initial || changed) {
        zadnjaTempSHT = lastTempSHT; zadnjaTempBME = lastTempBME;
        zadnjaVlagaSHT = lastHumSHT; zadnjaVlagaBME = lastHumBME;
        zadnjiTlak = lastPressure; zadnjiVOC = lastVOC;
        zadnjiCCT = lastCCT; zadnjiLux = lastLux;
        zadnjiR = lastR; zadnjiG = lastG; zadnjiB = lastB;
        zadnjeMotionCount = motionCount; zadnjiMotionTime = predzadnjiMotionTime;

        tft.fillRect(0, TOP_SECTION_HEIGHT, SCREEN_WIDTH, MIDDLE_SECTION_HEIGHT, TFT_BLACK);
        tft.setTextFont(2);
        int yPoz = TOP_SECTION_HEIGHT + 5;
        int leftOffset = 5;
        int valueOffsetSHT = 50;
        int valueOffsetBME = 100;

        tft.setTextColor(currentGraphSensor == 0 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("Temp:");
        if (sht41Present) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%.1f C", lastTempSHT);
        }
        if (bmePresent) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.setCursor(valueOffsetBME, yPoz); tft.printf("%.1f C", lastTempBME);
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 1 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("Vlaga:");
        if (sht41Present) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%.1f %%", lastHumSHT);
        }
        if (bmePresent) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.setCursor(valueOffsetBME, yPoz); tft.printf("%.1f %%", lastHumBME);
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 2 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("Tlak:");
        if (bmePresent) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%.1f hPa", lastPressure);
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.print("0.0 hPa");
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 3 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("VOC:");
        if (bmePresent) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%.1f kOhm", lastVOC);
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.print("0.0 kOhm");
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 4 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("Lux:");
        if (tcsPresent) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%d lx", lastLux);
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.print("0 lx");
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 5 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("CCT:");
        if (tcsPresent) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.printf("%d K", lastCCT);
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(valueOffsetSHT, yPoz); tft.print("0 K");
        }
        yPoz += 18;

        tft.setTextColor(currentGraphSensor == 6 ? TFT_RED : TFT_WHITE, TFT_BLACK);
        tft.setCursor(leftOffset, yPoz); tft.print("Gib: ");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(leftOffset + 30, yPoz); 
        tft.print(String(motionCount) + " (" + predzadnjiMotionTime + ")");

        yPoz += 18;

        if (tcsPresent && lastR + lastG + lastB > 0) {
            int redWidth = (lastR * 150) / (lastR + lastG + lastB);
            int greenWidth = (lastG * 150) / (lastR + lastG + lastB);
            int blueWidth = 150 - redWidth - greenWidth;
            tft.fillRect(leftOffset, yPoz + 5, redWidth, 3, TFT_RED);
            tft.fillRect(leftOffset + redWidth, yPoz + 5, greenWidth, 3, TFT_GREEN);
            tft.fillRect(leftOffset + redWidth + greenWidth, yPoz + 5, blueWidth, 3, TFT_BLUE);
        }

        lastMiddleUpdateTime = millis();
    }
}

// --- Konec tretjega dela ---
// --- Začetek četrtega dela: Funkcije zaslona (druga polovica) in loop ---

void narisiSpodnjiDel(bool initial, bool forceUpdate) {
    int graphX = 20;
    int graphY = TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + BOTTOM_SECTION_HEIGHT - 10;
    int graphHeight = BOTTOM_SECTION_HEIGHT - 20;
    int graphWidth = SCREEN_WIDTH - 40;

    fs::File file = LittleFS.open(HISTORY_FILE, FILE_READ);
    if (!file) {
        if (initial || forceUpdate) {
            tft.fillRect(0, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT, SCREEN_WIDTH, BOTTOM_SECTION_HEIGHT, TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(graphX, graphY - graphHeight / 2);
            tft.println("LittleFS Error");
            lastGraphUpdateTime = millis();
        }
        return;
    }

    int totalEntries = file.size() / sizeof(SensorData);
    static int lastTotalEntries = -1;

    if (initial || forceUpdate || totalEntries != lastTotalEntries) {
        lastTotalEntries = totalEntries;
        tft.fillRect(0, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT, SCREEN_WIDTH, BOTTOM_SECTION_HEIGHT, TFT_BLACK);

        tft.drawLine(graphX, graphY, graphX + graphWidth, graphY, TFT_GREEN);
        tft.drawLine(graphX, graphY, graphX, graphY - graphHeight, TFT_GREEN);

        unsigned long currentTime = timeClient.getEpochTime();
        unsigned long startTime = currentTime - (graphHours * 3600UL);

        std::vector<SensorData> validData;
        file.seek(0);
        while (file.available()) {
            SensorData tempData;
            file.read((uint8_t*)&tempData, sizeof(SensorData));
            if (tempData.valid && tempData.timestamp >= startTime && tempData.timestamp <= currentTime) {
                validData.push_back(tempData);
            }
        }
        file.close();

        int numPoints = validData.size();
        if (numPoints < 2) {
            lastGraphUpdateTime = millis();
            return;
        }

        unsigned long minTimestamp = validData[0].timestamp;
        unsigned long maxTimestamp = validData[numPoints - 1].timestamp;
        float dataTimeRange = (maxTimestamp - minTimestamp) / 3600.0;
        float dataWidth = (dataTimeRange / graphHours) * graphWidth;
        float xStart = graphX + ((minTimestamp - startTime) / (graphHours * 3600.0)) * graphWidth;
        float xEnd = xStart + dataWidth;

        if (xStart < graphX) xStart = graphX;
        if (xEnd > graphX + graphWidth) xEnd = graphX + graphWidth;

        if (currentGraphSensor == 6) { // Graf za gibanje
            int totalHours = graphHours;
            float motionPerHour[totalHours] = {0};
            for (int i = 0; i < numPoints; i++) {
                if (validData[i].motion == 1) {
                    int hourIndex = (validData[i].timestamp - startTime) / 3600;
                    if (hourIndex >= 0 && hourIndex < totalHours) {
                        motionPerHour[hourIndex]++;
                    }
                }
            }

            float pixelsPerHour = (float)graphWidth / totalHours;
            for (int i = 0; i < totalHours; i++) {
                float motionCount = motionPerHour[i];
                int barHeight = (motionCount <= 10) ? (motionCount / 10.0 * graphHeight) : graphHeight;
                int xPos = graphX + i * pixelsPerHour;
                tft.fillRect(xPos, graphY - barHeight, pixelsPerHour - 1, barHeight, TFT_YELLOW);
            }

            int numTicks = (graphHours <= 4) ? 2 : 3;
            for (int i = 0; i < numTicks; i++) {
                int xPos = graphX + (i * graphWidth / (numTicks - 1));
                tft.drawLine(xPos, graphY, xPos, graphY + 10, TFT_GREEN);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextFont(1);
                int labelX = xPos - 6;
                if (labelX < 0) labelX = 0;
                if (labelX > graphX + graphWidth - 12) labelX = graphX + graphWidth - 12;
                tft.setCursor(labelX, graphY + 2);
                int relativeTime = -graphHours + (i * graphHours / (numTicks - 1));
                tft.print(relativeTime);
            }

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextFont(1);
            tft.setCursor(graphX - 10, graphY - 5); tft.print("0");
            tft.setCursor(graphX - 10, graphY - graphHeight); tft.print("10");
        } else { // Graf za ostale senzorje
            const int displayPoints = 130;
            float values[displayPoints] = {0};
            float smoothed[displayPoints] = {0};

            if (numPoints >= displayPoints) {
                int pointsPerPixel = numPoints / displayPoints;
                for (int i = 0; i < displayPoints; i++) {
                    float sum = 0;
                    int count = 0;
                    int startIdx = i * pointsPerPixel;
                    int endIdx = min((i + 1) * pointsPerPixel, numPoints);
                    for (int j = startIdx; j < endIdx; j++) {
                        float value = 0;
                        switch (currentGraphSensor) {
                            case 0: value = validData[j].temp; break;
                            case 1: value = validData[j].humidity; break;
                            case 2: value = validData[j].pressure; break;
                            case 3: value = validData[j].voc; break;
                            case 4: value = validData[j].lux; break;
                            case 5: value = validData[j].cct; break;
                        }
                        if (value > 0 && value < 1e6) {
                            sum += value;
                            count++;
                        }
                    }
                    values[i] = count > 0 ? sum / count : 0;
                }
            } else {
                float pixelsPerPoint = (float)displayPoints / numPoints;
                for (int i = 0; i < numPoints; i++) {
                    float value = 0;
                    switch (currentGraphSensor) {
                        case 0: value = validData[i].temp; break;
                        case 1: value = validData[i].humidity; break;
                        case 2: value = validData[i].pressure; break;
                        case 3: value = validData[i].voc; break;
                        case 4: value = validData[i].lux; break;
                        case 5: value = validData[i].cct; break;
                    }
                    if (value > 0 && value < 1e6) {
                        int idx = (int)(i * pixelsPerPoint);
                        values[idx] = value;
                    }
                }
                for (int i = 1; i < displayPoints; i++) {
                    if (values[i] == 0 && i > 0) {
                        values[i] = values[i - 1];
                    }
                }
            }

            for (int i = 0; i < displayPoints; i++) {
                if (values[i] <= 0) {
                    smoothed[i] = 0;
                    continue;
                }
                int windowSize = 5;
                float sum = 0;
                int count = 0;
                for (int j = -windowSize / 2; j <= windowSize / 2; j++) {
                    int idx = i + j;
                    if (idx >= 0 && idx < displayPoints && values[idx] > 0) {
                        sum += values[idx];
                        count++;
                    }
                }
                smoothed[i] = count > 0 ? sum / count : values[i];
            }

            float rawMin = 9999, rawMax = -9999;
            for (int i = 0; i < displayPoints; i++) {
                if (smoothed[i] > 0) {
                    if (smoothed[i] > rawMax) rawMax = smoothed[i];
                    if (smoothed[i] < rawMin) rawMin = smoothed[i];
                }
            }
            if (rawMax <= rawMin) rawMax = rawMin + 1;

            const float steps[] = {5.0, 10.0, 10.0, 10.0, 100.0, 1000.0};
            float minValue = floor(rawMin / steps[currentGraphSensor]) * steps[currentGraphSensor];
            float maxValue = ceil(rawMax / steps[currentGraphSensor]) * steps[currentGraphSensor];
            float range = maxValue - minValue;
            if (range < 2 * steps[currentGraphSensor]) {
                minValue = floor((rawMin - steps[currentGraphSensor]) / steps[currentGraphSensor]) * steps[currentGraphSensor];
                maxValue = ceil((rawMax + steps[currentGraphSensor]) / steps[currentGraphSensor]) * steps[currentGraphSensor];
            }

            int numTicks = (graphHours <= 4) ? 2 : 3;
            for (int i = 0; i < numTicks; i++) {
                int xPos = graphX + (i * graphWidth / (numTicks - 1));
                tft.drawLine(xPos, graphY, xPos, graphY + 10, TFT_GREEN);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextFont(1);
                int labelX = xPos - 6;
                if (labelX < 0) labelX = 0;
                if (labelX > graphX + graphWidth - 12) labelX = graphX + graphWidth - 12;
                tft.setCursor(labelX, graphY + 2);
                int relativeTime = -graphHours + (i * graphHours / (numTicks - 1));
                tft.print(relativeTime);
            }

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextFont(1);
            printMinMaxLabel(minValue, graphX - 18, graphY - 5);
            printMinMaxLabel(maxValue, graphX - 18, graphY - graphHeight);

            for (int i = 0; i < numPoints - 1; i++) {
                float value1 = 0, value2 = 0;
                switch (currentGraphSensor) {
                    case 0: value1 = validData[i].temp; value2 = validData[i + 1].temp; break;
                    case 1: value1 = validData[i].humidity; value2 = validData[i + 1].humidity; break;
                    case 2: value1 = validData[i].pressure; value2 = validData[i + 1].pressure; break;
                    case 3: value1 = validData[i].voc; value2 = validData[i + 1].voc; break;
                    case 4: value1 = validData[i].lux; value2 = validData[i + 1].lux; break;
                    case 5: value1 = validData[i].cct; value2 = validData[i + 1].cct; break;
                }
                if (value1 > 0 && value2 > 0 && value1 < 1e6 && value2 < 1e6) {
                    float x1 = graphX + ((validData[i].timestamp - startTime) / (graphHours * 3600.0)) * graphWidth;
                    float y1 = graphY - ((value1 - minValue) * graphHeight / (maxValue - minValue));
                    float x2 = graphX + ((validData[i + 1].timestamp - startTime) / (graphHours * 3600.0)) * graphWidth;
                    float y2 = graphY - ((value2 - minValue) * graphHeight / (maxValue - minValue));
                    if (x1 >= graphX && x2 <= graphX + graphWidth) {
                        tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
                    }
                }
            }
        }

        lastGraphUpdateTime = millis();
    }
}

void narisiServisniZgornjiDel() {
    unsigned long casDelovanja = millis() / 1000;
    int dni = casDelovanja / 86400;
    int ure = (casDelovanja % 86400) / 3600;
    int minute = (casDelovanja % 3600) / 60;
    String formatiranCas = String(dni) + "D " + String(ure) + "H " + String(minute) + "m";
    String stanjeWiFi = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Prekinjen";
    int ramUsage = (ESP.getFreeHeap() * 100) / ESP.getHeapSize();

    static String zadnjiFormatiranCas = "";
    static String zadnjeStanjeWiFi = "";
    static int zadnjiRamUsage = -1;
    if (formatiranCas != zadnjiFormatiranCas || stanjeWiFi != zadnjeStanjeWiFi || ramUsage != zadnjiRamUsage) {
        zadnjiFormatiranCas = formatiranCas;
        zadnjeStanjeWiFi = stanjeWiFi;
        zadnjiRamUsage = ramUsage;

        tft.fillRect(0, 0, SCREEN_WIDTH, TOP_SECTION_HEIGHT, TFT_BLACK);
        tft.setTextFont(2);
        tft.setTextColor(MY_LIGHTGREEN, TFT_BLACK);
        tft.setCursor(10, 10);
        tft.println(formatiranCas);
        tft.setCursor(10, 25);
        tft.println("Wi-Fi: " + stanjeWiFi);
        tft.setCursor(10, 40);
        tft.printf("RAM: %d%%", ramUsage);

        int rssi = WiFi.RSSI();
        uint8_t signalStrength = 3;
        if (rssi < -80) signalStrength = 1;
        else if (rssi < -67) signalStrength = 2;

        uint16_t wifiColor;
        if (sendingHTTP) {
            wifiColor = TFT_BLUE;
        } else if (httpStatus != 0 && millis() - httpStatusTime < 1000) {
            wifiColor = (httpStatus == 1) ? TFT_GREEN : TFT_RED;
        } else {
            wifiColor = (WiFi.status() == WL_CONNECTED) ? TFT_WHITE : TFT_GRAY;
        }

        if (WiFi.status() == WL_CONNECTED) {
            tft.fillCircle(150, 20, 3, signalStrength >= 1 ? wifiColor : TFT_GRAY);
            tft.drawCircle(150, 20, 6, signalStrength >= 2 ? wifiColor : TFT_GRAY);
            tft.drawCircle(150, 20, 10, signalStrength == 3 ? wifiColor : TFT_GRAY);
        } else {
            tft.fillCircle(150, 20, 3, TFT_GRAY);
            tft.drawCircle(150, 20, 6, TFT_GRAY);
            tft.drawCircle(150, 20, 10, TFT_GRAY);
            tft.drawLine(140, 10, 160, 30, TFT_RED);
            tft.drawLine(141, 10, 161, 30, TFT_RED);
            tft.drawLine(139, 10, 159, 30, TFT_RED);
            tft.drawLine(140, 30, 160, 10, TFT_RED);
            tft.drawLine(141, 30, 161, 10, TFT_RED);
            tft.drawLine(139, 30, 159, 10, TFT_RED);
        }

        lastTopUpdateTime = millis();
    }
}

void narisiServisniSrednjiDel(bool initial) {
    static int lastGraphHoursDisplayed = -1;
    if (initial || graphHours != lastGraphHoursDisplayed) {
        lastGraphHoursDisplayed = graphHours;
        tft.fillRect(0, TOP_SECTION_HEIGHT, SCREEN_WIDTH, MIDDLE_SECTION_HEIGHT + BOTTOM_SECTION_HEIGHT, TFT_BLACK);
        tft.setTextFont(2);
        int yPoz = TOP_SECTION_HEIGHT + 5;

        float tempProcesor = temperatureRead();
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, yPoz); tft.println("CPU T: " + String(tempProcesor, 1) + " C");
        yPoz += 18;

        if (touchPresent && checkI2CDevice(CST816_SLAVE_ADDRESS)) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("Dotik: 0x15");
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("Dotik: Napaka");
        }
        yPoz += 18;

        if (tcsPresent && checkI2CDevice(0x29)) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("TCS34725: 0x29");
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("TCS34725: Napaka");
        }
        yPoz += 18;

        if (bmePresent && checkI2CDevice(bme680Address)) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("BME680: 0x" + String(bme680Address, HEX));
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("BME680: Napaka");
        }
        yPoz += 18;

        if (sht41Present && checkI2CDevice(0x44)) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("SHT41: 0x44");
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, yPoz); tft.println("SHT41: Napaka");
        }
        yPoz += 18;

        String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, yPoz); tft.println("IP: " + ip);
        yPoz += 18;

        String stanjeNTP = (timeClient.getEpochTime() > 0) ? "Sinhroniziran" : "Ni uspel";
        tft.setCursor(10, yPoz); tft.println("NTP: " + stanjeNTP);
        yPoz += 18;

        String httpStatusStr = (httpAttemptCount > 0) ? String((httpSuccessCount * 100) / httpAttemptCount) + "%" : "Nedosegljiv";
        tft.setCursor(10, yPoz); tft.println("HTTP: " + httpStatusStr);
        yPoz += 18;

        fs::File file = LittleFS.open(HISTORY_FILE, FILE_READ);
        if (file) {
            int totalEntries = file.size() / sizeof(SensorData);
            float dataPercentage = (totalEntries / (float)MAX_ENTRIES) * 100.0;
            SensorData firstData, lastData;
            file.seek(0);
            file.read((uint8_t*)&firstData, sizeof(SensorData));
            file.seek(file.size() - sizeof(SensorData));
            file.read((uint8_t*)&lastData, sizeof(SensorData));
            file.close();
            unsigned long dataAgeSeconds = lastData.timestamp - firstData.timestamp;
            int dataAgeHours = dataAgeSeconds / 3600;
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(10, yPoz);
            tft.printf("Data: %.1f%% (%dh)", dataPercentage, dataAgeHours);
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, yPoz);
            tft.println("Data: Napaka");
        }
        yPoz += 18;

        tft.setTextFont(4);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(70, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 20);
        if (graphHours < 10) {
            tft.printf("0%d", graphHours);
        } else {
            tft.printf("%d", graphHours);
        }

        tft.fillRoundRect(10, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15, 50, 30, 10, MY_DARKGREEN);
        tft.drawRoundRect(10, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15, 50, 30, 10, TFT_WHITE);
        tft.drawLine(35, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 18, 35, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 42, TFT_WHITE);
        tft.drawLine(15, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 30, 55, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 30, TFT_WHITE);

        tft.fillRoundRect(110, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15, 50, 30, 10, MY_DARKRED);
        tft.drawRoundRect(110, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15, 50, 30, 10, TFT_WHITE);
        tft.drawLine(115, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 30, 155, TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 30, TFT_WHITE);

        lastServiceMiddleUpdateTime = millis();
    }
}

void prikaziGlavniZaslon() {
    narisiZgornjiDel();
    narisiSrednjiDel();
    narisiSpodnjiDel();
}

void prikaziServisniZaslon() {
    static bool firstRun = true;
    narisiServisniZgornjiDel();
    narisiServisniSrednjiDel(firstRun);
    firstRun = false;
}

void printMinMaxLabel(float value, int x, int y) {
    tft.setCursor(x, y);
    if (currentGraphSensor == 2) {
        if (value >= 1000) tft.print("1k" + String((int)(value - 1000)));
        else tft.print((int)value);
    } else if (currentGraphSensor == 3) {
        tft.print(String((int)value) + "k");
    } else if (currentGraphSensor == 4) {
        if (value >= 1000) tft.print(String((int)(value / 1000)) + "k");
        else tft.print((int)value);
    } else if (currentGraphSensor == 5) {
        tft.print(String((int)(value / 1000)) + "k");
    } else {
        tft.print((int)value);
    }
}

void loop() {
    // Zaznavanje gibanja
    if (digitalRead(MOTION_SENSOR_PIN) == HIGH) {
        if (!motionDetected) {
            if (DEBUG_MODE) Serial.println("Gibanje zaznano");
            motionDetected = true;
            // LED utripanje ponoči
            if (lastLux < NIGHT_LUX_THRESHOLD && millis() - lastLedBlinkTime >= LED_BLINK_INTERVAL) {
                ledcWrite(1, 64); delay(50); ledcWrite(1, 0); delay(250);
                ledcWrite(1, 64); delay(50); ledcWrite(1, 0);
                lastLedBlinkTime = millis();
                if (DEBUG_MODE) Serial.println("LED utripnil 2x zaradi gibanja");
            }
        }
        lastMotionTime = millis();
        if (!screenOn) {
            ledcWrite(0, 255);
            screenOn = true;
            if (DEBUG_MODE) Serial.println("Zaslon vklopljen zaradi gibanja");
        }
    }

    // Vklop zaslona ob dotiku ali pritisku na tipko
    int btn1State = digitalRead(BTN1_PIN);
    int btn2State = digitalRead(BTN2_PIN);
    uint8_t dotaknjen = touchPresent ? touch.getPoint(x, y, touch.getSupportTouchPoint()) : 0;
    if ((dotaknjen > 0 || btn1State == LOW || btn2State == LOW) && !screenOn) {
        lastMotionTime = millis();
        ledcWrite(0, 255);
        screenOn = true;
        if (DEBUG_MODE) Serial.println("Zaslon vklopljen zaradi dotika ali tipke");
    }

    // Izklop zaslona po 1 minuti
    if (screenOn && (millis() - lastMotionTime > MOTION_TIMEOUT)) {
        ledcWrite(0, 0);
        screenOn = false;
        if (DEBUG_MODE) Serial.println("Zaslon ugasnjen");
    }

    // LED prehod vsako uro ponoči
    if (lastLux < NIGHT_LUX_THRESHOLD && millis() - lastLedHourTime >= LED_HOUR_INTERVAL) {
        time_t now = timeClient.getEpochTime();
        struct tm* timeinfo = localtime(&now);
        if (timeinfo->tm_min == 0 && timeinfo->tm_sec < 1) {
            for (int i = 0; i <= 255; i += 25) { ledcWrite(1, i); delay(50); }
            for (int i = 255; i >= 0; i -= 25) { ledcWrite(1, i); delay(50); }
            lastLedHourTime = millis();
            if (DEBUG_MODE) Serial.println("LED prehod izveden ob polni uri");
        }
    }

    // Obdelava dotika
    if (millis() - lastTouchReadTime >= TOUCH_READ_INTERVAL) {
        dotaknjen = touchPresent ? touch.getPoint(x, y, touch.getSupportTouchPoint()) : 0;
        bool motion = digitalRead(MOTION_SENSOR_PIN) == HIGH;

        if (dotaknjen && (millis() - lastTouchTime > TOUCH_READ_INTERVAL)) {
            lastTouchTime = millis();
            lastInteractionTime = millis();
            bool previousScreen = mainScreen;
            for (int i = 0; i < dotaknjen; ++i) {
                if (y[i] < TOP_SECTION_HEIGHT) {
                    if (DEBUG_MODE) Serial.println("Pritisnjen gumb_zgoraj (touch)");
                    mainScreen = !mainScreen;
                    tft.fillScreen(TFT_BLACK);
                    if (mainScreen) {
                        narisiZgornjiDel();
                        narisiSrednjiDel(true);
                        narisiSpodnjiDel(true);
                        if (!previousScreen) saveGraphHoursToEEPROM();
                    } else {
                        narisiServisniZgornjiDel();
                        narisiServisniSrednjiDel(true);
                    }
                } else if (y[i] >= TOP_SECTION_HEIGHT && y[i] < TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT) {
                    if (DEBUG_MODE) Serial.println("Pritisnjen gumb_srednji (touch)");
                    currentGraphSensor = (currentGraphSensor + 1) % 7;
                    narisiSpodnjiDel(false, true);
                    narisiSrednjiDel(true);
                    if (DEBUG_MODE) Serial.println("Trenutni currentGraphSensor: " + String(currentGraphSensor));
                } else if (y[i] >= TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT && !mainScreen) {
                    if (DEBUG_MODE) Serial.println("Pritisnjen gumb_spodaj na servisnem zaslonu (touch)");
                    if (x[i] >= 10 && x[i] <= 60 && y[i] >= TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15 && y[i] <= TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 45) {
                        if (graphHours < MAX_GRAPH_HOURS) graphHours++;
                        if (DEBUG_MODE) Serial.println("Povecanje stevila ur: " + String(graphHours));
                        narisiServisniSrednjiDel();
                    } else if (x[i] >= 110 && x[i] <= 160 && y[i] >= TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 15 && y[i] <= TOP_SECTION_HEIGHT + MIDDLE_SECTION_HEIGHT + 45) {
                        if (graphHours > MIN_GRAPH_HOURS) graphHours--;
                        if (DEBUG_MODE) Serial.println("Zmanjsanje stevila ur: " + String(graphHours));
                        narisiServisniSrednjiDel();
                    }
                }
            }
        }
        lastTouchReadTime = millis();
    }

    // Obdelava tipk
    if (btn1State == LOW && !btn1Pressed) {
        btn1Pressed = true;
        lastBtn1Press = millis();
        lastInteractionTime = millis();
        if (DEBUG_MODE) Serial.println("BTN1 pritisnjen");
    } else if (btn1State == LOW && btn1Pressed) {
        unsigned long pressDuration = millis() - lastBtn1Press;
        if (pressDuration >= BUTTON_LONG_PRESS) {
            if (DEBUG_MODE) Serial.println("Dolg pritisk BTN1 zaznan");
            bool previousScreen = mainScreen;
            mainScreen = !mainScreen;
            tft.fillScreen(TFT_BLACK);
            if (mainScreen) {
                narisiZgornjiDel();
                narisiSrednjiDel(true);
                narisiSpodnjiDel(true);
                if (!previousScreen) saveGraphHoursToEEPROM();
            } else {
                narisiServisniZgornjiDel();
                narisiServisniSrednjiDel(true);
            }
            btn1Pressed = false;
        }
    } else if (btn1State == HIGH && btn1Pressed) {
        unsigned long pressDuration = millis() - lastBtn1Press;
        if (pressDuration > DEBOUNCE_DELAY && pressDuration < BUTTON_LONG_PRESS) {
            if (DEBUG_MODE) Serial.println("Kratek pritisk BTN1");
            if (mainScreen) {
                currentGraphSensor = (currentGraphSensor + 1) % 7;
                narisiSpodnjiDel(false, true);
                narisiSrednjiDel(true);
                if (DEBUG_MODE) Serial.println("Trenutni currentGraphSensor: " + String(currentGraphSensor));
            } else {
                if (graphHours < MAX_GRAPH_HOURS) {
                    graphHours++;
                    if (DEBUG_MODE) Serial.println("Povecanje stevila ur: " + String(graphHours));
                    narisiServisniSrednjiDel();
                }
            }
        }
        btn1Pressed = false;
    }

    if (btn2State == LOW && !btn2Pressed) {
        btn2Pressed = true;
        lastBtn2Press = millis();
        lastInteractionTime = millis();
        if (DEBUG_MODE) Serial.println("BTN2 pritisnjen");
    } else if (btn2State == LOW && btn2Pressed) {
        unsigned long pressDuration = millis() - lastBtn2Press;
        if (pressDuration >= BUTTON_LONG_PRESS) {
            if (DEBUG_MODE) Serial.println("Dolg pritisk BTN2 zaznan");
            bool previousScreen = mainScreen;
            mainScreen = !mainScreen;
            tft.fillScreen(TFT_BLACK);
            if (mainScreen) {
                narisiZgornjiDel();
                narisiSrednjiDel(true);
                narisiSpodnjiDel(true);
                if (!previousScreen) saveGraphHoursToEEPROM();
            } else {
                narisiServisniZgornjiDel();
                narisiServisniSrednjiDel(true);
            }
            btn2Pressed = false;
        }
    } else if (btn2State == HIGH && btn2Pressed) {
        unsigned long pressDuration = millis() - lastBtn2Press;
        if (pressDuration > DEBOUNCE_DELAY && pressDuration < BUTTON_LONG_PRESS) {
            if (DEBUG_MODE) Serial.println("Kratek pritisk BTN2");
            if (mainScreen) {
                currentGraphSensor = (currentGraphSensor - 1 + 7) % 7;
                narisiSpodnjiDel(false, true);
                narisiSrednjiDel(true);
                if (DEBUG_MODE) Serial.println("Trenutni currentGraphSensor: " + String(currentGraphSensor));
            } else {
                if (graphHours > MIN_GRAPH_HOURS) {
                    graphHours--;
                    if (DEBUG_MODE) Serial.println("Zmanjsanje stevila ur: " + String(graphHours));
                    narisiServisniSrednjiDel();
                }
            }
        }
        btn2Pressed = false;
    }

    // Preklop LED z obema gumboma
    if (btn1State == LOW && btn2State == LOW) {
        if (bothPressedTime == 0) {
            bothPressedTime = millis();
            lastInteractionTime = millis();
            if (DEBUG_MODE) Serial.println("Oba gumba pritisnjena, začetek merjenja");
        } else if (millis() - bothPressedTime >= 500) {
            if (DEBUG_MODE) Serial.println("Hkratni pritisk na oba gumba, LED " + String(ledState ? "izklopljena" : "vklopljena"));
            ledState = !ledState;
            ledcWrite(1, ledState ? 255 : 0);
            bothPressedTime = 0;
        }
    } else {
        bothPressedTime = 0;
    }

    // Redni posodobitveni klici
    if (DEBUG_MODE && millis() - lastMillisCheck >= 30000) {
        Serial.println("Trenutni millis: " + String(millis()));
        lastMillisCheck = millis();
    }

    if (millis() - lastWiFiCheckTime >= 300000) {
        connectWiFi();
        lastWiFiCheckTime = millis();
    }

    if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
        readSensors();
    }

    if (millis() - lastDataSaveTime >= DATA_SAVE_INTERVAL) {
        saveSensorData();
    }

    if (!mainScreen && millis() - lastInteractionTime >= SERVIS_TIMEOUT) {
        if (DEBUG_MODE) Serial.println("Samodejni prehod na glavni zaslon po neaktivnosti");
        mainScreen = true;
        tft.fillScreen(TFT_BLACK);
        narisiZgornjiDel();
        narisiSrednjiDel(true);
        narisiSpodnjiDel(true);
        saveGraphHoursToEEPROM();
    }

    if (mainScreen) {
        prikaziGlavniZaslon();
    } else {
        prikaziServisniZaslon();
    }
}

// --- Konec četrtega dela ---