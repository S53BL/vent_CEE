// http.cpp - HTTP client functions for sending messages

#include "http.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "logging.h"
#include "config.h"
#include "vent.h"
#include "message_fields.h"

// URLs for different units
#define REW_URL "http://192.168.2.190"
#define DEW_UT_URL "http://192.168.2.193"
#define DEW_KOP_URL "http://192.168.2.194"

// Helper: Compute fan states — shared between sendStatusUpdate and checkAndSendStatusUpdate
// Eliminates code duplication and keeps both functions in sync
struct FanStates {
    uint8_t fwc, fut, fkop, fdse;
};

FanStates computeFanStates() {
    FanStates s;
    s.fwc = currentData.disableWc ? 9 : (currentData.wcFan ? 1 : 0);
    if (currentData.disableUtility) {
        s.fut = 9;
    } else if (currentData.utilityDryingMode && currentData.utilityCycleMode >= 1 && currentData.utilityCycleMode <= 3) {
        s.fut = 5 + currentData.utilityCycleMode;  // 6=mode1, 7=mode2, 8=mode3
    } else {
        s.fut = currentData.utilityFan ? 1 : 0;
    }
    if (currentData.disableBathroom) {
        s.fkop = 9;
    } else if (currentData.bathroomDryingMode && currentData.bathroomCycleMode >= 1 && currentData.bathroomCycleMode <= 3) {
        s.fkop = 5 + currentData.bathroomCycleMode;  // 6=mode1, 7=mode2, 8=mode3
    } else {
        s.fkop = currentData.bathroomFan ? 1 : 0;
    }
    s.fdse = currentData.disableLivingRoom ? 9 : currentData.livingExhaustLevel;
    return s;
}

// Send STATUS_UPDATE to REW
bool sendStatusUpdate() {
    String url = String(REW_URL) + "/api/status-update";

    DynamicJsonDocument doc(512);

    // Fans — via shared helper (0=off, 1=on, 6-8=drying mode, 9=disabled)
    FanStates fs = computeFanStates();
    doc[FIELD_FAN_WC]         = fs.fwc;
    doc[FIELD_FAN_UTILITY]    = fs.fut;
    doc[FIELD_FAN_BATHROOM]   = fs.fkop;
    doc[FIELD_FAN_LIVING_EXH] = fs.fdse;

    // Inputs (digital states)
    doc[FIELD_INPUT_BATHROOM_L1] = currentData.bathroomLight1 ? 1 : 0;
    doc[FIELD_INPUT_BATHROOM_L2] = currentData.bathroomLight2 ? 1 : 0;
    doc[FIELD_INPUT_UTILITY_L]   = currentData.utilityLight ? 1 : 0;
    doc[FIELD_INPUT_WC_L]        = currentData.wcLight ? 1 : 0;
    doc[FIELD_INPUT_WINDOW_ROOF] = currentData.windowSensor1 ? 1 : 0;
    doc[FIELD_INPUT_WINDOW_BALC] = currentData.windowSensor2 ? 1 : 0;

    // Off-times (Unix timestamps)
    doc[FIELD_TIME_WC] = currentData.offTimes[2];
    if (currentData.utilityDryingMode && currentData.utilityCycleMode >= 1 && currentData.utilityCycleMode <= 3) {
        doc[FIELD_TIME_UTILITY] = currentData.utilityExpectedEndTime;
    } else {
        doc[FIELD_TIME_UTILITY] = currentData.offTimes[1];
    }
    if (currentData.bathroomDryingMode && currentData.bathroomCycleMode >= 1 && currentData.bathroomCycleMode <= 3) {
        doc[FIELD_TIME_BATHROOM] = currentData.bathroomExpectedEndTime;
    } else {
        doc[FIELD_TIME_BATHROOM] = currentData.offTimes[0];
    }
    doc[FIELD_TIME_LIVING_EXH] = currentData.offTimes[4];

    // Error flags (0=ok, 1=error)
    doc[FIELD_ERROR_BME280] = currentData.errorFlags & ERR_BME280 ? 1 : 0;
    doc[FIELD_ERROR_SHT41]  = currentData.errorFlags & ERR_SHT41 ? 1 : 0;
    doc[FIELD_ERROR_POWER]  = currentData.errorFlags & ERR_POWER ? 1 : 0;
    doc[FIELD_ERROR_DEW]    = currentData.dewError;
    // Time sync error detection
    uint8_t timeSyncError = 0;
    if (!timeSynced) {
        timeSyncError = 1;  // NTP not synchronized
    } else if (externalDataValid) {
        uint32_t currentTime = myTZ.now();
        uint32_t timeDiff = abs((int32_t)(currentTime - externalData.timestamp));
        if (timeDiff > 300) timeSyncError = 2;  // Time discrepancy >5min
    }
    doc[FIELD_ERROR_TIME_SYNC] = timeSyncError;

    // Sensor data
    doc[FIELD_TEMP_BATHROOM]      = currentData.bathroomTemp;
    doc[FIELD_HUM_BATHROOM]       = currentData.bathroomHumidity;
    doc[FIELD_PRESS_BATHROOM]     = currentData.bathroomPressure;
    doc[FIELD_TEMP_UTILITY]       = currentData.utilityTemp;
    doc[FIELD_HUM_UTILITY]        = currentData.utilityHumidity;
    doc[FIELD_CURRENT_POWER]      = currentData.currentPower;
    doc[FIELD_ENERGY_CONSUMPTION] = currentData.energyConsumption;
    doc[FIELD_DUTY_CYCLE_LIVING]  = (int)currentData.livingRoomDutyCycle;

    String jsonString;
    serializeJson(doc, jsonString);

    // Structured logging
    LOG_INFO("HTTP", "STATUS_UPDATE FAN: fwc=%d fut=%d fkop=%d fdse=%d", (int)doc[FIELD_FAN_WC], (int)doc[FIELD_FAN_UTILITY], (int)doc[FIELD_FAN_BATHROOM], (int)doc[FIELD_FAN_LIVING_EXH]);
    LOG_INFO("HTTP", "S_U INPUT: il1=%d il2=%d iul=%d iwc=%d iwr=%d iwb=%d", (int)doc[FIELD_INPUT_BATHROOM_L1], (int)doc[FIELD_INPUT_BATHROOM_L2], (int)doc[FIELD_INPUT_UTILITY_L], (int)doc[FIELD_INPUT_WC_L], (int)doc[FIELD_INPUT_WINDOW_ROOF], (int)doc[FIELD_INPUT_WINDOW_BALC]);
    LOG_INFO("HTTP", "S_U OFFTIME: twc=%d tut=%d tkop=%d tdse=%d", (int)doc[FIELD_TIME_WC], (int)doc[FIELD_TIME_UTILITY], (int)doc[FIELD_TIME_BATHROOM], (int)doc[FIELD_TIME_LIVING_EXH]);
    LOG_INFO("HTTP", "S_U SENSOR: tbat=%.1f hbat=%.1f pbat=%.1f tutl=%.1f hutl=%.1f", (float)doc[FIELD_TEMP_BATHROOM], (float)doc[FIELD_HUM_BATHROOM], (float)doc[FIELD_PRESS_BATHROOM], (float)doc[FIELD_TEMP_UTILITY], (float)doc[FIELD_HUM_UTILITY]);
    LOG_INFO("HTTP", "S_U ERR+EN: ebm=%d esht=%d epwr=%d edew=%d etms=%d pwr=%.1f eng=%.1f dlds=%d", (int)doc[FIELD_ERROR_BME280], (int)doc[FIELD_ERROR_SHT41], (int)doc[FIELD_ERROR_POWER], (int)doc[FIELD_ERROR_DEW], (int)doc[FIELD_ERROR_TIME_SYNC], (float)doc[FIELD_CURRENT_POWER], (float)doc[FIELD_ENERGY_CONSUMPTION], (int)doc[FIELD_DUTY_CYCLE_LIVING]);

    bool success = sendHttpPostWithRetry("REW", url.c_str(), jsonString, 2, false);
    if (success) {
        LOG_INFO("HTTP", "STATUS_UPDATE na REW: uspeh");
    }
    return success;
}

// Send DEW_UPDATE to specified DEW unit - unified payload for both UT and KOP
bool sendDewUpdate(const char* room) {
    String url;
    if (strcmp(room, "UT") == 0) {
        url = String(DEW_UT_URL) + "/api/dew-update";
    } else if (strcmp(room, "KOP") == 0) {
        url = String(DEW_KOP_URL) + "/api/dew-update";
    } else {
        LOG_ERROR("HTTP", "Invalid room for DEW_UPDATE: %s", room);
        return false;
    }

    // Create unified JSON payload - same structure sent to both UT and KOP
    // Each DEW unit reads the fields relevant to its role
    DynamicJsonDocument doc(384);

    // Utility fan state (0=off, 1=on, 6-8=drying mode, 9=disabled)
    if (currentData.disableUtility) {
        doc[FIELD_FAN_UTILITY] = 9;
    } else if (currentData.utilityDryingMode && currentData.utilityCycleMode >= 1 && currentData.utilityCycleMode <= 3) {
        doc[FIELD_FAN_UTILITY] = 5 + currentData.utilityCycleMode;  // 6, 7, or 8
    } else {
        doc[FIELD_FAN_UTILITY] = currentData.utilityFan ? 1 : 0;
    }

    // Bathroom fan state (0=off, 1=on, 6-8=drying mode, 9=disabled)
    if (currentData.disableBathroom) {
        doc[FIELD_FAN_BATHROOM] = 9;
    } else if (currentData.bathroomDryingMode && currentData.bathroomCycleMode >= 1 && currentData.bathroomCycleMode <= 3) {
        doc[FIELD_FAN_BATHROOM] = 5 + currentData.bathroomCycleMode;  // 6, 7, or 8
    } else {
        doc[FIELD_FAN_BATHROOM] = currentData.bathroomFan ? 1 : 0;
    }

    // Utility time: expectedEndTime if drying (6-8), else offTimes[1]
    if (currentData.utilityDryingMode && currentData.utilityCycleMode >= 1 && currentData.utilityCycleMode <= 3) {
        doc[FIELD_TIME_UTILITY] = currentData.utilityExpectedEndTime;
    } else {
        doc[FIELD_TIME_UTILITY] = currentData.offTimes[1];
    }

    // Bathroom time: expectedEndTime if drying (6-8), else offTimes[0]
    if (currentData.bathroomDryingMode && currentData.bathroomCycleMode >= 1 && currentData.bathroomCycleMode <= 3) {
        doc[FIELD_TIME_BATHROOM] = currentData.bathroomExpectedEndTime;
    } else {
        doc[FIELD_TIME_BATHROOM] = currentData.offTimes[0];
    }

    // Sensor data - both rooms (each DEW unit uses its own fields)
    doc[FIELD_TEMP_BATHROOM] = currentData.bathroomTemp;      // tbat - KOP_DEW uses this
    doc[FIELD_HUM_BATHROOM]  = currentData.bathroomHumidity;  // hbat - KOP_DEW uses this
    doc[FIELD_PRESS_BATHROOM] = currentData.bathroomPressure; // pbat - KOP_DEW uses this
    doc[FIELD_TEMP_UTILITY]  = currentData.utilityTemp;       // tutl - UT_DEW uses this
    doc[FIELD_HUM_UTILITY]   = currentData.utilityHumidity;   // hutl - UT_DEW uses this

    // Weather icon and season (both DEW units use these)
    doc[FIELD_WEATHER_ICON] = currentWeatherIcon;             // wi (string)
    doc[FIELD_SEASON_CODE]  = currentSeasonCode;              // ss

    // Error flags (both DEW units may use these)
    doc[FIELD_ERROR_BME280] = currentData.errorFlags & ERR_BME280 ? 1 : 0;  // ebm
    doc[FIELD_ERROR_SHT41]  = currentData.errorFlags & ERR_SHT41 ? 1 : 0;   // esht

    String jsonString;
    serializeJson(doc, jsonString);

    LOG_DEBUG("HTTP", "DEW_UPDATE to %s: %s", room, jsonString.c_str());
    return sendHttpPostWithRetry(room, url.c_str(), jsonString);
}

// Check and send STATUS_UPDATE to REW when states change or periodically
void checkAndSendStatusUpdate() {
    // Fan states via shared helper — eliminates code duplication with sendStatusUpdate
    FanStates fs = computeFanStates();

    uint8_t currentInputL1 = currentData.bathroomLight1 ? 1 : 0;
    uint8_t currentInputL2 = currentData.bathroomLight2 ? 1 : 0;
    uint8_t currentInputUl = currentData.utilityLight ? 1 : 0;
    uint8_t currentInputWc = currentData.wcLight ? 1 : 0;
    uint8_t currentInputWr = currentData.windowSensor1 ? 1 : 0;
    uint8_t currentInputWb = currentData.windowSensor2 ? 1 : 0;
    // Error flags — kritične napake morajo takoj sprožiti STATUS_UPDATE
    uint8_t currentErrFlags = currentData.errorFlags & (ERR_BME280 | ERR_SHT41 | ERR_POWER);
    uint8_t currentDewErr   = currentData.dewError;

    // Check if any state changed
    static uint8_t lastFanWc = 255, lastFanUt = 255, lastFanKop = 255, lastFanDse = 255;
    static uint8_t lastInputL1 = 255, lastInputL2 = 255;
    static uint8_t lastInputUl = 255, lastInputWc = 255, lastInputWr = 255, lastInputWb = 255;
    static uint8_t lastErrFlags = 255, lastDewErr = 255;

    bool changed = (fs.fwc != lastFanWc) || (fs.fut != lastFanUt) ||
                   (fs.fkop != lastFanKop) || (fs.fdse != lastFanDse) ||
                   (currentInputL1 != lastInputL1) || (currentInputL2 != lastInputL2) ||
                   (currentInputUl != lastInputUl) || (currentInputWc != lastInputWc) ||
                   (currentInputWr != lastInputWr) || (currentInputWb != lastInputWb) ||
                   (currentErrFlags != lastErrFlags) || (currentDewErr != lastDewErr);

    unsigned long now = millis();
    bool timeToSend = (currentData.lastStatusUpdateTime == 0) ||
                      (now - currentData.lastStatusUpdateTime >= STATUS_UPDATE_INTERVAL);

    if (!changed && !timeToSend) return;

    // Calculate power and accumulate energy before sending
    calculatePower();
    static unsigned long lastEnergyUpdate = 0;
    if (lastEnergyUpdate > 0) {
        float hours = (now - lastEnergyUpdate) / 3600000.0;
        currentData.energyConsumption += currentData.currentPower * hours;
    }
    lastEnergyUpdate = now;

    // Send STATUS_UPDATE only if REW is online
    bool success = false;
    if (rewStatus.isOnline) {
        success = sendStatusUpdate();
        if (!success) {
            rewStatus.isOnline = false;
            LOG_WARN("HTTP", "REW marked offline due to STATUS_UPDATE failure");
        }
    }

    // Send DEW updates only if devices are online
    if (utDewStatus.isOnline) {
        if (!sendDewUpdate("UT")) {
            utDewStatus.isOnline = false;
            LOG_WARN("HTTP", "UT_DEW marked offline due to DEW_UPDATE failure");
        }
    }

    if (kopDewStatus.isOnline) {
        if (!sendDewUpdate("KOP")) {
            kopDewStatus.isOnline = false;
            LOG_WARN("HTTP", "KOP_DEW marked offline due to DEW_UPDATE failure");
        }
    }

    // Update last states and timestamp only if STATUS_UPDATE successful
    if (success) {
        lastFanWc = fs.fwc;
        lastFanUt = fs.fut;
        lastFanKop = fs.fkop;
        lastFanDse = fs.fdse;
        lastErrFlags = currentErrFlags;
        lastDewErr = currentDewErr;
        lastInputL1 = currentInputL1;
        lastInputL2 = currentInputL2;
        lastInputUl = currentInputUl;
        lastInputWc = currentInputWc;
        lastInputWr = currentInputWr;
        lastInputWb = currentInputWb;
        currentData.lastStatusUpdateTime = now;
    }
}

// Send logs to REW
bool sendLogsToREW() {
    if (logBuffer.length() == 0) return true;
    String url = String(REW_URL) + "/api/logs";

    // Wrap logBuffer in JSON format
    DynamicJsonDocument doc(1024);
    doc[FIELD_LOGS] = logBuffer;

    String jsonString;
    serializeJson(doc, jsonString);

    float kb = logBuffer.length() / 1024.0;
    bool success = sendHttpPostWithRetry("REW", url.c_str(), jsonString, 2, false);
    if (success) {
        logBuffer.clear();
        LOG_INFO("HTTP", "LOGS na REW: %.1f kB uspeh", kb);
    } else {
        LOG_ERROR("HTTP", "LOGS na REW: neuspeh, ponovno čez 5 min");
    }
    return success;
}

// Helper function to send HTTP POST
int sendHttpPost(const char* url, const String& jsonData, int timeoutMs) {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(timeoutMs);

    int httpResponseCode = http.POST(jsonData);

    http.end();
    return httpResponseCode;
}

// Helper function with retry logic
bool sendHttpPostWithRetry(const char* deviceName, const char* url, const String& jsonData, int maxRetries, bool logResult) {
    const int HTTP_TIMEOUT_MS = 10000; // 10 second timeout
    int lastHttpCode = 0;

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        int httpCode = sendHttpPost(url, jsonData, HTTP_TIMEOUT_MS);
        lastHttpCode = httpCode;

        // Success (200-299)
        if (httpCode >= 200 && httpCode < 300) {
            if (logResult) {
                LOG_INFO("HTTP", "POST to %s - Response: %d, Success", deviceName, httpCode);
            }
            return true;
        }

        // HTTP client error (400-499) - message invalid but device online
        if (httpCode >= 400 && httpCode < 500) {
            LOG_WARN("HTTP", "Request rejected code=%d - message invalid but device online", httpCode);
            if (logResult) {
                LOG_INFO("HTTP", "POST to %s - Response: %d, Success", deviceName, httpCode);
            }
            return true; // Don't mark offline, message was processed
        }

        // Connection error (0, negative) or server error (500+) - device offline
        if (httpCode <= 0 || httpCode >= 500) {
            if (attempt < maxRetries) {
                // Delay before retry: 2s
                int delayMs = 2000;
                delay(delayMs);
            }
        }
    }

    if (logResult) {
        LOG_ERROR("HTTP", "POST to %s - Failed after %d attempts", deviceName, maxRetries);
    }
    return false; // Mark device as offline
}

// Check if a device is online by pinging its /api/ping endpoint
bool checkDeviceOnline(const char* ip) {
    String url = String("http://") + ip + "/api/ping";
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000); // 2 second timeout

    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        String response = http.getString();
        if (response == "pong") {
            http.end();
            return true;
        }
    }

    http.end();
    return false;
}

// Check all devices that are currently offline
void checkAllDevices() {
    String statusMessage = "Offline test:";

    // Check REW
    bool rewOnline = checkDeviceOnline("192.168.2.190");
    if (rewOnline != rewStatus.isOnline) {
        rewStatus.isOnline = rewOnline;
    }
    statusMessage += String(" REW ") + (rewOnline ? "online" : "offline");

    // Check UT_DEW
    bool utOnline = checkDeviceOnline("192.168.2.193");
    if (utOnline != utDewStatus.isOnline) {
        utDewStatus.isOnline = utOnline;
    }
    statusMessage += String(", UT_DEW ") + (utOnline ? "online" : "offline");

    // Check KOP_DEW
    bool kopOnline = checkDeviceOnline("192.168.2.194");
    if (kopOnline != kopDewStatus.isOnline) {
        kopDewStatus.isOnline = kopOnline;
    }
    statusMessage += String(", KOP_DEW ") + (kopOnline ? "online" : "offline");

    LOG_INFO("HTTP", "%s", statusMessage.c_str());
}
