#include <Arduino.h>
#include <WiFi.h>
#include <ETHClass2.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include "config.h"
#include "globals.h"
#include "logging.h"
#include "sens.h"
#include "vent.h"
#include "http.h"
#include "web.h"
#include "sd.h"

#define ETH ETH2

// SPI host for ESP32-S3
#define HSPI_HOST SPI3_HOST

// Shortened event names
#define ETH_START       ARDUINO_EVENT_ETH_START
#define ETH_CONNECTED   ARDUINO_EVENT_ETH_CONNECTED
#define ETH_GOT_IP      ARDUINO_EVENT_ETH_GOT_IP
#define ETH_DISCONNECTED ARDUINO_EVENT_ETH_DISCONNECTED
#define ETH_STOP        ARDUINO_EVENT_ETH_STOP



// HTTP endpoint handlers
void handleManualControl(AsyncWebServerRequest *request) {
    String body = request->arg("plain");
    LOG_INFO("HTTP", "Received MANUAL_CONTROL: %s", body.c_str());

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        LOG_ERROR("HTTP", "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Invalid JSON\"}");
        return;
    }

    String room = doc["room"] | "";
    String action = doc["action"] | "";

    if (room == "" || action == "") {
        LOG_ERROR("HTTP", "Missing room or action in MANUAL_CONTROL");
        request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Missing room or action\"}");
        return;
    }

    // Handle manual control based on room and action
    if (action == "manual") {
        if (room == "wc") {
            currentData.manualTriggerWC = true;
            LOG_INFO("HTTP", "Manual trigger WC activated");
        } else if (room == "ut") {
            currentData.manualTriggerUtility = true;
            LOG_INFO("HTTP", "Manual trigger Utility activated");
        } else if (room == "kop") {
            currentData.manualTriggerBathroom = true;
            LOG_INFO("HTTP", "Manual trigger Bathroom activated");
        } else if (room == "ds") {
            currentData.manualTriggerLivingRoom = true;
            LOG_INFO("HTTP", "Manual trigger Living Room activated");
        } else {
            LOG_ERROR("HTTP", "Unknown room: %s", room.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room\"}");
            return;
        }
    } else if (action == "toggle") {
        if (room == "wc") {
            currentData.disableWc = !currentData.disableWc;
            LOG_INFO("HTTP", "WC disable toggled to: %s", currentData.disableWc ? "true" : "false");
        } else if (room == "ut") {
            currentData.disableUtility = !currentData.disableUtility;
            LOG_INFO("HTTP", "Utility disable toggled to: %s", currentData.disableUtility ? "true" : "false");
        } else if (room == "kop") {
            currentData.disableBathroom = !currentData.disableBathroom;
            LOG_INFO("HTTP", "Bathroom disable toggled to: %s", currentData.disableBathroom ? "true" : "false");
        } else if (room == "ds") {
            currentData.disableLivingRoom = !currentData.disableLivingRoom;
            LOG_INFO("HTTP", "Living Room disable toggled to: %s", currentData.disableLivingRoom ? "true" : "false");
        } else {
            LOG_ERROR("HTTP", "Unknown room: %s", room.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room\"}");
            return;
        }
    } else {
        LOG_ERROR("HTTP", "Unknown action: %s", action.c_str());
        request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown action\"}");
        return;
    }

    request->send(200, "application/json", "{\"status\":\"OK\"}");
}

void handleSensorData(AsyncWebServerRequest *request) {
    String body = request->arg("plain");
    LOG_INFO("HTTP", "Received SENSOR_DATA: %s", body.c_str());

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        LOG_ERROR("HTTP", "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Invalid JSON\"}");
        return;
    }

    // Update external data from REW
    externalData.externalTemperature = doc["et"] | 0.0f;     // external temp
    externalData.externalHumidity = doc["eh"] | 0.0f;       // external humidity
    externalData.externalPressure = doc["ep"] | 0.0f;       // external pressure
    externalData.livingTempDS = doc["dt"] | 0.0f;           // ds temp (living room)
    externalData.livingHumidityDS = doc["dh"] | 0.0f;       // ds humidity
    externalData.livingCO2 = doc["dc"] | 0;                 // ds CO2
    externalData.timestamp = doc["ts"] | 0;                 // timestamp

    // Update currentData for immediate use by vent control logic
    currentData.externalTemp = externalData.externalTemperature;
    currentData.externalHumidity = externalData.externalHumidity;
    currentData.externalPressure = externalData.externalPressure;
    currentData.livingTemp = externalData.livingTempDS;
    currentData.livingHumidity = externalData.livingHumidityDS;
    currentData.livingCO2 = externalData.livingCO2;

    // Update external data validation
    if (timeSynced) {
        lastSensorDataTime = myTZ.now();
        externalDataValid = true;
    }

    LOG_INFO("HTTP", "Updated external data - Temp: %.1f°C, Hum: %.1f%%, CO2: %d",
             externalData.externalTemperature, externalData.externalHumidity, externalData.livingCO2);

    request->send(200, "application/json", "{\"status\":\"OK\"}");
}

void onNTPUpdate() {
    if (timeStatus() == timeSet) {
        timeSynced = true;
        Serial.println("NTP: Time synchronized successfully");
        Serial.printf("NTP: Current time: %s\n", myTZ.dateTime().c_str());
    } else {
        Serial.println("NTP: Sync failed");
        timeSynced = false;
    }
}

void setupNTP() {
    myTZ.setPosix(TZ_STRING);
    events();
    setInterval(NTP_UPDATE_INTERVAL / 1000);
    Serial.println("NTP: Timezone set to CET/CEST - asynchronous mode enabled");
}

bool syncNTP() {
    if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println("NTP: Ethernet not connected - cannot sync");
        timeSynced = false;
        return false;
    }

    Serial.println("NTP: Starting NTP sync with servers:");
    for (int i = 0; i < NTP_SERVER_COUNT; i++) {
        Serial.printf("NTP: Trying server %d: %s\n", i+1, ntpServers[i]);

        setServer(ntpServers[i]);
        updateNTP();
        delay(2000); // Wait for NTP response

        // Check if time is now valid (ezTime sets time after successful sync)
        if (myTZ.now() > 1609459200) { // Check if time is after 2021-01-01 (reasonable check)
            Serial.printf("NTP: SUCCESS with %s\n", ntpServers[i]);
            Serial.printf("NTP: Current time: %s\n", myTZ.dateTime().c_str());
            timeSynced = true;
            return true;
        } else {
            Serial.printf("NTP: FAILED with %s (invalid time)\n", ntpServers[i]);
        }

        delay(1000); // Small delay between attempts
    }

    Serial.println("NTP: All servers failed - time not synchronized");
    timeSynced = false;
    return false;
}

// Setup server endpoints
void setupServer() {
    LOG_INFO("HTTP", "Setting up server endpoints");

    // Settings endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleDataRequest);
    server.on("/settings/update", HTTP_POST, handlePostSettings);
    server.on("/settings/reset", HTTP_POST, handleResetSettings);
    server.on("/settings/status", HTTP_GET, handleSettingsStatus);

    // Existing endpoints
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String status = "vent_CEE Status\n";
        status += "IP: " + ETH.localIP().toString() + "\n";
        status += "MAC: " + ETH.macAddress() + "\n";
        status += "Uptime: " + String(millis() / 1000) + " seconds\n";
        request->send(200, "text/plain", status);
    });

    // New API endpoints
    server.on("/api/manual-control", HTTP_POST, handleManualControl);
    server.on("/api/sensor-data", HTTP_POST, handleSensorData);

    // Heartbeat endpoint
    server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "pong");
    });

    LOG_INFO("HTTP", "Starting server");
    server.begin();
    LOG_INFO("HTTP", "Server started on port 80");
}

// Ethernet event handler
void onEvent(arduino_event_id_t event) {
    switch (event) {
        case ETH_START:
            Serial.println("ETH Started");
            break;
        case ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case ETH_GOT_IP:
            Serial.print("ETH IP: ");
            Serial.println(ETH.localIP());
            break;
        case ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            break;
        case ETH_STOP:
            Serial.println("ETH Stopped");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting vent_CEE...");

    // Register Ethernet event handler
    WiFi.onEvent(onEvent);

    // Initialize Ethernet with W5500
    bool ethInitialized = ETH.begin(ETH_PHY_W5500, 1, 14, 10, 9, HSPI_HOST, 13, 12, 11);
    if (!ethInitialized) {
        Serial.println("ETH init failed! Continuing without network...");
    } else {
        // Set static IP
        ETH.config(IPAddress(192,168,2,192), IPAddress(192,168,2,1), IPAddress(255,255,255,0), IPAddress(192,168,2,1));

        // Wait for IP with timeout (20 seconds)
        unsigned long startTime = millis();
        const unsigned long IP_TIMEOUT = 20000; // 20 seconds

        while (ETH.localIP() == IPAddress(0, 0, 0, 0) && (millis() - startTime) < IP_TIMEOUT) {
            delay(1000);
            Serial.println("Waiting for IP...");
        }

        if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
            Serial.println("IP timeout! Continuing without network connection...");
        } else {
            Serial.print("Static IP assigned: ");
            Serial.println(ETH.localIP());
        }
    }

    // Setup NTP
    Serial.println("Setting up NTP...");
    setupNTP();
    syncNTP();  // Try to sync NTP on startup
    Serial.println("NTP setup complete");

    // Initialize SD card
    Serial.println("Initializing SD card...");
    initSD();
    Serial.println("SD initialization complete");

    // Initialize sensors
    initSensors();

    // Initialize inputs
    setupInputs();

    // Initialize vent outputs
    setupVent();

    // Initialize logging
    initLogging();

    // Load settings from NVS
    loadSettings();

    // Setup and start web server
    setupServer();
}

void loop() {
    uint32_t now = millis();

    // Periodic sensor reading
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL * 1000) {
        readSensors();
        performPeriodicSensorCheck();
        performPeriodicI2CReset();
        lastSensorRead = now;
    }

    // Periodic input reading
    static unsigned long lastInputRead = 0;
    if (now - lastInputRead >= 200) {  // Every 200ms
        readInputs();
        lastInputRead = now;
    }

    // Vent control functions - every 1 second (separate from input reading)
    static unsigned long lastVentControl = 0;
    if (now - lastVentControl >= 1000) {  // Every 1 second
        controlBathroom();
        controlUtility();
        controlWC();
        controlLivingRoom();
        controlFans();
        lastVentControl = now;
    }

    // Periodic log flushing
    if (now - lastLogFlush >= 30000) {  // Every 30 seconds
        flushLogBuffer();
        lastLogFlush = now;
    }

    // Check and send STATUS_UPDATE to REW
    checkAndSendStatusUpdate();

    // NTP status check - ezTime handles synchronization asynchronously
    if (!timeSynced && timeStatus() == timeSet) {
        timeSynced = true;
        Serial.println("NTP: Time synchronized successfully");
        Serial.printf("NTP: Current time: %s\n", myTZ.dateTime().c_str());
    }

    // Check REW sensor data timeout
    static unsigned long lastDataTimeoutCheck = 0;
    if (timeSynced && externalDataValid && now - lastDataTimeoutCheck > 300000) {  // Check every 5 minutes
        lastDataTimeoutCheck = now;
        if ((myTZ.now() - lastSensorDataTime) > 900) {  // 15 minutes = 900 seconds
            LOG_ERROR("HTTP", "REW sensor data timeout - no data for 15+ minutes, invalidating external data");
            externalDataValid = false;
        }
    }

    // Periodic network retry - every 5 minutes if no network
    static unsigned long lastNetworkRetry = 0;
    const unsigned long NETWORK_RETRY_INTERVAL = 300000; // 5 minutes
    if (ETH.localIP() == IPAddress(0, 0, 0, 0) &&
        (now - lastNetworkRetry) > NETWORK_RETRY_INTERVAL) {
        lastNetworkRetry = now;
        Serial.println("Retrying network connection...");

        if (ETH.begin(ETH_PHY_W5500, 1, 14, 10, 9, HSPI_HOST, 13, 12, 11)) {
            ETH.config(IPAddress(192,168,2,192), IPAddress(192,168,2,1), IPAddress(255,255,255,0), IPAddress(192,168,2,1));
            Serial.println("Network reconnected!");
        }
    }

    // Maintain loop timing for consistent execution
    static unsigned long lastLoopTime = 0;
    const unsigned long LOOP_INTERVAL = 1000; // 1 second

    unsigned long currentTime = millis();
    if (currentTime - lastLoopTime < LOOP_INTERVAL) {
        delay(LOOP_INTERVAL - (currentTime - lastLoopTime));
    }
    lastLoopTime = millis();
}
