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

    // Fans (0=off, 1=on, 2=disabled)
    doc["fwc"] = currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0;  // fan wc
    doc["fut"] = currentData.utilityFan ? (currentData.disableUtility ? 2 : 1) : 0;   // fan utility
    doc["fkop"] = currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0; // fan bathroom
    // fdse: 0=off, 1=level1, 2=level2, 3=level3, 9=disabled
    if (currentData.disableLivingRoom) {
        doc["fdse"] = 9;  // disabled
    } else {
        doc["fdse"] = currentData.livingExhaustLevel;  // 0, 1, 2, ali 3
    }
    doc["fdsi"] = currentData.commonIntake ? 1 : 0;  // fan living intake
    doc["fliv"] = currentData.livingExhaustLevel > 0 ? 1 : 0;  // fan living

    // Inputs (digital states)
    doc["ibt"] = currentData.bathroomButton;     // input bathroom button
    doc["iut"] = currentData.utilitySwitch;      // input utility switch
    doc["il1"] = currentData.bathroomLight1;     // input bathroom light 1
    doc["il2"] = currentData.bathroomLight2;     // input bathroom light 2
    doc["iul"] = currentData.utilityLight;       // input utility light
    doc["iwc"] = currentData.wcLight;            // input wc light
    doc["iwr"] = currentData.windowSensor1;      // input window roof
    doc["iwb"] = currentData.windowSensor2;      // input window balcony

    // Off-times (Unix timestamps)
    doc["twc"] = currentData.offTimes[0];        // time wc
    doc["tut"] = currentData.offTimes[1];        // time utility
    doc["tkop"] = currentData.offTimes[2];       // time bathroom
    doc["tdse"] = currentData.offTimes[3];       // time living exhaust
    doc["tdsi"] = currentData.offTimes[4];       // time living intake
    doc["tliv"] = currentData.offTimes[5];       // time living

    // Error flags (0=ok, 1=error)
    doc["ebm"] = currentData.errorFlags & ERR_BME280 ? 1 : 0;   // error bme280
    doc["esht"] = currentData.errorFlags & ERR_SHT41 ? 1 : 0;   // error sht41
    doc["elfs"] = 0;  // error littlefs (not implemented)
    doc["ehtp"] = 0;  // error http (not implemented)
    doc["esd"] = 0;   // error sd (not implemented)

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

    LOG_DEBUG("HTTP", "Sending JSON: %s", jsonString.c_str());
    LOG_INFO("HTTP", "Sending STATUS_UPDATE to REW");
    return sendHttpPostWithRetry(url.c_str(), jsonString);
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
    } else if (strcmp(room, "KOP") == 0) {
        doc["df"] = currentData.bathroomFan ? 1 : 0;       // dew fan
        doc["do"] = currentData.offTimes[0];               // dew off (timestamp)
        doc["dt"] = currentData.bathroomTemp;              // dew temp
        doc["dh"] = currentData.bathroomHumidity;          // dew hum
        doc["de"] = currentData.errorFlags & ERR_BME280 ? 1 : 0; // dew err
    }

    String jsonString;
    serializeJson(doc, jsonString);

    LOG_DEBUG("HTTP", "Sending JSON: %s", jsonString.c_str());
    LOG_INFO("HTTP", "Sending DEW_UPDATE to %s", room);
    return sendHttpPostWithRetry(url.c_str(), jsonString);
}

// Check and send STATUS_UPDATE to REW when states change or periodically
void checkAndSendStatusUpdate() {
    // Calculate current states for comparison
    uint8_t currentFanWc = currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0;
    uint8_t currentFanUt = currentData.utilityFan ? (currentData.disableUtility ? 2 : 1) : 0;
    uint8_t currentFanKop = currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0;
    uint8_t currentFanDse = currentData.disableLivingRoom ? 9 : currentData.livingExhaustLevel;
    uint8_t currentFanDsi = currentData.commonIntake ? 1 : 0;
    uint8_t currentFanLiv = currentData.livingExhaustLevel > 0 ? 1 : 0;

    uint8_t currentInputBt = currentData.bathroomButton;
    uint8_t currentInputUt = currentData.utilitySwitch;
    uint8_t currentInputL1 = currentData.bathroomLight1;
    uint8_t currentInputL2 = currentData.bathroomLight2;
    uint8_t currentInputUl = currentData.utilityLight;
    uint8_t currentInputWc = currentData.wcLight;
    uint8_t currentInputWr = currentData.windowSensor1;
    uint8_t currentInputWb = currentData.windowSensor2;

    // Check if any state changed (simplified - compare key values)
    static uint8_t lastFanWc = 255, lastFanUt = 255, lastFanKop = 255;
    static uint8_t lastInputBt = 255, lastInputUt = 255;

    bool changed = (currentFanWc != lastFanWc) || (currentFanUt != lastFanUt) ||
                   (currentFanKop != lastFanKop) || (currentInputBt != lastInputBt) ||
                   (currentInputUt != lastInputUt);

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
        lastInputBt = currentInputBt;
        lastInputUt = currentInputUt;
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

    LOG_INFO("HTTP", "Sending logs to REW, length: %d", logBuffer.length());
    bool success = sendHttpPostWithRetry(url.c_str(), jsonString);
    if (success) {
        logBuffer.clear();
        LOG_INFO("HTTP", "Logs sent to REW successfully");
    } else {
        LOG_ERROR("HTTP", "Failed to send logs to REW");
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

    if (httpResponseCode > 0) {
        String response = http.getString();
        LOG_INFO("HTTP", "POST to %s - Response: %d, Body: %s", url, httpResponseCode, response.c_str());
        http.end();
        return httpResponseCode;
    } else {
        LOG_ERROR("HTTP", "POST to %s failed - Error: %d", url, httpResponseCode);
        http.end();
        return httpResponseCode; // Return the error code (negative or 0)
    }
}

// Helper function with retry logic
bool sendHttpPostWithRetry(const char* url, const String& jsonData, int maxRetries) {
    const int HTTP_TIMEOUT_MS = 5000; // 5 second timeout

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        LOG_INFO("HTTP", "Attempt %d/%d to %s", attempt, maxRetries, url);

        int httpCode = sendHttpPost(url, jsonData, HTTP_TIMEOUT_MS);

        // Success (200-299)
        if (httpCode >= 200 && httpCode < 300) {
            return true;
        }

        // HTTP client error (400-499) - message invalid but device online
        if (httpCode >= 400 && httpCode < 500) {
            LOG_WARN("HTTP", "Request rejected code=%d - message invalid but device online", httpCode);
            return true; // Don't mark offline, message was processed
        }

        // Connection error (0, negative) or server error (500+) - device offline
        if (httpCode <= 0 || httpCode >= 500) {
            LOG_ERROR("HTTP", "Connection failed code=%d - device offline", httpCode);
            if (attempt < maxRetries) {
                // Delay before retry: 2s
                int delayMs = 2000;
                LOG_INFO("HTTP", "Retrying in %d ms", delayMs);
                delay(delayMs);
            }
        }
    }

    LOG_ERROR("HTTP", "All %d attempts failed for %s", maxRetries, url);
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
    LOG_INFO("HTTP", "Checking offline devices...");

    if (!rewStatus.isOnline) {
        if (checkDeviceOnline("192.168.2.190")) {
            rewStatus.isOnline = true;
            LOG_INFO("HTTP", "REW (192.168.2.190) is now online");
        }
    }

    if (!utDewStatus.isOnline) {
        if (checkDeviceOnline("192.168.2.193")) {
            utDewStatus.isOnline = true;
            LOG_INFO("HTTP", "UT_DEW (192.168.2.193) is now online");
        }
    }

    if (!kopDewStatus.isOnline) {
        if (checkDeviceOnline("192.168.2.194")) {
            kopDewStatus.isOnline = true;
            LOG_INFO("HTTP", "KOP_DEW (192.168.2.194) is now online");
        }
    }

    LOG_INFO("HTTP", "Device check complete");
}
