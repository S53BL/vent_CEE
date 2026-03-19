// logging.cpp

#include "logging.h"
#include <Arduino.h>
#include <stdarg.h>
#include "globals.h"
#include "config.h"

void logEvent(const char* message) {
    unsigned long timestamp;
    if (timeSynced) {
        timestamp = myTZ.now();  // Unix čas za zgodovinsko primerjavo
    } else {
        timestamp = millis() / 1000;  // Fallback v sekundah od boot-a
    }

    char entry[256];
    snprintf(entry, sizeof(entry), "[%lu] %s", timestamp, message);
    Serial.println(entry);

    if (loggingInitialized) {
        String ts = String(timestamp);
        String logLine = ts + "|CEE|" + String(message) + "\n";
        logBuffer += logLine;
    }
}

void logEvent(LogLevel level, const char* tag, const char* format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Create formatted message with level and tag
    char fullMessage[256];
    const char* levelStr = "";
    switch (level) {
        case LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
        case LOG_LEVEL_INFO:  levelStr = "INFO";  break;
        case LOG_LEVEL_WARN:  levelStr = "WARN";  break;
        case LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
    }

    // Always include the level for consistency
    snprintf(fullMessage, sizeof(fullMessage), "[%s:%s] %s", tag, levelStr, message);

    // Use the existing logEvent function to handle the rest
    logEvent(fullMessage);
}

void initLogging(void) {
    logBuffer.clear();
    loggingInitialized = true;
    lastLogFlush = millis();
}

void flushLogBuffer(void) {
    if (!loggingInitialized) return;

    size_t len = logBuffer.length();
    float pct = (len * 100.0f) / LOG_THRESHOLD_IDLE;

    // Static tracking za zaporedne neuspehe
    static int consecutiveFailures = 0;
    static unsigned long lastFailureTime = 0;

    if (len == 0) {
        LOG_DEBUG("LOG", "buffer: prazen");
        lastLogFlush = millis();
        return;
    }

    // Dosežen MAX — pošlji v vsakem primeru (OOM zaščita že implementirana)
    if (len >= LOG_BUFFER_MAX) {
        float pctMax = (len * 100.0f) / LOG_BUFFER_MAX;
        LOG_WARN("LOG", "buffer MAX: %d B (%.0f%%) — pošiljam v vsakem primeru", (int)len, pctMax);
        bool success = sendLogsToREW();
        if (success) {
            consecutiveFailures = 0;  // Reset counter
        } else {
            consecutiveFailures++;
            lastFailureTime = millis();
            LOG_ERROR("LOG", "buffer MAX: pošiljanje neuspešno (fail #%d) — buffer izbrisan (OOM zaščita)", consecutiveFailures);
            logBuffer.clear();
        }
        lastLogFlush = millis();
        return;
    }

    // Dosežen prag — pošlji samo če idle
    if (len >= LOG_THRESHOLD_IDLE) {
        if (isIdle()) {
            LOG_INFO("LOG", "buffer: %d B (%.0f%%) idle=DA — pošiljam", (int)len, pct);
            bool success = sendLogsToREW();
            
            // Track success/failure
            if (success) {
                if (consecutiveFailures > 0) {
                    LOG_INFO("LOG", "Recovery: logs successfully sent after %d failures", consecutiveFailures);
                }
                consecutiveFailures = 0;
            } else {
                consecutiveFailures++;
                lastFailureTime = millis();
                LOG_WARN("HTTP", "Log send failed (fail #%d) - buffer %.1f kB retained, will retry", 
                         consecutiveFailures, len / 1024.0);
                
                // Če dolgotrajno failajo (5+), zmanjšaj agresivnost log-anja
                if (consecutiveFailures >= 5) {
                    LOG_ERROR("HTTP", "Persistent log failures (%d consecutive) - REW connectivity issue?", 
                              consecutiveFailures);
                }
            }
        } else {
            LOG_INFO("LOG", "buffer: %d B (%.0f%%) idle=NE (wc=%d bat=%d ut=%d luc=%d/%d/%d/%d) — čakam",
                     (int)len, pct,
                     currentData.wcFan ? 1 : 0,
                     currentData.bathroomFan ? 1 : 0,
                     currentData.utilityFan ? 1 : 0,
                     currentData.wcLight ? 1 : 0,
                     currentData.bathroomLight1 ? 1 : 0,
                     currentData.bathroomLight2 ? 1 : 0,
                     currentData.utilityLight ? 1 : 0);
        }
        lastLogFlush = millis();
        return;
    }

    // Pod pragom — status z info o consecutive failures če obstajajo
    if (consecutiveFailures > 0) {
        LOG_INFO("LOG", "buffer: %d B (%.0f%%) - %d consecutive failures", (int)len, pct, consecutiveFailures);
    } else {
        LOG_INFO("LOG", "buffer: %d B (%.0f%%)", (int)len, pct);
    }
    lastLogFlush = millis();
}
