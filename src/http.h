#ifndef HTTP_H
#define HTTP_H

#include <Arduino.h>

// HTTP client functions for sending messages
bool sendStatusUpdate();
bool sendDewUpdate(const char* room);
bool sendLogsToREW();
void checkAndSendStatusUpdate();

// Device status checking
bool checkDeviceOnline(const char* ip);
void checkAllDevices();

// Helper functions
int sendHttpPost(const char* url, const String& jsonData, int timeoutMs = 5000);
bool sendHttpPostWithRetry(const char* deviceName, const char* url, const String& jsonData, int maxRetries = 2, bool logResult = true);

#endif // HTTP_H