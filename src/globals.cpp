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

void initDefaults() {
  settings.humThreshold = 60.0f;
  settings.fanDuration = 180;
  settings.fanOffDuration = 1200;
  settings.fanOffDurationKop = 1200;
  settings.tempLowThreshold = 5.0f;
  settings.tempMinThreshold = -10.0f;
  settings.dndAllowableAutomatic = false;
  settings.dndAllowableSemiautomatic = false;
  settings.dndAllowableManual = true;
  settings.cycleDurationDS = 60;
  settings.cycleActivePercentDS = 30.0f;
  settings.humThresholdDS = 60.0f;
  settings.humThresholdHighDS = 70.0f;
  settings.co2ThresholdLowDS = 900;
  settings.co2ThresholdHighDS = 1200;
  settings.incrementPercentLowDS = 15.0f;
  settings.incrementPercentHighDS = 50.0f;
  settings.incrementPercentTempDS = 20.0f;
  settings.tempIdealDS = 24.0f;
  settings.tempExtremeHighDS = 30.0f;
  settings.tempExtremeLowDS = -10.0f;
  settings.humExtremeHighDS = 80.0f;

  // Sensor offset defaults
  settings.bmeTempOffset = 0.0f;
  settings.bmeHumidityOffset = 0.0f;
  settings.bmePressureOffset = 0.0f;
  settings.shtTempOffset = 0.0f;
  settings.shtHumidityOffset = 0.0f;
  settings.reservedSensor1 = 0.0f;
  settings.reservedSensor2 = 0.0f;

  settings.lastKnownUnixTime = 0;

  // Inicializacija currentData
  currentData.supply5V = 0.0f;
  currentData.supply3V3 = 0.0f;
}

void loadSettings() {
  if (!useNVS) {
    initDefaults();
    LOG_INFO("Settings", "NVS onemogočen, uporabljene privzete nastavitve");
    return;
  }

  Preferences prefs;
  prefs.begin("vent", true);

  // Preveri marker za validacijo podatkov
  uint8_t marker = prefs.getUChar("marker", 0);
  if (marker != SETTINGS_MARKER) {
    LOG_INFO("Settings", "Marker neveljaven (prebran: 0x%02X, pričakovan: 0x%02X) - uporabljene privzete nastavitve, shranjene v NVS", marker, SETTINGS_MARKER);
    prefs.end();
    initDefaults();
    saveSettings();
    return;
  }

  // Preberi nastavitve iz NVS
  settings.humThreshold = prefs.getFloat("humThreshold", 60.0f);
  settings.fanDuration = prefs.getUInt("fanDuration", 180);
  settings.fanOffDuration = prefs.getUInt("fanOffDuration", 1200);
  settings.fanOffDurationKop = prefs.getUInt("fanOffDurationKop", 1200);
  settings.tempLowThreshold = prefs.getFloat("tempLowThreshold", 5.0f);
  settings.tempMinThreshold = prefs.getFloat("tempMinThreshold", -10.0f);
  settings.dndAllowableAutomatic = prefs.getUChar("dndAllowableAutomatic", 1) != 0;
  settings.dndAllowableSemiautomatic = prefs.getUChar("dndAllowableSemiautomatic", 1) != 0;
  settings.dndAllowableManual = prefs.getUChar("dndAllowableManual", 1) != 0;
  settings.cycleDurationDS = prefs.getUInt("cycleDurationDS", 60);
  settings.cycleActivePercentDS = prefs.getFloat("cycleActivePercentDS", 30.0f);
  settings.humThresholdDS = prefs.getFloat("humThresholdDS", 60.0f);
  settings.humThresholdHighDS = prefs.getFloat("humThresholdHighDS", 70.0f);
  settings.co2ThresholdLowDS = prefs.getUInt("co2ThresholdLowDS", 900);
  settings.co2ThresholdHighDS = prefs.getUInt("co2ThresholdHighDS", 1200);
  settings.incrementPercentLowDS = prefs.getFloat("incrementPercentLowDS", 15.0f);
  settings.incrementPercentHighDS = prefs.getFloat("incrementPercentHighDS", 50.0f);
  settings.incrementPercentTempDS = prefs.getFloat("incrementPercentTempDS", 20.0f);
  settings.tempIdealDS = prefs.getFloat("tempIdealDS", 24.0f);
  settings.tempExtremeHighDS = prefs.getFloat("tempExtremeHighDS", 30.0f);
  settings.tempExtremeLowDS = prefs.getFloat("tempExtremeLowDS", -10.0f);
  settings.humExtremeHighDS = prefs.getFloat("humExtremeHighDS", 80.0f);

  // Load sensor offsets
  settings.bmeTempOffset = prefs.getFloat("bmeTempOffset", 0.0f);
  settings.bmeHumidityOffset = prefs.getFloat("bmeHumidityOffset", 0.0f);
  settings.bmePressureOffset = prefs.getFloat("bmePressureOffset", 0.0f);
  settings.shtTempOffset = prefs.getFloat("shtTempOffset", 0.0f);
  settings.shtHumidityOffset = prefs.getFloat("shtHumidityOffset", 0.0f);
  settings.reservedSensor1 = prefs.getFloat("reservedSensor1", 0.0f);
  settings.reservedSensor2 = prefs.getFloat("reservedSensor2", 0.0f);

  settings.lastKnownUnixTime = prefs.getULong("lastKnownUnixTime", 0);

  // Preberi shranjen CRC
  uint16_t stored_crc = prefs.getUShort("settings_crc", 0);
  prefs.end();

  // Izračunaj CRC prebranih nastavitev (na podlagi posameznih polj, ne struct memory)
  // POZOR: Če se doda nova polja v Settings strukturo, jih je potrebno dodati tudi tukaj
  // in v saveSettings() funkciji v ISTEM VRSTNEM REDU!
  uint8_t crcData[sizeof(Settings)];
  memset(crcData, 0, sizeof(crcData));
  size_t offset = 0;
  memcpy(crcData + offset, &settings.humThreshold, sizeof(settings.humThreshold)); offset += sizeof(settings.humThreshold);
  memcpy(crcData + offset, &settings.fanDuration, sizeof(settings.fanDuration)); offset += sizeof(settings.fanDuration);
  memcpy(crcData + offset, &settings.fanOffDuration, sizeof(settings.fanOffDuration)); offset += sizeof(settings.fanOffDuration);
  memcpy(crcData + offset, &settings.fanOffDurationKop, sizeof(settings.fanOffDurationKop)); offset += sizeof(settings.fanOffDurationKop);
  memcpy(crcData + offset, &settings.tempLowThreshold, sizeof(settings.tempLowThreshold)); offset += sizeof(settings.tempLowThreshold);
  memcpy(crcData + offset, &settings.tempMinThreshold, sizeof(settings.tempMinThreshold)); offset += sizeof(settings.tempMinThreshold);
  memcpy(crcData + offset, &settings.dndAllowableAutomatic, sizeof(settings.dndAllowableAutomatic)); offset += sizeof(settings.dndAllowableAutomatic);
  memcpy(crcData + offset, &settings.dndAllowableSemiautomatic, sizeof(settings.dndAllowableSemiautomatic)); offset += sizeof(settings.dndAllowableSemiautomatic);
  memcpy(crcData + offset, &settings.dndAllowableManual, sizeof(settings.dndAllowableManual)); offset += sizeof(settings.dndAllowableManual);
  memcpy(crcData + offset, &settings.cycleDurationDS, sizeof(settings.cycleDurationDS)); offset += sizeof(settings.cycleDurationDS);
  memcpy(crcData + offset, &settings.cycleActivePercentDS, sizeof(settings.cycleActivePercentDS)); offset += sizeof(settings.cycleActivePercentDS);
  memcpy(crcData + offset, &settings.humThresholdDS, sizeof(settings.humThresholdDS)); offset += sizeof(settings.humThresholdDS);
  memcpy(crcData + offset, &settings.humThresholdHighDS, sizeof(settings.humThresholdHighDS)); offset += sizeof(settings.humThresholdHighDS);
  memcpy(crcData + offset, &settings.co2ThresholdLowDS, sizeof(settings.co2ThresholdLowDS)); offset += sizeof(settings.co2ThresholdLowDS);
  memcpy(crcData + offset, &settings.co2ThresholdHighDS, sizeof(settings.co2ThresholdHighDS)); offset += sizeof(settings.co2ThresholdHighDS);
  memcpy(crcData + offset, &settings.incrementPercentLowDS, sizeof(settings.incrementPercentLowDS)); offset += sizeof(settings.incrementPercentLowDS);
  memcpy(crcData + offset, &settings.incrementPercentHighDS, sizeof(settings.incrementPercentHighDS)); offset += sizeof(settings.incrementPercentHighDS);
  memcpy(crcData + offset, &settings.incrementPercentTempDS, sizeof(settings.incrementPercentTempDS)); offset += sizeof(settings.incrementPercentTempDS);
  memcpy(crcData + offset, &settings.tempIdealDS, sizeof(settings.tempIdealDS)); offset += sizeof(settings.tempIdealDS);
  memcpy(crcData + offset, &settings.tempExtremeHighDS, sizeof(settings.tempExtremeHighDS)); offset += sizeof(settings.tempExtremeHighDS);
  memcpy(crcData + offset, &settings.tempExtremeLowDS, sizeof(settings.tempExtremeLowDS)); offset += sizeof(settings.tempExtremeLowDS);
  memcpy(crcData + offset, &settings.humExtremeHighDS, sizeof(settings.humExtremeHighDS)); offset += sizeof(settings.humExtremeHighDS);

  // Add sensor offsets to CRC
  memcpy(crcData + offset, &settings.bmeTempOffset, sizeof(settings.bmeTempOffset)); offset += sizeof(settings.bmeTempOffset);
  memcpy(crcData + offset, &settings.bmeHumidityOffset, sizeof(settings.bmeHumidityOffset)); offset += sizeof(settings.bmeHumidityOffset);
  memcpy(crcData + offset, &settings.bmePressureOffset, sizeof(settings.bmePressureOffset)); offset += sizeof(settings.bmePressureOffset);
  memcpy(crcData + offset, &settings.shtTempOffset, sizeof(settings.shtTempOffset)); offset += sizeof(settings.shtTempOffset);
  memcpy(crcData + offset, &settings.shtHumidityOffset, sizeof(settings.shtHumidityOffset)); offset += sizeof(settings.shtHumidityOffset);
  memcpy(crcData + offset, &settings.reservedSensor1, sizeof(settings.reservedSensor1)); offset += sizeof(settings.reservedSensor1);
  memcpy(crcData + offset, &settings.reservedSensor2, sizeof(settings.reservedSensor2)); offset += sizeof(settings.reservedSensor2);

  memcpy(crcData + offset, &settings.lastKnownUnixTime, sizeof(settings.lastKnownUnixTime)); offset += sizeof(settings.lastKnownUnixTime);

  uint16_t calculated_crc = calculateCRC(crcData, offset);

  LOG_INFO("Settings", "Prebran shranjen CRC: 0x%04X", stored_crc);
  LOG_INFO("Settings", "Izračunan CRC: 0x%04X", calculated_crc);

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
  Preferences prefs;
  prefs.begin("vent", false);
  // Shrani marker za validacijo
  prefs.putUChar("marker", SETTINGS_MARKER);
  prefs.putFloat("humThreshold", settings.humThreshold);
  prefs.putUInt("fanDuration", settings.fanDuration);
  prefs.putUInt("fanOffDuration", settings.fanOffDuration);
  prefs.putUInt("fanOffDurationKop", settings.fanOffDurationKop);
  prefs.putFloat("tempLowThreshold", settings.tempLowThreshold);
  prefs.putFloat("tempMinThreshold", settings.tempMinThreshold);
  prefs.putUChar("dndAllowableAutomatic", settings.dndAllowableAutomatic ? 1 : 0);
  prefs.putUChar("dndAllowableSemiautomatic", settings.dndAllowableSemiautomatic ? 1 : 0);
  prefs.putUChar("dndAllowableManual", settings.dndAllowableManual ? 1 : 0);
  prefs.putUInt("cycleDurationDS", settings.cycleDurationDS);
  prefs.putFloat("cycleActivePercentDS", settings.cycleActivePercentDS);
  prefs.putFloat("humThresholdDS", settings.humThresholdDS);
  prefs.putFloat("humThresholdHighDS", settings.humThresholdHighDS);
  prefs.putUInt("co2ThresholdLowDS", settings.co2ThresholdLowDS);
  prefs.putUInt("co2ThresholdHighDS", settings.co2ThresholdHighDS);
  prefs.putFloat("incrementPercentLowDS", settings.incrementPercentLowDS);
  prefs.putFloat("incrementPercentHighDS", settings.incrementPercentHighDS);
  prefs.putFloat("incrementPercentTempDS", settings.incrementPercentTempDS);
  prefs.putFloat("tempIdealDS", settings.tempIdealDS);
  prefs.putFloat("tempExtremeHighDS", settings.tempExtremeHighDS);
  prefs.putFloat("tempExtremeLowDS", settings.tempExtremeLowDS);
  prefs.putFloat("humExtremeHighDS", settings.humExtremeHighDS);

  // Save sensor offsets
  prefs.putFloat("bmeTempOffset", settings.bmeTempOffset);
  prefs.putFloat("bmeHumidityOffset", settings.bmeHumidityOffset);
  prefs.putFloat("bmePressureOffset", settings.bmePressureOffset);
  prefs.putFloat("shtTempOffset", settings.shtTempOffset);
  prefs.putFloat("shtHumidityOffset", settings.shtHumidityOffset);
  prefs.putFloat("reservedSensor1", settings.reservedSensor1);
  prefs.putFloat("reservedSensor2", settings.reservedSensor2);

  prefs.putULong("lastKnownUnixTime", settings.lastKnownUnixTime);

  // Izračunaj in shrani CRC za validacijo (na podlagi posameznih polj, ne struct memory)
  // POZOR: Če se doda nova polja v Settings strukturo, jih je potrebno dodati tudi tukaj
  // in v loadSettings() funkciji v ISTEM VRSTNEM REDU!
  uint8_t crcData[sizeof(Settings)];
  memset(crcData, 0, sizeof(crcData));
  size_t offset = 0;
  memcpy(crcData + offset, &settings.humThreshold, sizeof(settings.humThreshold)); offset += sizeof(settings.humThreshold);
  memcpy(crcData + offset, &settings.fanDuration, sizeof(settings.fanDuration)); offset += sizeof(settings.fanDuration);
  memcpy(crcData + offset, &settings.fanOffDuration, sizeof(settings.fanOffDuration)); offset += sizeof(settings.fanOffDuration);
  memcpy(crcData + offset, &settings.fanOffDurationKop, sizeof(settings.fanOffDurationKop)); offset += sizeof(settings.fanOffDurationKop);
  memcpy(crcData + offset, &settings.tempLowThreshold, sizeof(settings.tempLowThreshold)); offset += sizeof(settings.tempLowThreshold);
  memcpy(crcData + offset, &settings.tempMinThreshold, sizeof(settings.tempMinThreshold)); offset += sizeof(settings.tempMinThreshold);
  memcpy(crcData + offset, &settings.dndAllowableAutomatic, sizeof(settings.dndAllowableAutomatic)); offset += sizeof(settings.dndAllowableAutomatic);
  memcpy(crcData + offset, &settings.dndAllowableSemiautomatic, sizeof(settings.dndAllowableSemiautomatic)); offset += sizeof(settings.dndAllowableSemiautomatic);
  memcpy(crcData + offset, &settings.dndAllowableManual, sizeof(settings.dndAllowableManual)); offset += sizeof(settings.dndAllowableManual);
  memcpy(crcData + offset, &settings.cycleDurationDS, sizeof(settings.cycleDurationDS)); offset += sizeof(settings.cycleDurationDS);
  memcpy(crcData + offset, &settings.cycleActivePercentDS, sizeof(settings.cycleActivePercentDS)); offset += sizeof(settings.cycleActivePercentDS);
  memcpy(crcData + offset, &settings.humThresholdDS, sizeof(settings.humThresholdDS)); offset += sizeof(settings.humThresholdDS);
  memcpy(crcData + offset, &settings.humThresholdHighDS, sizeof(settings.humThresholdHighDS)); offset += sizeof(settings.humThresholdHighDS);
  memcpy(crcData + offset, &settings.co2ThresholdLowDS, sizeof(settings.co2ThresholdLowDS)); offset += sizeof(settings.co2ThresholdLowDS);
  memcpy(crcData + offset, &settings.co2ThresholdHighDS, sizeof(settings.co2ThresholdHighDS)); offset += sizeof(settings.co2ThresholdHighDS);
  memcpy(crcData + offset, &settings.incrementPercentLowDS, sizeof(settings.incrementPercentLowDS)); offset += sizeof(settings.incrementPercentLowDS);
  memcpy(crcData + offset, &settings.incrementPercentHighDS, sizeof(settings.incrementPercentHighDS)); offset += sizeof(settings.incrementPercentHighDS);
  memcpy(crcData + offset, &settings.incrementPercentTempDS, sizeof(settings.incrementPercentTempDS)); offset += sizeof(settings.incrementPercentTempDS);
  memcpy(crcData + offset, &settings.tempIdealDS, sizeof(settings.tempIdealDS)); offset += sizeof(settings.tempIdealDS);
  memcpy(crcData + offset, &settings.tempExtremeHighDS, sizeof(settings.tempExtremeHighDS)); offset += sizeof(settings.tempExtremeHighDS);
  memcpy(crcData + offset, &settings.tempExtremeLowDS, sizeof(settings.tempExtremeLowDS)); offset += sizeof(settings.tempExtremeLowDS);
  memcpy(crcData + offset, &settings.humExtremeHighDS, sizeof(settings.humExtremeHighDS)); offset += sizeof(settings.humExtremeHighDS);

  // Add sensor offsets to CRC
  memcpy(crcData + offset, &settings.bmeTempOffset, sizeof(settings.bmeTempOffset)); offset += sizeof(settings.bmeTempOffset);
  memcpy(crcData + offset, &settings.bmeHumidityOffset, sizeof(settings.bmeHumidityOffset)); offset += sizeof(settings.bmeHumidityOffset);
  memcpy(crcData + offset, &settings.bmePressureOffset, sizeof(settings.bmePressureOffset)); offset += sizeof(settings.bmePressureOffset);
  memcpy(crcData + offset, &settings.shtTempOffset, sizeof(settings.shtTempOffset)); offset += sizeof(settings.shtTempOffset);
  memcpy(crcData + offset, &settings.shtHumidityOffset, sizeof(settings.shtHumidityOffset)); offset += sizeof(settings.shtHumidityOffset);
  memcpy(crcData + offset, &settings.reservedSensor1, sizeof(settings.reservedSensor1)); offset += sizeof(settings.reservedSensor1);
  memcpy(crcData + offset, &settings.reservedSensor2, sizeof(settings.reservedSensor2)); offset += sizeof(settings.reservedSensor2);

  memcpy(crcData + offset, &settings.lastKnownUnixTime, sizeof(settings.lastKnownUnixTime)); offset += sizeof(settings.lastKnownUnixTime);

  uint16_t crc = calculateCRC(crcData, offset);
  prefs.putUShort("settings_crc", crc);

  prefs.end();
  LOG_INFO("Settings", "Shranjen nov CRC: 0x%04X", crc);
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
