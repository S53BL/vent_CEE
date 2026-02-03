// sens.cpp

#include "sens.h"
#include <functional>

void initI2CBus() {
    // Initialize I2C bus only once
    static bool i2cInitialized = false;
    if (!i2cInitialized) {
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        Wire.setClock(100000);
        Wire.setTimeout(100);
        i2cInitialized = true;
        LOG_INFO("I2C", "Bus initialized");
    }
}

void initSensors() {
    char logMessage[256];

    // Initialize I2C bus
    initI2CBus();

    // Check and initialize SHT41
    bool sht41_ok = false;
    if (checkI2CDevice(SHT41_ADDRESS)) {
        sht41 = new Adafruit_SHT4x();
        for (int retry = 0; retry < 3; retry++) {
            if (sht41->begin()) {
                sht41_ok = true;
                sht41Present = true;
                sht41->setPrecision(SHT4X_HIGH_PRECISION);
                sht41->setHeater(SHT4X_NO_HEATER);
                break;
            } else {
                delete sht41;
                sht41 = nullptr;
            }
            delay(200);
        }
    } else {
        LOG_INFO("SHT41", "Sensor not detected on I2C bus");
    }

    if (!sht41_ok) {
        logEvent("SHT41 init failed");
        currentData.errorFlags |= ERR_SHT41;
        sht41Present = false;
    }

    // Check and initialize BME280
    bool bme280_ok = false;
    if (checkI2CDevice(BME280_ADDRESS)) {
        bme280 = new Adafruit_BME280();
        for (int retry = 0; retry < 3; retry++) {
            if (bme280->begin(BME280_ADDRESS)) {
                bme280_ok = true;
                bmePresent = true;
                break;
            } else {
                delete bme280;
                bme280 = nullptr;
            }
            delay(200);
        }
    } else {
        LOG_INFO("BME280", "Sensor not detected on I2C bus");
    }

    if (!bme280_ok) {
        logEvent("BME280 init failed");
        currentData.errorFlags |= ERR_BME280;
        bmePresent = false;
    }

    if (sht41_ok && bme280_ok) {
        logEvent("Sensors initialized");
    } else if (sht41_ok || bme280_ok) {
        logEvent("Partial sensor initialization");
    } else {
        logEvent("No sensors initialized");
    }
}

// Generic sensor reading function with retry logic
bool readSensorWithRetry(
    uint8_t address,
    std::function<bool(float*, float*, float*)> readCallback,
    float minTemp, float maxTemp,
    float minHum, float maxHum,
    float minPress, float maxPress,
    float* tempOut, float* humOut, float* pressOut,
    const char* sensorName
) {
    char logMessage[256];

    // First attempt
    if (checkI2CDevice(address)) {
        float temp, hum = -999.0f, press = -999.0f;
        if (readCallback(&temp, &hum, &press)) {
            // Validate readings
            bool tempValid = (temp >= minTemp && temp <= maxTemp);
            bool humValid = (hum == -999.0f) || (hum >= minHum && hum <= maxHum);
            bool pressValid = (press == -999.0f) || (press >= minPress && press <= maxPress);

            if (tempValid && humValid && pressValid) {
                *tempOut = temp;
                if (humOut) *humOut = hum;
                if (pressOut) *pressOut = press;
                return true;
            } else {
                snprintf(logMessage, sizeof(logMessage), "Invalid %s data: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                        sensorName, temp, hum, press);
                logEvent(logMessage);
                return false;
            }
        }
    }

    // First attempt failed, reset bus and retry
    resetI2CBus();

    if (checkI2CDevice(address)) {
        float temp, hum = -999.0f, press = -999.0f;
        if (readCallback(&temp, &hum, &press)) {
            // Validate readings
            bool tempValid = (temp >= minTemp && temp <= maxTemp);
            bool humValid = (hum == -999.0f) || (hum >= minHum && hum <= maxHum);
            bool pressValid = (press == -999.0f) || (press >= minPress && press <= maxPress);

            if (tempValid && humValid && pressValid) {
                *tempOut = temp;
                if (humOut) *humOut = hum;
                if (pressOut) *pressOut = press;
                return true;
            } else {
                snprintf(logMessage, sizeof(logMessage), "Invalid %s data after retry: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                        sensorName, temp, hum, press);
                logEvent(logMessage);
                return false;
            }
        }
    }

    // Both attempts failed
    snprintf(logMessage, sizeof(logMessage), "%s I2C error after reset", sensorName);
    logEvent(logMessage);
    return false;
}

// Callback function for SHT41 reading
bool readSHT41(float* temp, float* hum, float* press) {
    if (!sht41) return false;
    sensors_event_t humidity, temperature;
    sht41->getEvent(&humidity, &temperature);
    *temp = temperature.temperature;
    *hum = humidity.relative_humidity;
    *press = -999.0f; // SHT41 doesn't measure pressure
    return true;
}

// Callback function for BME280 reading
bool readBME280(float* temp, float* hum, float* press) {
    if (!bme280) return false;
    *temp = bme280->readTemperature();
    *hum = bme280->readHumidity() + BME_HUMIDITY_OFFSET;
    *press = bme280->readPressure() / 100.0;
    return true;
}

void readSensors() {
    // Update timestamp if NTP is synced
    if (timeSynced) {
        currentData.timestamp = myTZ.now();
    }

    // Read SHT41 only if present
    if (sht41Present) {
        if (!readSensorWithRetry(
            SHT41_ADDRESS,
            readSHT41,
            0.0f, 50.0f,    // temp range
            10.0f, 100.0f,  // hum range
            -999.0f, -999.0f, // no pressure validation
            &currentData.utilityTemp,
            &currentData.utilityHumidity,
            nullptr, // no pressure output
            "SHT41"
        )) {
            currentData.errorFlags |= ERR_SHT41;
        } else {
            currentData.errorFlags &= ~ERR_SHT41;
        }
    } else {
        // Sensor not present, set error flag
        currentData.errorFlags |= ERR_SHT41;
    }

    // Read BME280 only if present
    if (bmePresent) {
        if (!readSensorWithRetry(
            BME280_ADDRESS,
            readBME280,
            0.0f, 50.0f,     // temp range
            10.0f, 100.0f,   // hum range
            300.0f, 1100.0f, // pressure range
            &currentData.bathroomTemp,
            &currentData.bathroomHumidity,
            &currentData.bathroomPressure,
            "BME280"
        )) {
            currentData.errorFlags |= ERR_BME280;
        } else {
            currentData.errorFlags &= ~ERR_BME280;
        }
    } else {
        // Sensor not present, set error flag
        currentData.errorFlags |= ERR_BME280;
    }
}

bool checkI2CDevice(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

void resetI2CBus() {
    char logMessage[256];
    // Toggle SCL 9x for bus clear
    pinMode(PIN_I2C_SCL, OUTPUT);
    pinMode(PIN_I2C_SDA, INPUT); // SDA input for checking
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_I2C_SCL, LOW);
        delayMicroseconds(10);
        digitalWrite(PIN_I2C_SCL, HIGH);
        delayMicroseconds(10);
    }
    // Check SDA
    if (digitalRead(PIN_I2C_SDA) == LOW) {
        snprintf(logMessage, sizeof(logMessage), "[I2C] Error: SDA stuck low after bus recovery");
        logEvent(logMessage);
    } else {
        snprintf(logMessage, sizeof(logMessage), "[I2C] Bus recovery successful");
        logEvent(logMessage);
    }
    // Reinitialize I2C with slower speed for recovery
    Wire.setClock(10000); // 10 kHz
    Wire.setTimeout(I2C_TIMEOUT_MS); // 50 ms
    // Use centralized init to avoid multiple Wire.begin calls
    initI2CBus();
}

void performPeriodicSensorCheck() {
    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck >= 600000) { // 10 minut
        bool sensorStateChanged = false;

        // Check SHT41
        if (!sht41Present && checkI2CDevice(SHT41_ADDRESS)) {
            // Try to reinitialize SHT41
            sht41 = new Adafruit_SHT4x();
            bool sht41_reconnected = false;
            for (int retry = 0; retry < 3; retry++) {
                if (sht41->begin()) {
                    sht41->setPrecision(SHT4X_HIGH_PRECISION);
                    sht41->setHeater(SHT4X_NO_HEATER);
                    sht41_reconnected = true;
                    sht41Present = true;
                    currentData.errorFlags &= ~ERR_SHT41;
                    LOG_INFO("SHT41", "Sensor ponovno priključen in inicializiran");
                    sensorStateChanged = true;
                    break;
                }
                delay(200);
            }
            if (!sht41_reconnected) {
                delete sht41;
                sht41 = nullptr;
                LOG_WARN("SHT41", "Sensor zaznан ampak inicializacija ni uspela");
            }
        }

        // Check BME280
        if (!bmePresent && checkI2CDevice(BME280_ADDRESS)) {
            // Try to reinitialize BME280
            bme280 = new Adafruit_BME280();
            bool bme_reconnected = false;
            for (int retry = 0; retry < 3; retry++) {
                if (bme280->begin(BME280_ADDRESS)) {
                    bme_reconnected = true;
                    bmePresent = true;
                    currentData.errorFlags &= ~ERR_BME280;
                    LOG_INFO("BME280", "Sensor ponovno priključen in inicializiran");
                    sensorStateChanged = true;
                    break;
                }
                delay(200);
            }
            if (!bme_reconnected) {
                delete bme280;
                bme280 = nullptr;
                LOG_WARN("BME280", "Sensor zaznан ampak inicializacija ni uspela");
            }
        }

        // Log if any sensors are still missing
        if (!sensorStateChanged && (!sht41Present || !bmePresent)) {
            static bool lastMissingLog = false;
            if (!lastMissingLog) {
                char missingMsg[100] = "";
                if (!sht41Present && !bmePresent) {
                    strcpy(missingMsg, "SHT41 in BME280 senzorja nista dostopna");
                } else if (!sht41Present) {
                    strcpy(missingMsg, "SHT41 senzor ni dostopen");
                } else if (!bmePresent) {
                    strcpy(missingMsg, "BME280 senzor ni dostopen");
                }
                LOG_INFO("Sensors", "%s", missingMsg);
                lastMissingLog = true;
            }
        } else if (sensorStateChanged) {
            // Reset the missing log flag when sensors are reconnected
            // This will be set by the static variable above
        }

        lastSensorCheck = millis();
    }
}

void performPeriodicI2CReset() {
    static unsigned long lastReset = 0;
    if (millis() - lastReset >= 1800000) { // 30 minut
        resetI2CBus();
        LOG_INFO("I2C", "Periodična inicializacija");
        lastReset = millis();
    }
}

void setupInputs() {
    pinMode(PIN_KOPALNICA_TIPKA, INPUT_PULLUP);
    pinMode(PIN_UTILITY_STIKALO, INPUT_PULLUP);
    pinMode(PIN_OKNO_SENZOR_1, INPUT_PULLUP);
    pinMode(PIN_OKNO_SENZOR_2, INPUT_PULLUP);
    pinMode(PIN_KOPALNICA_LUC_1, INPUT_PULLUP);
    pinMode(PIN_KOPALNICA_LUC_2, INPUT_PULLUP);
    pinMode(PIN_UTILITY_LUC, INPUT_PULLUP);
    pinMode(PIN_WC_LUC, INPUT_PULLUP);
}

void readInputs() {
    // Read all digital inputs at once and update currentData
    currentData.bathroomButton = digitalRead(PIN_KOPALNICA_TIPKA) == LOW;
    currentData.utilitySwitch = digitalRead(PIN_UTILITY_STIKALO) == LOW;
    currentData.windowSensor1 = digitalRead(PIN_OKNO_SENZOR_1) == LOW;
    currentData.windowSensor2 = digitalRead(PIN_OKNO_SENZOR_2) == LOW;
    currentData.bathroomLight1 = digitalRead(PIN_KOPALNICA_LUC_1) == LOW;
    currentData.bathroomLight2 = digitalRead(PIN_KOPALNICA_LUC_2) == LOW;
    currentData.utilityLight = digitalRead(PIN_UTILITY_LUC) == LOW;
    currentData.wcLight = digitalRead(PIN_WC_LUC) == LOW;

    // Logging for state changes
    static bool lastButtonState = currentData.bathroomButton;
    static bool lastLightState1 = currentData.bathroomLight1;
    static bool lastLightState2 = currentData.bathroomLight2;
    static bool lastWindowOpen = !currentData.windowSensor1 || !currentData.windowSensor2;
    static unsigned long lastWindowLogTime = 0;

    const int inputPins[8] = {PIN_KOPALNICA_TIPKA, PIN_UTILITY_STIKALO, PIN_OKNO_SENZOR_1, PIN_OKNO_SENZOR_2,
                              PIN_KOPALNICA_LUC_1, PIN_KOPALNICA_LUC_2, PIN_UTILITY_LUC, PIN_WC_LUC};
    static int lastInputStates[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

    bool currentButtonState = currentData.bathroomButton;
    bool currentLightState1 = currentData.bathroomLight1;
    bool currentLightState2 = currentData.bathroomLight2;
    bool currentWindowOpen = !currentData.windowSensor1 || !currentData.windowSensor2;

    char logMessage[256];
    if (currentButtonState != lastButtonState) {
        snprintf(logMessage, sizeof(logMessage), "[KOP SW] %s", currentButtonState ? "ON" : "OFF");
        logEvent(logMessage);
        lastButtonState = currentButtonState;
    }
    if (currentLightState1 != lastLightState1) {
        snprintf(logMessage, sizeof(logMessage), "[KOP Luč 1] %s", currentLightState1 ? "ON" : "OFF");
        logEvent(logMessage);
        lastLightState1 = currentLightState1;
    }
    if (currentLightState2 != lastLightState2) {
        snprintf(logMessage, sizeof(logMessage), "[KOP Luč 2] %s", currentLightState2 ? "ON" : "OFF");
        logEvent(logMessage);
        lastLightState2 = currentLightState2;
    }

    for (int i = 0; i < 8; i++) {
        int currentState = digitalRead(inputPins[i]);
        if (currentState != lastInputStates[i]) {
            const char* event;
            if (i == 0) {
                event = currentState == LOW ? "KOP SW ON" : "KOP SW OFF";
            } else if (i == 1) {
                event = currentState == LOW ? "UT SW ON" : "UT SW OFF";
            } else if (i == 2) {
                event = currentState == LOW ? "Okna Streha Zaprta" : "Okna Streha Odprta";
            } else if (i == 3) {
                event = currentState == LOW ? "Okna Balkon Zaprta" : "Okna Balkon Odprta";
            } else if (i == 4) {
                event = currentState == LOW ? "KOP Luč 1 ON" : "KOP Luč 1 OFF";
            } else if (i == 5) {
                event = currentState == LOW ? "KOP Luč 2 ON" : "KOP Luč 2 OFF";
            } else if (i == 6) {
                event = currentState == LOW ? "UT Luč ON" : "UT Luč OFF";
            } else {
                event = currentState == LOW ? "WC Luč ON" : "WC Luč OFF";
            }
            snprintf(logMessage, sizeof(logMessage), "[%s]", event);
            logEvent(logMessage);
            lastInputStates[i] = currentState;
        }
    }

    if ((currentWindowOpen != lastWindowOpen) && (millis() - lastWindowLogTime >= LOG_REPEAT_INTERVAL)) {
        snprintf(logMessage, sizeof(logMessage), "[Okna DS] %s", currentWindowOpen ? "Odprta" : "Zaprta");
        logEvent(logMessage);
        lastWindowOpen = currentWindowOpen;
        lastWindowLogTime = millis();
    }
}
