// system.cpp - Time restriction functions for CEE

#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "globals.h"
#include "logging.h"

bool isDNDTime() {
    if (!timeSynced) return false;  // Vedno dovoljeno če ni časa

    int hour = myTZ.hour();  // Lokalna ura

    // DND: 22:00 - 06:00
    return (hour >= 22 || hour < 6);
}

bool isNNDTime() {
    if (!timeSynced) return false;  // Vedno dovoljeno če ni časa

    int hour = myTZ.hour();  // Lokalna ura
    int day = myTZ.weekday() - 1;  // 0=Ned, 1=Pon, ..., 6=Sob

    // NND: 06:00 - 16:00 only on workdays (Mon-Fri)
    // NND_DAYS = {true, true, true, true, true, false, false}
    if (!NND_DAYS[day == 0 ? 6 : day - 1]) return false;  // Not a workday
    return (hour >= 6 && hour < 16);
}

// system.cpp