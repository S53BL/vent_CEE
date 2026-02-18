// globals.h

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SHT4x.h>
#include <ezTime.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

extern ExternalData externalData;
extern Settings settings;
extern CurrentData currentData;
extern String currentWeatherIcon;
extern int currentSeasonCode;

void loadSettings();
void saveSettings();
uint16_t calculateCRC(const uint8_t* data, size_t len);
void initDefaults();
void initCurrentData();
bool isIdle(void);

extern Adafruit_BME280 *bme280;
extern Adafruit_SHT4x *sht41;
extern bool bmePresent;
extern bool sht41Present;
extern unsigned long lastSensorRead;

// Logging globals
extern String logBuffer;
extern bool loggingInitialized;
extern unsigned long lastLogFlush;

// Time management globals
extern Timezone myTZ;
extern bool timeSynced;
extern const char* ntpServers[];

// External data validation
extern bool externalDataValid;
extern uint32_t lastSensorDataTime;

// Device status tracking
struct DeviceStatus {
    bool isOnline;
};

extern DeviceStatus rewStatus;
extern DeviceStatus utDewStatus;
extern DeviceStatus kopDewStatus;

// I2C mutex for thread-safe operations
extern SemaphoreHandle_t i2cMutex;

#endif // GLOBALS_H
