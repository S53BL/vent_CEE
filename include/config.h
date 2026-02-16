// config.h

#ifndef CONFIG_H
#define CONFIG_H
#include <cstdint>
#include <Arduino.h>
// Ethernet pins for ESP32-S3-ETH (W5500)
#define ETH_MISO_PIN    12
#define ETH_MOSI_PIN    11
#define ETH_SCLK_PIN    13
#define ETH_CS_PIN      14
#define ETH_INT_PIN     10
#define ETH_RST_PIN     9
#define ETH_ADDR        1

// Static IP configuration
#define STATIC_IP       192,168,2,192
#define GATEWAY_IP      192,168,2,1
#define SUBNET_MASK     255,255,255,0
#define DNS_IP          192,168,2,1

// MAC address
#define MAC_ADDR        0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED

// Določitev, ali se nastavitve berejo iz NVS ali privzete iz config.h
const bool useNVS = true; // true za produkcijo
#define SETTINGS_MARKER 0xAD
#define REW_IP "192.168.2.190"
// Korekcija odčitkov vlage za kompenzacijo odstopanj senzorjev
#define BME_HUMIDITY_OFFSET 8.0  // Dodatek k vlagi BME280 (kopalnica) v odstotkih = 8.0
#define SHT_HUMIDITY_OFFSET -2.0 // Odvzem od vlage SHT41 (Utility) v odstotkih = -2.0
#define TIMG0_BASE           0x6001F000
#define TIMG0_WDTWPROTECT    (TIMG0_BASE + 0x0064)
#define TIMG0_WDTCONFIG0     (TIMG0_BASE + 0x0048)
#define TIMG1_BASE           0x60020000
#define TIMG1_WDTWPROTECT    (TIMG1_BASE + 0x0064)
#define TIMG1_WDTCONFIG0     (TIMG1_BASE + 0x0048)
#define RTC_CNTL_BASE        0x60008000
#define RTC_CNTL_WDTWPROTECT (RTC_CNTL_BASE + 0x010C)
#define RTC_CNTL_WDTCONFIG0  (RTC_CNTL_BASE + 0x0090)
#define WDT_WKEY             0x50D83AA1
#define PIN_KOPALNICA_ODVOD 42
#define PIN_UTILITY_ODVOD 41
#define PIN_WC_ODVOD 40
#define PIN_SKUPNI_VPIH 39
#define PIN_DNEVNI_VPIH 38
#define PIN_DNEVNI_ODVOD_1 37
#define PIN_DNEVNI_ODVOD_2 36
#define PIN_DNEVNI_ODVOD_3 35
#define PIN_KOPALNICA_TIPKA 34
#define PIN_UTILITY_STIKALO 33
#define PIN_OKNO_SENZOR_1 16
#define PIN_OKNO_SENZOR_2 17
#define PIN_KOPALNICA_LUC_1 43
#define PIN_KOPALNICA_LUC_2 44
#define PIN_UTILITY_LUC 15
#define PIN_WC_LUC 18
#define PIN_I2C_SDA 48
#define PIN_I2C_SCL 47
// SD card pins
#define SD_CS_PIN 4
#define SD_MOSI_PIN 6
#define SD_MISO_PIN 5
#define SD_SCK_PIN 7
#define BME280_ADDRESS 0x76
#define SHT41_ADDRESS 0x44
#define I2C_TIMEOUT_MS 50
#define SENSOR_READ_TIMEOUT_MS 200
#define LOG_REPEAT_INTERVAL 60000 // Omejitev ponovitev log sporočil (60 sekund)
#define SENSOR_TEST_INTERVAL 3600 // Interval za preverjanje senzorjev (sekunde)
#define STATUS_UPDATE_INTERVAL 300000UL // Interval za STATUS_UPDATE (5 minut v ms)
#define ERR_BME280 0x01
#define ERR_SHT41 0x02
#define ERR_DEW 0x08
#define ERR_TIME_SYNC 0x10
#define ERR_POWER 0x40
#define TZ_STRING "CET-1CEST,M3.5.0/2,M10.5.0/3"  // POSIX za CET

// Fan power consumption values (Watts)
#define FAN_POWER_BATHROOM 20.0
#define FAN_POWER_UTILITY 20.0
#define FAN_POWER_WC 20.0
#define FAN_POWER_COMMON_INTAKE 30.0
#define FAN_POWER_LIVING_INTAKE 30.0
#define FAN_POWER_LIVING_EXHAUST_1 120.0
#define FAN_POWER_LIVING_EXHAUST_2 150.0
#define FAN_POWER_LIVING_EXHAUST_3 200.0
#define NTP_UPDATE_INTERVAL 3600000UL  // 1 ura v ms
#define NTP_SERVER_COUNT 3
#define DND_START_HOUR 22
#define DND_START_MIN 0
#define DND_END_HOUR 6
#define DND_END_MIN 0
#define NND_START_HOUR 6
#define NND_START_MIN 0
#define NND_END_HOUR 16
#define NND_END_MIN 0
const bool NND_DAYS[7] = {true, true, true, true, true, false, false};
#define SERIAL_BAUD 115200
#define SENSOR_READ_INTERVAL 60
#define DATA_SAVE_INTERVAL 360
#define LOG_THRESHOLD_IDLE 10240  // 10kB - flush logs when idle
#define LOG_BUFFER_MAX 30720      // 30kB - force flush regardless of idle status
// Privzete vrednosti za struct so odstranjene - glej initDefaults() v globals.cpp
// To poenostavi vzdrževanje tovarniških nastavitev (samo eno mesto za spremembe)
struct Settings {
    float humThreshold;
    uint16_t fanDuration;
    uint16_t fanOffDuration;
    uint16_t fanOffDurationKop;
    float tempLowThreshold;
    float tempMinThreshold;
    bool dndAllowableAutomatic;
    bool dndAllowableSemiautomatic;
    bool dndAllowableManual;
    uint16_t cycleDurationDS;
    float cycleActivePercentDS;
    float humThresholdDS;
    float humThresholdHighDS;
    uint16_t co2ThresholdLowDS;
    uint16_t co2ThresholdHighDS;
    float incrementPercentLowDS;
    float incrementPercentHighDS;
    float incrementPercentTempDS;
    float tempIdealDS;
    float tempExtremeHighDS;
    float tempExtremeLowDS;
    float humExtremeHighDS;

    // Sensor offset settings for CEE
    float bmeTempOffset;        // BME280 temperature offset (°C)
    float bmeHumidityOffset;    // BME280 humidity offset (%)
    float bmePressureOffset;    // BME280 pressure offset (hPa)
    float shtTempOffset;        // SHT41 temperature offset (°C)
    float shtHumidityOffset;    // SHT41 humidity offset (%)

    // Reserved for future sensor offsets
    float reservedSensor1;
    float reservedSensor2;

    uint32_t lastKnownUnixTime;
} __attribute__((packed));
struct ExternalData {
    float externalTemperature;
    float externalHumidity;
    float externalPressure;
    float externalLight;
    float livingTempDS;
    float livingHumidityDS;
    uint16_t livingCO2;
    int weatherIcon;
    int seasonCode;
    uint32_t timestamp;
};
struct CurrentData {
    float externalTemp;
    float externalHumidity;
    float externalPressure;
    float externalLight;
    float livingTemp;
    float livingHumidity;
    uint16_t livingCO2;
    float bathroomTemp;
    float bathroomHumidity;
    float bathroomPressure;
    float utilityTemp;
    float utilityHumidity;
    bool bathroomFan;
    bool utilityFan;
    bool wcFan;
    bool commonIntake;
    bool livingIntake;
    uint8_t livingExhaustLevel;
    float currentPower;
    float energyConsumption;
    float livingRoomDutyCycle; // Current duty cycle percentage for living room
    bool bathroomButton;
    bool utilitySwitch;
    bool windowSensor1;
    bool windowSensor2;
    bool bathroomLight1;
    bool bathroomLight2;
    bool utilityLight;
    bool wcLight;
    bool manualTriggerWC;
    bool disableBathroom;
    bool manualTriggerBathroom;
    bool manualTriggerBathroomDrying;
    bool manualTriggerUtility;
    bool manualTriggerUtilityDrying;
    bool disableUtility;
    bool disableLivingRoom;
    bool manualTriggerLivingRoom;
    bool disableWc;
    uint8_t errorFlags;
    uint8_t dewError;
    uint32_t timestamp;
    uint32_t offTimes[6]; // Dodano za STATUS_UPDATE: časi izklopa ventilatorjev
    uint8_t previousFans[6];
    uint8_t previousInputs[8];
    unsigned long lastStatusUpdateTime;
    float supply5V;      // izmerjena 5 V napetost (po korekciji)
    float supply3V3;     // izmerjena 3,3 V napetost (po korekciji)
    // Drying mode status
    bool utilityDryingMode;
    bool bathroomDryingMode;
    int utilityCycleMode;
    int bathroomCycleMode;
    time_t utilityExpectedEndTime;
    time_t bathroomExpectedEndTime;
};
#endif
// config.h
