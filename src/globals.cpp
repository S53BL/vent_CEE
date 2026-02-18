// globals.cpp

#include "globals.h"
#include <Preferences.h>
#include <ezTime.h>
#include <cstring>
#include "logging.h"

// NTP servers definition - internet servers only
const char* ntpServers[] = {"pool.ntp.org", "time.nist.gov", "time.google.com"};

// Time management globals
Timezone myTZ;
bool timeSynced = false;

// External data validation
bool externalDataValid = false;
uint32_t lastSensorDataTime = 0;

// Device status tracking
DeviceStatus rewStatus = {false};
DeviceStatus utDewStatus = {false};
DeviceStatus kopDewStatus = {false};

ExternalData externalData;
Settings settings;
CurrentData currentData;
String currentWeatherIcon = "";
int currentSeasonCode = 0;

Adafruit_BME280 *bme280 = nullptr;
Adafruit_SHT4x *sht41 = nullptr;
bool bmePresent = false;
bool sht41Present = false;
unsigned long lastSensorRead = 0;

// Logging globals
String logBuffer = "";
bool loggingInitialized = false;
unsigned long lastLogFlush = 0;

uint16_t calculateCRC(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void hexdumpCRCData(const uint8_t* data, size_t len, const char* label) {
  LOG_INFO("Hexdump", "=== %s ===", label);
  for (size_t i = 0; i < len; i += 16) {
    char line[80];
    sprintf(line, "%04X: ", (uint16_t)i);
    for (size_t j = 0; j < 16 && i + j < len; j++) {
      sprintf(line + strlen(line), "%02X ", data[i + j]);
    }
    LOG_INFO("Hexdump", "%s", line);
  }
  LOG_INFO("Hexdump", "=== END %s ===", label);
}

void initCurrentData() {
  // Inicializacija currentData na privzete vrednosti
  currentData.supply5V = 0.0f;
  currentData.supply3V3 = 0.0f;
  currentData.livingCO2 = 0;
  currentData.livingTemp = 0.0f;
  currentData.livingHumidity = 0.0f;
  currentData.externalTemp = 0.0f;
  currentData.externalHumidity = 0.0f;
  currentData.externalPressure = 0.0f;
  currentData.externalLight = 0.0f;
  currentData.bathroomTemp = 0.0f;
  currentData.bathroomHumidity = 0.0f;
  currentData.bathroomPressure = 0.0f;
  currentData.utilityTemp = 0.0f;
  currentData.utilityHumidity = 0.0f;
  currentData.currentPower = 0.0f;
  currentData.energyConsumption = 0.0f;
  currentData.livingRoomDutyCycle = 0.0f;
  currentData.livingExhaustLevel = 0;
  currentData.bathroomFan = false;
  currentData.utilityFan = false;
  currentData.wcFan = false;
  currentData.commonIntake = false;
  currentData.livingIntake = false;
  currentData.bathroomButton = false;
  currentData.utilitySwitch = false;
  currentData.windowSensor1 = false;
  currentData.windowSensor2 = false;
  currentData.bathroomLight1 = false;
  currentData.bathroomLight2 = false;
  currentData.utilityLight = false;
  currentData.wcLight = false;
  currentData.manualTriggerWC = false;
  currentData.disableBathroom = false;
  currentData.manualTriggerBathroom = false;
  currentData.manualTriggerBathroomDrying = false;
  currentData.manualTriggerUtility = false;
  currentData.manualTriggerUtilityDrying = false;
  currentData.disableUtility = false;
  currentData.disableLivingRoom = false;
  currentData.manualTriggerLivingRoom = false;
  currentData.disableWc = false;
  currentData.errorFlags = 0;
  currentData.dewError = 0;
  currentData.timestamp = 0;
  currentData.utilityDryingMode = false;
  currentData.bathroomDryingMode = false;
  currentData.utilityCycleMode = 0;
  currentData.bathroomCycleMode = 0;
  
  for (int i = 0; i < 6; i++) {
    currentData.offTimes[i] = 0;
    currentData.previousFans[i] = 0;
  }
  for (int i = 0; i < 8; i++) {
    currentData.previousInputs[i] = 0;
  }
}

void initDefaults() {
  // Tovarniške nastavitve - prenešene iz config.h za poenostavitev vzdrževanja
  settings.humThreshold = 65.0f;
  settings.fanDuration = 180;
  settings.fanOffDuration = 1800;
  settings.fanOffDurationKop = 180;
  settings.tempLowThreshold = 5.0f;
  settings.tempMinThreshold = -10.0f;
  settings.dndAllowableAutomatic = true;
  settings.dndAllowableSemiautomatic = true;
  settings.dndAllowableManual = true;
  settings.cycleDurationDS = 2000;
  settings.cycleActivePercentDS = 9.0f;
  settings.humThresholdDS = 60.0f;
  settings.humThresholdHighDS = 70.0f;
  settings.co2ThresholdLowDS = 900;
  settings.co2ThresholdHighDS = 1200;
  settings.incrementPercentLowDS = 5.0f;
  settings.incrementPercentHighDS = 20.0f;
  settings.incrementPercentTempDS = 10.0f;
  settings.tempIdealDS = 23.0f;
  settings.tempExtremeHighDS = 30.0f;
  settings.tempExtremeLowDS = -7.0f;
  settings.humExtremeHighDS = 85.0f;

  // Sensor offset defaults
  settings.bmeTempOffset = 0.0f;
  settings.bmeHumidityOffset = 0.0f;
  settings.bmePressureOffset = 0.0f;
  settings.shtTempOffset = 0.0f;
  settings.shtHumidityOffset = 0.0f;
  settings.reservedSensor1 = 0.0f;
  settings.reservedSensor2 = 0.0f;

  settings.lastKnownUnixTime = 0;

  // Debug: izpis privzetih vrednosti
  LOG_INFO("Settings", "Defaults: co2ThresholdLowDS=%d, co2ThresholdHighDS=%d", 
           settings.co2ThresholdLowDS, settings.co2ThresholdHighDS);
}

void loadSettings() {
  if (!useNVS) {
    initDefaults();
    LOG_INFO("Settings", "NVS onemogočen, uporabljene privzete nastavitve");
    return;
  }

  Preferences prefs;
  prefs.begin("settings", true);

  // Preveri marker za validacijo podatkov
  uint8_t marker = prefs.getUChar("marker", 0x00);
  if (marker != SETTINGS_MARKER) {
    LOG_INFO("Settings", "Marker neveljaven (prebran: 0x%02X, pričakovan: 0x%02X) - uporabljene privzete nastavitve, shranjene v NVS", marker, SETTINGS_MARKER);
    prefs.end();
    initDefaults();
    saveSettings();
    return;
  }

  // Preberi nastavitve kot blob
  size_t bytesRead = prefs.getBytes("settings", (uint8_t*)&settings, sizeof(Settings));
  uint16_t stored_crc = prefs.getUShort("settings_crc", 0);
  prefs.end();

  if (bytesRead != sizeof(Settings)) {
    LOG_WARN("Settings", "Neveljavna velikost podatkov (%d != %d), uporabljene privzete nastavitve", bytesRead, sizeof(Settings));
    initDefaults();
    saveSettings();
    return;
  }

  // Izračunaj CRC
  uint16_t calculated_crc = calculateCRC((const uint8_t*)&settings, sizeof(Settings));

  if (calculated_crc != stored_crc) {
    LOG_WARN("Settings", "CRC neustreza (izračunan: 0x%04X, shranjen: 0x%04X) - naložene in shranjene privzete nastavitve v NVS z novim CRC-jem", calculated_crc, stored_crc);
    initDefaults();
    saveSettings();
  } else {
    LOG_INFO("Settings", "Nastavitve uspešno naložene iz NVS-a (CRC: 0x%04X)", calculated_crc);
  }
}

void saveSettings() {
  if (!useNVS) return;

  LOG_INFO("Settings", "Shranjujem nastavitve");

  Preferences prefs;
  prefs.begin("settings", false);

  // Shrani marker
  prefs.putUChar("marker", SETTINGS_MARKER);

  // Shrani nastavitve kot blob
  prefs.putBytes("settings", (const uint8_t*)&settings, sizeof(Settings));

  // Izračunaj in shrani CRC
  uint16_t crc = calculateCRC((const uint8_t*)&settings, sizeof(Settings));
  prefs.putUShort("settings_crc", crc);

  prefs.end();

  LOG_INFO("Settings", "Shranjen CRC: 0x%04X", crc);
}

bool isIdle(void) {
  // Check if WC, KOP, UT fans and lights are all off
  return (currentData.wcFan == false &&
          currentData.bathroomFan == false &&
          currentData.utilityFan == false &&
          currentData.wcLight == false &&
          currentData.bathroomLight1 == false &&
          currentData.bathroomLight2 == false &&
          currentData.utilityLight == false);
}
