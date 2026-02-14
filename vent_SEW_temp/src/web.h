// web.h

#ifndef WEB_H
#define WEB_H

#include <ESPAsyncWebServer.h>

// Deklaracija funkcij za spletni strežnik
void setupWebServer();

// Deklaracija globalnih spremenljivk iz senzor.cpp, ki jih spletni strežnik potrebuje
extern bool sht41Present;
extern bool bmePresent;
extern bool tcsPresent;
extern float lastTempSHT;
extern float lastTempBME;
extern float lastHumSHT;
extern float lastHumBME;
extern float lastPressure;
extern float lastVOC;
extern uint16_t lastLux;
extern uint16_t lastCCT;
extern uint16_t lastR;
extern uint16_t lastG;
extern uint16_t lastB;

// Deklaracija funkcije za NTP čas iz senzor.cpp
extern unsigned long getCurrentTime();

#endif // WEB_H