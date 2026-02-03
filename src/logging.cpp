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

    if (level == LOG_LEVEL_INFO) {
        // For INFO level, keep the simple format for backward compatibility
        snprintf(fullMessage, sizeof(fullMessage), "[%s] %s", tag, message);
    } else {
        // For other levels, include the level
        snprintf(fullMessage, sizeof(fullMessage), "[%s:%s] %s", tag, levelStr, message);
    }

    // Use the existing logEvent function to handle the rest
    logEvent(fullMessage);
}

void initLogging(void) {
    logBuffer.clear();
    loggingInitialized = true;
    lastLogFlush = millis();
}

void flushLogBuffer(void) {
    if (!loggingInitialized || logBuffer.length() == 0) return;
    size_t len = logBuffer.length();
    if ((len >= LOG_THRESHOLD_IDLE && isIdle()) || len >= LOG_BUFFER_MAX) {
        sendLogsToREW();
    }
    lastLogFlush = millis();
}
