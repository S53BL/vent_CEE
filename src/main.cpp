#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
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
#include "message_fields.h"

#define ETH ETH2

// SPI host for ESP32-S3
#define HSPI_HOST SPI3_HOST

// Shortened event names
#define ETH_START       ARDUINO_EVENT_ETH_START
#define ETH_CONNECTED   ARDUINO_EVENT_ETH_CONNECTED
#define ETH_GOT_IP      ARDUINO_EVENT_ETH_GOT_IP
#define ETH_DISCONNECTED ARDUINO_EVENT_ETH_DISCONNECTED
#define ETH_STOP        ARDUINO_EVENT_ETH_STOP





void onNTPUpdate() {
    if (timeStatus() == timeSet) {
        timeSynced = true;
        LOG_INFO("NTP", "Time synchronized successfully");
        LOG_INFO("NTP", "Current time: %s", myTZ.dateTime().c_str());
    } else {
        LOG_ERROR("NTP", "Sync failed");
        timeSynced = false;
    }
}

void setupNTP() {
    // Synchronous mode
    myTZ.setPosix(TZ_STRING);
    LOG_INFO("NTP", "Timezone set to CET/CEST - synchronous mode enabled");
}

// NTP UDP implementation
WiFiUDP ntpUDP;
const int NTP_BUFFER_SIZE = 48;
byte packetBuffer[NTP_BUFFER_SIZE];

bool sendNTPpacket(const char* address) {
    // Initialize NTP UDP
    ntpUDP.begin(8888); // Local port

    // Set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_BUFFER_SIZE);

    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // Send packet
    if (ntpUDP.beginPacket(address, 123) == 1) {
        ntpUDP.write(packetBuffer, NTP_BUFFER_SIZE);
        if (ntpUDP.endPacket() == 1) {
            LOG_DEBUG("NTP", "Sent NTP packet to %s", address);
            return true;
        }
    }

    LOG_ERROR("NTP", "Failed to send NTP packet to %s", address);
    return false;
}

bool syncNTP() {
    if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
        LOG_ERROR("NTP", "Ethernet not connected - cannot sync");
        timeSynced = false;
        return false;
    }

    // Debug DNS
    LOG_DEBUG("NTP", "DNS IP: %s", ETH.dnsIP().toString().c_str());

    LOG_INFO("NTP", "Starting NTP sync with servers");
    const char* additionalServers[] = {"0.europe.pool.ntp.org", "time.cloudflare.com"};
    const int totalServers = NTP_SERVER_COUNT + 2;

    for (int i = 0; i < totalServers; i++) {
        const char* server;
        if (i < NTP_SERVER_COUNT) {
            server = ntpServers[i];
        } else {
            server = additionalServers[i - NTP_SERVER_COUNT];
        }

        LOG_INFO("NTP", "Trying server %d: %s", i+1, server);

        // Send NTP packet
        if (sendNTPpacket(server)) {
            // Wait for response
            delay(1000);

            if (ntpUDP.parsePacket()) {
                LOG_DEBUG("NTP", "Received NTP response");
                ntpUDP.read(packetBuffer, NTP_BUFFER_SIZE);

                // Parse NTP time
                unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
                unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
                unsigned long secsSince1900 = highWord << 16 | lowWord;

                LOG_DEBUG("NTP", "Seconds since Jan 1 1900 = %lu", secsSince1900);

                // Convert to Unix time
                const unsigned long seventyYears = 2208988800UL;
                unsigned long epoch = secsSince1900 - seventyYears;

                LOG_DEBUG("NTP", "Unix time = %lu", epoch);

                if (epoch > 1609459200) { // Check if time is after 2021-01-01
                    // Set system time
                    struct timeval tv;
                    tv.tv_sec = epoch;
                    tv.tv_usec = 0;
                    settimeofday(&tv, NULL);

                    // Update ezTime
                    setTime(epoch);  // global funkcija ezTime, nastavi internal UTC čas

                    LOG_DEBUG("NTP", "UTC: %s", UTC.dateTime().c_str());
                    LOG_DEBUG("NTP", "Local: %s", myTZ.dateTime().c_str());

                    LOG_INFO("NTP", "SUCCESS with %s", server);
                    LOG_INFO("NTP", "Current time: %s", myTZ.dateTime().c_str());
                    timeSynced = true;
                    return true;
                } else {
                    LOG_ERROR("NTP", "FAILED with %s (invalid time: %lu)", server, epoch);
                }
            } else {
                LOG_WARN("NTP", "No response from %s", server);
            }
        } else {
            LOG_ERROR("NTP", "Failed to send packet to %s", server);
        }

        delay(1000); // Small delay between attempts
    }

    LOG_ERROR("NTP", "All servers failed - time not synchronized");
    LOG_INFO("NTP", "Possible causes: Firewall blocking UDP port 123, DNS issues, or network restrictions");
    timeSynced = false;
    return false;
}



// Ethernet event handler
void onEvent(arduino_event_id_t event) {
    switch (event) {
        case ETH_START:
            LOG_INFO("ETH", "Started");
            break;
        case ETH_CONNECTED:
            LOG_INFO("ETH", "Connected");
            break;
        case ETH_GOT_IP:
            LOG_INFO("ETH", "IP: %s", ETH.localIP().toString().c_str());
            break;
        case ETH_DISCONNECTED:
            LOG_WARN("ETH", "Disconnected");
            break;
        case ETH_STOP:
            LOG_WARN("ETH", "Stopped");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize logging system early - only needs Serial and RAM
    initLogging();
    LOG_INFO("System", "Starting vent_CEE...");

    // Register Ethernet event handler
    WiFi.onEvent(onEvent);

    // Initialize Ethernet with W5500
    bool ethInitialized = ETH.begin(ETH_PHY_W5500, 1, 14, 10, 9, HSPI_HOST, 13, 12, 11);
    if (!ethInitialized) {
        LOG_ERROR("ETH", "Init failed! Continuing without network...");
    } else {
        // Set static IP
        ETH.config(IPAddress(192,168,2,192), IPAddress(192,168,2,1), IPAddress(255,255,255,0), IPAddress(192,168,2,1));

        // Wait for IP with timeout (20 seconds)
        unsigned long startTime = millis();
        const unsigned long IP_TIMEOUT = 20000; // 20 seconds

        while (ETH.localIP() == IPAddress(0, 0, 0, 0) && (millis() - startTime) < IP_TIMEOUT) {
            delay(1000);
            LOG_INFO("ETH", "Waiting for IP...");
        }

        if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
            LOG_ERROR("ETH", "IP timeout! Continuing without network connection...");
        } else {
            LOG_INFO("ETH", "Static IP assigned: %s", ETH.localIP().toString().c_str());
        }
    }

    // Setup NTP
    LOG_INFO("NTP", "Setting up NTP...");
    setupNTP();
    syncNTP();  // Try to sync NTP on startup
    LOG_INFO("NTP", "Setup complete");

    // Initialize SD card
    LOG_INFO("SD", "Initializing SD card...");
    initSD();
    LOG_INFO("SD", "Initialization complete");

    // Initialize sensors
    initSensors();

    // Initialize inputs
    setupInputs();

    // Initialize vent outputs
    setupVent();

    // Load settings from NVS
    loadSettings();

    // Initialize currentData to default values
    initCurrentData();

    // Setup and start web server
    setupWebServer();

    // Initial device status check
    checkAllDevices();
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

    // Periodic input reading and vent control - every 200ms
    static unsigned long lastInputRead = 0;
    if (now - lastInputRead >= 200) {  // Every 200ms
        readInputs();
        controlBathroom();
        controlUtility();
        controlWC();
        controlLivingRoom();
        controlFans();
        lastInputRead = now;
    }



    // Check and send STATUS_UPDATE to REW
    checkAndSendStatusUpdate();

    // Check REW sensor data timeout
    static unsigned long lastDataTimeoutCheck = 0;
    if (timeSynced && externalDataValid && now - lastDataTimeoutCheck > 300000) {  // Check every 5 minutes
        lastDataTimeoutCheck = now;
        if ((myTZ.now() - lastSensorDataTime) > 900) {  // 15 minutes = 900 seconds
            LOG_ERROR("HTTP", "REW sensor data timeout - no data for 15+ minutes, invalidating external data");
            externalDataValid = false;
        }
    }

    // Periodic device status check and log maintenance - every 5 minutes
    static unsigned long lastDeviceCheck = 0;
    const unsigned long DEVICE_CHECK_INTERVAL = 300000; // 5 minutes
    if (now - lastDeviceCheck > DEVICE_CHECK_INTERVAL) {
        lastDeviceCheck = now;
        LOG_DEBUG("System", "Log buffer size: %d bytes", logBuffer.length());
        flushLogBuffer();
        checkAllDevices();
    }

    // Periodic network retry - every 5 minutes if no network
    static unsigned long lastNetworkRetry = 0;
    const unsigned long NETWORK_RETRY_INTERVAL = 300000; // 5 minutes
    if (ETH.localIP() == IPAddress(0, 0, 0, 0) &&
        (now - lastNetworkRetry) > NETWORK_RETRY_INTERVAL) {
        lastNetworkRetry = now;
        LOG_INFO("ETH", "Retrying network connection...");

        if (ETH.begin(ETH_PHY_W5500, 1, 14, 10, 9, HSPI_HOST, 13, 12, 11)) {
            ETH.config(IPAddress(192,168,2,192), IPAddress(192,168,2,1), IPAddress(255,255,255,0), IPAddress(192,168,2,1));
            LOG_INFO("ETH", "Network reconnected!");
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
