#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>

// Log level constants
enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3
};

void logEvent(const char* message);
void logEvent(LogLevel level, const char* tag, const char* format, ...);
void initLogging(void);
void flushLogBuffer(void);
bool sendLogsToREW(void);

// Convenience macros for common logging patterns
#define LOG_INFO(tag, format, ...) logEvent((LogLevel)LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define LOG_WARN(tag, format, ...) logEvent((LogLevel)LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define LOG_ERROR(tag, format, ...) logEvent((LogLevel)LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define LOG_DEBUG(tag, format, ...) logEvent((LogLevel)LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)

#endif