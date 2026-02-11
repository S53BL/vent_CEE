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

    // Set error flags by default - will be cleared only if ALL steps succeed
    currentData.errorFlags |= ERR_SHT41;
    currentData.errorFlags |= ERR_BME280;
    sht41Present = false;
    bmePresent = false;

    // Check and initialize SHT41
    if (checkI2CDevice(SHT41_ADDRESS)) {
        LOG_INFO("SHT41", "Sensor detected, initializing...");
        sht41 = new Adafruit_SHT4x();

        bool init_success = false;
        for (int retry = 0; retry < 3; retry++) {
            if (sht41->begin()) {
                sht41->setPrecision(SHT4X_HIGH_PRECISION);
                sht41->setHeater(SHT4X_NO_HEATER);
                init_success = true;
                break;
            }
            delay(200);
        }

        if (init_success) {
            // Try test read to verify sensor works
            sensors_event_t humidity, temperature;
            sht41->getEvent(&humidity, &temperature);

            if (temperature.temperature >= 0.0f && temperature.temperature <= 50.0f &&
                humidity.relative_humidity >= 10.0f && humidity.relative_humidity <= 100.0f) {
                // All steps successful - clear error flag
                currentData.errorFlags &= ~ERR_SHT41;
                sht41Present = true;
                LOG_INFO("SHT41", "Successfully initialized and tested - T:%.1f°C H:%.1f%%",
                        temperature.temperature, humidity.relative_humidity);
            } else {
                LOG_WARN("SHT41", "Initialization OK but test read failed - invalid values");
                delete sht41;
                sht41 = nullptr;
            }
        } else {
            LOG_WARN("SHT41", "Sensor detected but initialization failed");
            delete sht41;
            sht41 = nullptr;
        }
    } else {
        LOG_INFO("SHT41", "Sensor not detected on I2C bus");
    }

    // Check and initialize BME280
    if (checkI2CDevice(BME280_ADDRESS)) {
        LOG_INFO("BME280", "Sensor detected, initializing...");
        bme280 = new Adafruit_BME280();

        bool init_success = false;
        for (int retry = 0; retry < 3; retry++) {
            if (bme280->begin(BME280_ADDRESS)) {
                init_success = true;
                break;
            }
            delay(200);
        }

        if (init_success) {
            // Try test read to verify sensor works
            float temp = bme280->readTemperature() + settings.bmeTempOffset;
            float hum = bme280->readHumidity() + settings.bmeHumidityOffset;
            float press = bme280->readPressure() / 100.0 + settings.bmePressureOffset;

            if (temp >= 0.0f && temp <= 50.0f &&
                hum >= 10.0f && hum <= 100.0f &&
                press >= 300.0f && press <= 1100.0f) {
                // All steps successful - clear error flag
                currentData.errorFlags &= ~ERR_BME280;
                bmePresent = true;
                LOG_INFO("BME280", "Successfully initialized and tested - T:%.1f°C H:%.1f%% P:%.1fhPa",
                        temp, hum, press);
            } else {
                LOG_WARN("BME280", "Initialization OK but test read failed - invalid values");
                delete bme280;
                bme280 = nullptr;
            }
        } else {
            LOG_WARN("BME280", "Sensor detected but initialization failed");
            delete bme280;
            bme280 = nullptr;
        }
    } else {
        LOG_INFO("BME280", "Sensor not detected on I2C bus");
    }

    // Initial read of power supplies
    uint32_t adc5V_init  = analogRead(2);
    uint32_t adc3V3_init = analogRead(3);
    currentData.supply5V  = (adc5V_init  * 3.3f / 4095.0f) * 2.13f;
    currentData.supply3V3 = (adc3V3_init * 3.3f / 4095.0f) * 2.18f;

    // Log final status
    if (sht41Present && bmePresent) {
        LOG_INFO("Sensors", "All sensors initialized and tested successfully");
    } else if (sht41Present || bmePresent) {
        LOG_WARN("Sensors", "Partial sensor initialization - some sensors failed testing");
    } else {
        LOG_WARN("Sensors", "No sensors available or failed testing");
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
                LOG_WARN("Sensors", "Invalid %s data: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                        sensorName, temp, hum, press);
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
                LOG_ERROR("Sensors", "Invalid %s data after retry: T=%.1f°C, H=%.1f%%, P=%.1fhPa",
                        sensorName, temp, hum, press);
                return false;
            }
        }
    }

    // Both attempts failed
    LOG_ERROR("Sensors", "%s I2C error after reset", sensorName);
    return false;
}

// Callback function for SHT41 reading
bool readSHT41(float* temp, float* hum, float* press) {
    if (!sht41) return false;
    sensors_event_t humidity, temperature;
    sht41->getEvent(&humidity, &temperature);
    *temp = temperature.temperature + settings.shtTempOffset;
    *hum = humidity.relative_humidity + settings.shtHumidityOffset;
    *press = -999.0f; // SHT41 doesn't measure pressure
    return true;
}

// Callback function for BME280 reading
bool readBME280(float* temp, float* hum, float* press) {
    if (!bme280) return false;
    *temp = bme280->readTemperature() + settings.bmeTempOffset;
    *hum = bme280->readHumidity() + settings.bmeHumidityOffset;
    *press = bme280->readPressure() / 100.0 + settings.bmePressureOffset;
    return true;
}

void readSensors() {
    // Update timestamp if NTP is synced
    if (timeSynced) {
        currentData.timestamp = myTZ.now();
    }

    // Read SHT41 - always check presence first
    if (checkI2CDevice(SHT41_ADDRESS) && sht41Present) {
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
        // Sensor not present or not properly initialized, set error flag
        currentData.errorFlags |= ERR_SHT41;
        if (sht41Present) {
            LOG_WARN("SHT41", "Sensor was present but now unreachable");
            sht41Present = false; // Mark as not present for future checks
        }
    }

    // Read BME280 - always check presence first
    if (checkI2CDevice(BME280_ADDRESS) && bmePresent) {
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
        // Sensor not present or not properly initialized, set error flag
        currentData.errorFlags |= ERR_BME280;
        if (bmePresent) {
            LOG_WARN("BME280", "Sensor was present but now unreachable");
            bmePresent = false; // Mark as not present for future checks
        }
    }

    // Read power supplies directly
    uint32_t adc5V  = analogRead(2);   // GPIO2 za 5 V
    uint32_t adc3V3 = analogRead(3);   // GPIO3 za 3,3 V
    currentData.supply5V  = (adc5V  * 3.3f / 4095.0f) * 2.13f;
    currentData.supply3V3 = (adc3V3 * 3.3f / 4095.0f) * 2.18f;

    // Check power error
    bool powerError = (currentData.supply5V < 4.0f) || (currentData.supply3V3 < 3.0f);
    if (powerError) {
        currentData.errorFlags |= ERR_POWER;
    } else {
        currentData.errorFlags &= ~ERR_POWER;
    }

    // Log local sensors only
    LOG_INFO("Sensors", "UT: T=%.1f°C H=%.1f%% KOP: T=%.1f°C H=%.1f%% P=%.1fhPa 5V=%.3fV 3.3V=%.3fV",
             currentData.utilityTemp, currentData.utilityHumidity,
             currentData.bathroomTemp, currentData.bathroomHumidity, currentData.bathroomPressure,
             currentData.supply5V, currentData.supply3V3);
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
        LOG_ERROR("I2C", "Error: SDA stuck low after bus recovery");
    } else {
        LOG_INFO("I2C", "Bus recovery successful");
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
            LOG_INFO("SHT41", "Sensor detected during periodic check, reinitializing...");
            sht41 = new Adafruit_SHT4x();

            bool init_success = false;
            for (int retry = 0; retry < 3; retry++) {
                if (sht41->begin()) {
                    sht41->setPrecision(SHT4X_HIGH_PRECISION);
                    sht41->setHeater(SHT4X_NO_HEATER);
                    init_success = true;
                    break;
                }
                delay(200);
            }

            if (init_success) {
                // Try test read to verify sensor works
                sensors_event_t humidity, temperature;
                sht41->getEvent(&humidity, &temperature);

                if (temperature.temperature >= 0.0f && temperature.temperature <= 50.0f &&
                    humidity.relative_humidity >= 10.0f && humidity.relative_humidity <= 100.0f) {
                    // All steps successful - clear error flag and mark as present
                    sht41Present = true;
                    currentData.errorFlags &= ~ERR_SHT41;
                    LOG_INFO("SHT41", "Successfully reconnected and tested - T:%.1f°C H:%.1f%%",
                            temperature.temperature, humidity.relative_humidity);
                    sensorStateChanged = true;
                } else {
                    LOG_WARN("SHT41", "Reconnected but test read failed - invalid values");
                    delete sht41;
                    sht41 = nullptr;
                }
            } else {
                LOG_WARN("SHT41", "Sensor detected but reinitialization failed");
                delete sht41;
                sht41 = nullptr;
            }
        }

        // Check BME280
        if (!bmePresent && checkI2CDevice(BME280_ADDRESS)) {
            LOG_INFO("BME280", "Sensor detected during periodic check, reinitializing...");
            bme280 = new Adafruit_BME280();

            bool init_success = false;
            for (int retry = 0; retry < 3; retry++) {
                if (bme280->begin(BME280_ADDRESS)) {
                    init_success = true;
                    break;
                }
                delay(200);
            }

            if (init_success) {
                // Try test read to verify sensor works
                float temp = bme280->readTemperature() + settings.bmeTempOffset;
                float hum = bme280->readHumidity() + settings.bmeHumidityOffset;
                float press = bme280->readPressure() / 100.0 + settings.bmePressureOffset;

                if (temp >= 0.0f && temp <= 50.0f &&
                    hum >= 10.0f && hum <= 100.0f &&
                    press >= 300.0f && press <= 1100.0f) {
                    // All steps successful - clear error flag and mark as present
                    bmePresent = true;
                    currentData.errorFlags &= ~ERR_BME280;
                    LOG_INFO("BME280", "Successfully reconnected and tested - T:%.1f°C H:%.1f%% P:%.1fhPa",
                            temp, hum, press);
                    sensorStateChanged = true;
                } else {
                    LOG_WARN("BME280", "Reconnected but test read failed - invalid values");
                    delete bme280;
                    bme280 = nullptr;
                }
            } else {
                LOG_WARN("BME280", "Sensor detected but reinitialization failed");
                delete bme280;
                bme280 = nullptr;
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
    const int inputPins[8] = {PIN_KOPALNICA_TIPKA, PIN_UTILITY_STIKALO, PIN_OKNO_SENZOR_1, PIN_OKNO_SENZOR_2,
                              PIN_KOPALNICA_LUC_1, PIN_KOPALNICA_LUC_2, PIN_UTILITY_LUC, PIN_WC_LUC};
    static int lastInputStates[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

    char logMessage[256];
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
            LOG_INFO("Inputs", "%s", event);
            lastInputStates[i] = currentState;
        }
    }
}
