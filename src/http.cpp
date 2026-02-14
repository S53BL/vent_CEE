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

// Send STATUS_UPDATE to REW
bool sendStatusUpdate() {
    String url = String(REW_URL) + "/api/status-update";

    // Create JSON payload with short field names
    DynamicJsonDocument doc(512);

    // Fans (0=off, 1=on, 9=disabled)
    doc["fwc"] = currentData.disableWc ? 9 : (currentData.wcFan ? 1 : 0);  // fan wc
    doc["fut"] = currentData.disableUtility ? 9 : (currentData.utilityFan ? 1 : 0);   // fan utility
    doc["fkop"] = currentData.disableBathroom ? 9 : (currentData.bathroomFan ? 1 : 0); // fan bathroom
    // fdse: 0=off, 1=level1, 2=level2, 3=level3, 9=disabled
    if (currentData.disableLivingRoom) {
        doc["fdse"] = 9;  // disabled
    } else {
        doc["fdse"] = currentData.livingExhaustLevel;  // 0, 1, 2, ali 3
    }

    // Inputs (digital states)
    doc["il1"] = currentData.bathroomLight1;     // input bathroom light 1
    doc["il2"] = currentData.bathroomLight2;     // input bathroom light 2
    doc["iul"] = currentData.utilityLight;       // input utility light
    doc["iwc"] = currentData.wcLight;            // input wc light
    doc["iwr"] = currentData.windowSensor1;      // input window roof
    doc["iwb"] = currentData.windowSensor2;      // input window balcony

    // Off-times (Unix timestamps)
    doc["twc"] = currentData.offTimes[2];        // time wc
    doc["tut"] = currentData.offTimes[1];        // time utility
    doc["tkop"] = currentData.offTimes[0];       // time bathroom
    doc["tdse"] = currentData.offTimes[4];       // time living exhaust

    // Error flags (0=ok, 1=error)
    doc["ebm"] = currentData.errorFlags & ERR_BME280 ? 1 : 0;   // error bme280
    doc["esht"] = currentData.errorFlags & ERR_SHT41 ? 1 : 0;   // error sht41
    doc["epwr"] = currentData.errorFlags & ERR_POWER ? 1 : 0;   // error power
    doc["edew"] = currentData.dewError;   // error dew
    // Time sync error detection
    uint8_t timeSyncError = 0;
    if (!timeSynced) {
        timeSyncError = 1;  // NTP not synchronized
    } else if (externalDataValid) {
        // Check time difference with REW
        uint32_t currentTime = myTZ.now();
        uint32_t timeDiff = abs((int32_t)(currentTime - externalData.timestamp));
        if (timeDiff > 300) {  // 5 minutes = 300 seconds
            timeSyncError = 2;  // Time discrepancy >5min
        }
        // else timeSyncError = 0;  // All OK
    }
    // If externalDataValid is false, we can't check discrepancy, so assume OK if NTP is synced
    doc["etms"] = timeSyncError;   // error time sync (0=OK, 1=NTP unsync, 2=time discrepancy)

    // Sensor data
    doc["tbat"] = currentData.bathroomTemp;      // temperature bathroom
    doc["hbat"] = currentData.bathroomHumidity;  // humidity bathroom
    doc["pbat"] = currentData.bathroomPressure;  // pressure bathroom
    doc["tutl"] = currentData.utilityTemp;       // temperature utility
    doc["hutl"] = currentData.utilityHumidity;   // humidity utility
    doc["pwr"] = currentData.currentPower;       // current power
    doc["eng"] = currentData.energyConsumption;  // energy consumption
    doc["dlds"] = (int)currentData.livingRoomDutyCycle;  // duty cycle living room (%)

    String jsonString;
    serializeJson(doc, jsonString);

    // Structured logging for readability
    LOG_INFO("HTTP", "S_U FAN: fwc=%d fut=%d fkop=%d fdse=%d", (int)doc["fwc"], (int)doc["fut"], (int)doc["fkop"], (int)doc["fdse"]);
    LOG_INFO("HTTP", "S_U INPUT: il1=%d il2=%d iul=%d iwc=%d iwr=%d iwb=%d", (int)doc["il1"], (int)doc["il2"], (int)doc["iul"], (int)doc["iwc"], (int)doc["iwr"], (int)doc["iwb"]);
    LOG_INFO("HTTP", "S_U OFFTIME: twc=%d tut=%d tkop=%d tdse=%d", (int)doc["twc"], (int)doc["tut"], (int)doc["tkop"], (int)doc["tdse"]);
    LOG_INFO("HTTP", "S_U SENSOR: tbat=%.1f hbat=%.1f pbat=%.1f tutl=%.1f hutl=%.1f", (float)doc["tbat"], (float)doc["hbat"], (float)doc["pbat"], (float)doc["tutl"], (float)doc["hutl"]);
    LOG_INFO("HTTP", "S_U ERR+EN: ebm=%d esht=%d epwr=%d edew=%d etms=%d pwr=%.1f eng=%.1f dlds=%d", (int)doc["ebm"], (int)doc["esht"], (int)doc["epwr"], (int)doc["edew"], (int)doc["etms"], (float)doc["pwr"], (float)doc["eng"], (int)doc["dlds"]);

    bool success = sendHttpPostWithRetry("REW", url.c_str(), jsonString, 2, false);
    if (success) {
        LOG_INFO("HTTP", "STATUS_UPDATE na REW: uspeh");
    }
    return success;
}

// Send DEW_UPDATE to specified room
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

    // Create JSON payload with short unique names
    DynamicJsonDocument doc(256);

    // Get fan state for the room
    if (strcmp(room, "UT") == 0) {
        doc["df"] = currentData.utilityFan ? 1 : 0;        // dew fan
        doc["do"] = currentData.offTimes[1];               // dew off (timestamp)
        doc["dt"] = currentData.utilityTemp;               // dew temp
        doc["dh"] = currentData.utilityHumidity;           // dew hum
        doc["de"] = currentData.errorFlags & ERR_SHT41 ? 1 : 0;  // dew err
        doc["wi"] = currentWeatherIcon;                    // weather icon
    } else if (strcmp(room, "KOP") == 0) {
        doc["df"] = currentData.bathroomFan ? 1 : 0;       // dew fan
        doc["do"] = currentData.offTimes[0];               // dew off (timestamp)
        doc["dt"] = currentData.bathroomTemp;              // dew temp
        doc["dh"] = currentData.bathroomHumidity;          // dew hum
        doc["de"] = currentData.errorFlags & ERR_BME280 ? 1 : 0; // dew err
        doc["wi"] = currentWeatherIcon;                    // weather icon
    }

    String jsonString;
    serializeJson(doc, jsonString);

    LOG_DEBUG("HTTP", "Sending JSON: %s", jsonString.c_str());
    return sendHttpPostWithRetry(room, url.c_str(), jsonString);
}

// Check and send STATUS_UPDATE to REW when states change or periodically
void checkAndSendStatusUpdate() {
    // Calculate current states for comparison
    uint8_t currentFanWc = currentData.disableWc ? 9 : (currentData.wcFan ? 1 : 0);
    uint8_t currentFanUt = currentData.disableUtility ? 9 : (currentData.utilityFan ? 1 : 0);
    uint8_t currentFanKop = currentData.disableBathroom ? 9 : (currentData.bathroomFan ? 1 : 0);
    uint8_t currentFanDse = currentData.disableLivingRoom ? 9 : currentData.livingExhaustLevel;

    uint8_t currentInputL1 = currentData.bathroomLight1;
    uint8_t currentInputL2 = currentData.bathroomLight2;
    uint8_t currentInputUl = currentData.utilityLight;
    uint8_t currentInputWc = currentData.wcLight;
    uint8_t currentInputWr = currentData.windowSensor1;
    uint8_t currentInputWb = currentData.windowSensor2;

    // Check if any state changed
    static uint8_t lastFanWc = 255, lastFanUt = 255, lastFanKop = 255, lastFanDse = 255;
    static uint8_t lastInputL1 = 255, lastInputL2 = 255;
    static uint8_t lastInputUl = 255, lastInputWc = 255, lastInputWr = 255, lastInputWb = 255;

    bool changed = (currentFanWc != lastFanWc) || (currentFanUt != lastFanUt) ||
                   (currentFanKop != lastFanKop) || (currentFanDse != lastFanDse) ||
                   (currentInputL1 != lastInputL1) || (currentInputL2 != lastInputL2) ||
                   (currentInputUl != lastInputUl) || (currentInputWc != lastInputWc) ||
                   (currentInputWr != lastInputWr) || (currentInputWb != lastInputWb);

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
        lastFanWc = currentFanWc;
        lastFanUt = currentFanUt;
        lastFanKop = currentFanKop;
        lastFanDse = currentFanDse;
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
