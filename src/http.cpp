// http.cpp - HTTP client functions for sending messages

#include "http.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "logging.h"
#include "config.h"
#include "vent.h"

// URLs for different units
#define REW_URL "http://192.168.2.190"
#define DEW_UT_URL "http://192.168.2.193"
#define DEW_KOP_URL "http://192.168.2.194"

// Send STATUS_UPDATE to REW
bool sendStatusUpdate() {
    String url = String(REW_URL) + "/api/status-update";

    // Create JSON payload
    DynamicJsonDocument doc(512);
    JsonArray fans = doc.createNestedArray("fans");
    JsonArray inputs = doc.createNestedArray("inputs");
    JsonArray offTimes = doc.createNestedArray("offTimes");
    JsonArray errorFlags = doc.createNestedArray("errorFlags");

    // Fill fans array (6 fans: WC, UT, KOP, DS exhaust, DS intake, living)
    fans.add(currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0);  // WC
    fans.add(currentData.utilityFan ? (currentData.disableUtility ? 2 : 1) : 0);   // UT
    fans.add(currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0); // KOP (same as bathroom)
    fans.add(currentData.livingExhaustLevel > 0 ? 1 : 0);  // DS exhaust
    fans.add(currentData.commonIntake ? 1 : 0);  // DS intake
    fans.add(currentData.livingExhaustLevel > 0 ? 1 : 0);  // Living (same as DS)

    // Fill inputs array (8 inputs)
    inputs.add(currentData.bathroomButton);
    inputs.add(currentData.utilitySwitch);
    inputs.add(currentData.bathroomLight1);
    inputs.add(currentData.bathroomLight2);
    inputs.add(currentData.utilityLight);
    inputs.add(currentData.wcLight);
    inputs.add(currentData.windowSensor1);
    inputs.add(currentData.windowSensor2);

    // Fill offTimes array (6 values)
    for (int i = 0; i < 6; i++) {
        offTimes.add(currentData.offTimes[i]);
    }

    // Fill errorFlags array (5 values)
    errorFlags.add(currentData.errorFlags & ERR_BME280 ? 1 : 0);
    errorFlags.add(currentData.errorFlags & ERR_SHT41 ? 1 : 0);
    errorFlags.add(0); // ERR_LITTLEFS
    errorFlags.add(0); // ERR_HTTP
    errorFlags.add(0); // ERR_SD

    // Add sensor data
    doc["bathroomTemp"] = currentData.bathroomTemp;
    doc["bathroomHumidity"] = currentData.bathroomHumidity;
    doc["bathroomPressure"] = currentData.bathroomPressure;
    doc["utilityTemp"] = currentData.utilityTemp;
    doc["utilityHumidity"] = currentData.utilityHumidity;
    doc["currentPower"] = currentData.currentPower;
    doc["energyConsumption"] = currentData.energyConsumption;

    String jsonString;
    serializeJson(doc, jsonString);

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

    // Create JSON payload
    DynamicJsonDocument doc(256);

    // Get fan state for the room
    if (strcmp(room, "UT") == 0) {
        doc["fan"] = currentData.utilityFan ? 1 : 0;
        doc["countdown"] = currentData.offTimes[1]; // UT off time
        doc["temp"] = currentData.utilityTemp;
        doc["humidity"] = currentData.utilityHumidity;
        doc["error"] = currentData.errorFlags & ERR_SHT41 ? 1 : 0;
    } else if (strcmp(room, "KOP") == 0) {
        doc["fan"] = currentData.bathroomFan ? 1 : 0;
        doc["countdown"] = currentData.offTimes[0]; // KOP off time
        doc["temp"] = currentData.bathroomTemp;
        doc["humidity"] = currentData.bathroomHumidity;
        doc["error"] = currentData.errorFlags & ERR_BME280 ? 1 : 0;
    }

    String jsonString;
    serializeJson(doc, jsonString);

    LOG_INFO("HTTP", "Sending DEW_UPDATE to %s", room);
    return sendHttpPostWithRetry(url.c_str(), jsonString);
}

// Check and send STATUS_UPDATE to REW when fan states change or periodically
void checkAndSendStatusUpdate() {
    // Build current arrays
    uint8_t currentFans[6] = {
        currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0,  // WC
        currentData.utilityFan ? (currentData.disableUtility ? 2 : 1) : 0,   // UT
        currentData.bathroomFan ? (currentData.disableBathroom ? 2 : 1) : 0, // KOP (same as bathroom)
        (currentData.livingExhaustLevel > 0) ? 1 : (currentData.disableLivingRoom ? 2 : 0),  // DS exhaust
        currentData.commonIntake ? 1 : 0,  // DS intake
        (currentData.livingExhaustLevel > 0) ? 1 : 0   // Living (same as DS)
    };

    uint8_t currentInputs[8] = {
        currentData.bathroomButton ? 1 : 0,  // bathroomButton
        (currentData.bathroomLight1 || currentData.bathroomLight2) ? 1 : 0,  // bathroomLight
        currentData.utilityLight ? 1 : 0,  // utilityLight
        currentData.wcLight ? 1 : 0,  // wcLight
        currentData.windowSensor1 ? 1 : 0,  // windowSensor1
        currentData.windowSensor2 ? 1 : 0,  // windowSensor2
        0,  // skylightSensor (not implemented)
        0   // balconyDoorSensor (not implemented)
    };

    // Check if changed or time to send (5 minutes)
    bool changed = false;
    for (int i = 0; i < 6; i++) {
        if (currentFans[i] != currentData.previousFans[i]) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        for (int i = 0; i < 8; i++) {
            if (currentInputs[i] != currentData.previousInputs[i]) {
                changed = true;
                break;
            }
        }
    }
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

    // Send STATUS_UPDATE
    bool success = sendStatusUpdate();

    // Send DEW updates regardless of STATUS_UPDATE success/failure
    sendDewUpdate("UT");
    sendDewUpdate("KOP");

    // Update previous arrays and timestamp only if successful
    if (success) {
        memcpy(currentData.previousFans, currentFans, sizeof(currentFans));
        memcpy(currentData.previousInputs, currentInputs, sizeof(currentInputs));
        currentData.lastStatusUpdateTime = now;
    }
}

// Send logs to REW
bool sendLogsToREW() {
    if (logBuffer.length() == 0) return true;
    String url = String(REW_URL) + "/api/logs";
    LOG_INFO("HTTP", "Sending logs to REW, length: %d", logBuffer.length());
    bool success = sendHttpPostWithRetry(url.c_str(), logBuffer);
    if (success) {
        logBuffer.clear();
        LOG_INFO("HTTP", "Logs sent to REW successfully");
    } else {
        LOG_ERROR("HTTP", "Failed to send logs to REW");
    }
    return success;
}

// Helper function to send HTTP POST
bool sendHttpPost(const char* url, const String& jsonData, int timeoutMs) {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(timeoutMs);

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
        String response = http.getString();
        LOG_INFO("HTTP", "POST to %s - Response: %d, Body: %s", url, httpResponseCode, response.c_str());
        http.end();
        return (httpResponseCode >= 200 && httpResponseCode < 300);
    } else {
        LOG_ERROR("HTTP", "POST to %s failed - Error: %d", url, httpResponseCode);
        http.end();
        return false;
    }
}

// Helper function with retry logic
bool sendHttpPostWithRetry(const char* url, const String& jsonData, int maxRetries) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        LOG_INFO("HTTP", "Attempt %d/%d to %s", attempt, maxRetries, url);

        if (sendHttpPost(url, jsonData)) {
            return true;
        }

        if (attempt < maxRetries) {
            // Exponential backoff: 1s, 2s, 4s
            int delayMs = 1000 * (1 << (attempt - 1));
            LOG_INFO("HTTP", "Retrying in %d ms", delayMs);
            delay(delayMs);
        }
    }

    LOG_ERROR("HTTP", "All %d attempts failed for %s", maxRetries, url);
    return false;
}