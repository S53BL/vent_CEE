#ifndef WEB_H
#define WEB_H

#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

void handleRoot(AsyncWebServerRequest *request);
void handleDataRequest(AsyncWebServerRequest *request);
void handlePostSettings(AsyncWebServerRequest *request);
void handleResetSettings(AsyncWebServerRequest *request);
void handleSettingsStatus(AsyncWebServerRequest *request);
void setupWebServer();

#endif // WEB_H