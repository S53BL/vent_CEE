// vent.cpp - Fan control implementation for CEE

#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "logging.h"
#include "vent.h"
#include "system.h"

// Forward declarations for helper functions
bool checkAutomaticPreconditions();
float calculateDutyCycle();
void handleCycleTiming(float cyclePercent, bool canRunAutomatic, bool& fanActive, bool& manualMode,
                       uint8_t& currentLevel, unsigned long& fanStartTime, unsigned long& lastOffTime);
void handleManualTriggerLivingRoom(bool& fanActive, bool& manualMode, unsigned long& fanStartTime, uint8_t& currentLevel);
bool checkManualPreconditions();
void handleManualTimeout(bool& fanActive, bool& manualMode, unsigned long fanStartTime, uint8_t& currentLevel);
void handleDisableConditions(bool& fanActive, bool& manualMode, uint8_t& currentLevel);
void startLivingRoomFan(float cyclePercent, bool& fanActive, uint8_t& currentLevel, unsigned long& fanStartTime);
void startManualLivingRoomFan(bool& fanActive, bool& manualMode, uint8_t& currentLevel, unsigned long& fanStartTime);
void stopLivingRoomFan(bool& fanActive, bool& manualMode, uint8_t& currentLevel);
void logLivingRoomStatus(float cyclePercent, bool canRunAutomatic, uint8_t currentLevel);

void setupVent() {
    pinMode(PIN_KOPALNICA_ODVOD, OUTPUT);
    pinMode(PIN_UTILITY_ODVOD, OUTPUT);
    pinMode(PIN_WC_ODVOD, OUTPUT);
    pinMode(PIN_SKUPNI_VPIH, OUTPUT);
    pinMode(PIN_DNEVNI_VPIH, OUTPUT);
    pinMode(PIN_DNEVNI_ODVOD_1, OUTPUT);
    pinMode(PIN_DNEVNI_ODVOD_2, OUTPUT);
    pinMode(PIN_DNEVNI_ODVOD_3, OUTPUT);

    // Initialize all outputs to LOW (off)
    digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
    digitalWrite(PIN_UTILITY_ODVOD, LOW);
    digitalWrite(PIN_WC_ODVOD, LOW);
    digitalWrite(PIN_SKUPNI_VPIH, LOW);
    digitalWrite(PIN_DNEVNI_VPIH, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
}

void controlFans() {
    currentData.bathroomFan = digitalRead(PIN_KOPALNICA_ODVOD) == HIGH && !currentData.disableBathroom;
    currentData.utilityFan = digitalRead(PIN_UTILITY_ODVOD) == HIGH && !currentData.disableUtility;
    currentData.wcFan = digitalRead(PIN_WC_ODVOD) == HIGH;
    currentData.commonIntake = digitalRead(PIN_SKUPNI_VPIH) == HIGH;
    currentData.livingIntake = digitalRead(PIN_DNEVNI_VPIH) == HIGH && !currentData.disableLivingRoom;
    currentData.livingExhaustLevel = 0;
    if (digitalRead(PIN_DNEVNI_ODVOD_3) == HIGH && !currentData.disableLivingRoom) currentData.livingExhaustLevel = 3;
    else if (digitalRead(PIN_DNEVNI_ODVOD_2) == HIGH && !currentData.disableLivingRoom) currentData.livingExhaustLevel = 2;
    else if (digitalRead(PIN_DNEVNI_ODVOD_1) == HIGH && !currentData.disableLivingRoom) currentData.livingExhaustLevel = 1;

    if (currentData.bathroomFan || currentData.utilityFan || currentData.wcFan) {
        digitalWrite(PIN_SKUPNI_VPIH, HIGH);
        currentData.commonIntake = true;
        if (currentData.commonIntake) currentData.offTimes[3] = currentData.bathroomFan ? currentData.offTimes[0] :
                                                                currentData.utilityFan ? currentData.offTimes[1] :
                                                                currentData.wcFan ? currentData.offTimes[2] : 0;
    } else {
        digitalWrite(PIN_SKUPNI_VPIH, LOW);
        currentData.commonIntake = false;
        currentData.offTimes[3] = 0;
    }
}

void controlWC() {
    static unsigned long fanStartTime = 0;
    static bool lastLightState = false;
    static bool fanActive = false;

    bool currentLightState = currentData.wcLight;

    // Če ni NTP sinhronizacije - samo lokalni triggerji delujejo
    if (!timeSynced) {
        // Handle manual triggers only
        char logMessage[256];
        if (currentData.manualTriggerWC) {
            digitalWrite(PIN_WC_ODVOD, HIGH);
            fanStartTime = millis();
            fanActive = true;
            currentData.wcFan = true;
            unsigned long duration = settings.fanDuration * 1000;
            snprintf(logMessage, sizeof(logMessage), "[WC Vent] ON: Man Trigg via REW, Trajanje: %u s", duration / 1000);
            logEvent(logMessage);
            currentData.manualTriggerWC = false;
        }

        // Handle timeout for active fans
        if (fanActive && (millis() - fanStartTime >= settings.fanDuration * 1000)) {
            digitalWrite(PIN_WC_ODVOD, LOW);
            fanActive = false;
            currentData.wcFan = false;
            currentData.offTimes[2] = 0;
            snprintf(logMessage, sizeof(logMessage), "[WC Vent] OFF: Cikel konec");
            logEvent(logMessage);
        }

        lastLightState = currentLightState;
        return;
    }

    // Check for ignored manual triggers due to disabled state
    if (currentData.disableWc && currentData.manualTriggerWC) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] Manual trigger ignored - WC disabled");
        logEvent(logMessage);
        currentData.manualTriggerWC = false;
    }

    bool manualTrigger = currentData.manualTriggerWC && (!isDNDTime() || settings.dndAllowableManual);
    bool semiAutomaticTrigger = lastLightState && !currentLightState && (!isDNDTime() || settings.dndAllowableSemiautomatic);

    // Check for ignored manual triggers due to fan already active
    if (manualTrigger && fanActive) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] Manual trigger ignored - fan already active");
        logEvent(logMessage);
        currentData.manualTriggerWC = false;
    }

    char logMessage[256];
    if ((manualTrigger || (semiAutomaticTrigger && settings.dndAllowableSemiautomatic)) && !fanActive) {
        digitalWrite(PIN_WC_ODVOD, HIGH);
        fanStartTime = millis();
        fanActive = true;
        currentData.wcFan = true;
        unsigned long duration = settings.fanDuration * 1000;
        currentData.offTimes[2] = myTZ.now() + duration / 1000;
        const char* triggerType = manualTrigger ? "Man Trigg" : "SemiAuto Trigg (Luč OFF)";
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] ON: %s, Trajanje: %u s", triggerType, duration / 1000);
        logEvent(logMessage);
        if (manualTrigger) {
            currentData.manualTriggerWC = false;
        }
    } else if (semiAutomaticTrigger && isDNDTime() && !settings.dndAllowableSemiautomatic) {
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] OFF: SemiAuto Trigg zavrnjen (DND)");
        logEvent(logMessage);
    }

    if (fanActive && (millis() - fanStartTime >= settings.fanDuration * 1000)) {
        digitalWrite(PIN_WC_ODVOD, LOW);
        fanActive = false;
        currentData.wcFan = false;
        currentData.offTimes[2] = 0;
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] OFF: Cikel konec");
        logEvent(logMessage);
    }

    lastLightState = currentLightState;
}

void controlUtility() {
    static unsigned long fanStartTime = 0;
    static unsigned long lastOffTime = 0;
    static bool fanActive = false;
    static bool lastLightState = false;
    static bool lastSwitchState = false;

    bool currentLightState = currentData.utilityLight;
    bool currentSwitchState = currentData.utilitySwitch;

    // Če ni NTP sinhronizacije - samo lokalni triggerji delujejo
    if (!timeSynced) {
        // Handle switch toggles and manual triggers only
        char logMessage[256];
        if (currentSwitchState != lastSwitchState) {
            if (!currentSwitchState) {
                currentData.disableUtility = true;
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] Diss via SW");
                logEvent(logMessage);
            } else {
                currentData.disableUtility = false;
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] Enab via SW");
                logEvent(logMessage);
            }
        }

        if (currentData.disableUtility) {
            if (fanActive) {
                digitalWrite(PIN_UTILITY_ODVOD, LOW);
                fanActive = false;
                currentData.utilityFan = false;
                currentData.offTimes[1] = 0;
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Diss via REW/SW");
                logEvent(logMessage);
            }
        } else if (currentData.manualTriggerUtility) {
            digitalWrite(PIN_UTILITY_ODVOD, HIGH);
            fanStartTime = millis();
            fanActive = true;
            currentData.utilityFan = true;
            unsigned long duration = settings.fanDuration * 1000;
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] ON: Man Trigg via REW, Trajanje: %u s", duration / 1000);
            logEvent(logMessage);
            currentData.manualTriggerUtility = false;
        }

        // Handle timeout for active fans
        if (fanActive && (millis() - fanStartTime >= settings.fanDuration * 1000)) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            fanActive = false;
            currentData.utilityFan = false;
            currentData.offTimes[1] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Cikel konec");
            logEvent(logMessage);
        }

        lastLightState = currentLightState;
        lastSwitchState = currentSwitchState;
        return;
    }

    bool manualTrigger = currentData.manualTriggerUtility && (!isDNDTime() || settings.dndAllowableManual);
    bool semiAutomaticTrigger = lastLightState && !currentLightState && (!isDNDTime() || settings.dndAllowableSemiautomatic);
    bool automaticTrigger = currentData.utilityHumidity >= settings.humThreshold &&
                           (!isDNDTime() || settings.dndAllowableAutomatic) &&
                           (millis() - lastOffTime >= settings.fanOffDuration / 2 * 1000 || !fanActive) &&
                           externalDataValid;

    // Check for ignored manual triggers due to DND restrictions
    if (currentData.manualTriggerUtility && !manualTrigger) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual trigger ignored - DND dovoli ročno upravljanje = Disabled");
        logEvent(logMessage);
        currentData.manualTriggerUtility = false;
    }

    // Check for ignored manual triggers due to fan already active
    if (manualTrigger && fanActive) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual trigger ignored - fan already active");
        logEvent(logMessage);
        currentData.manualTriggerUtility = false;
    }

    char logMessage[256];

    // Check for ignored manual triggers due to disabled state
    if (currentData.disableUtility && currentData.manualTriggerUtility) {
        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual trigger ignored - utility disabled");
        logEvent(logMessage);
        currentData.manualTriggerUtility = false;
    }

    if (currentSwitchState != lastSwitchState) {
        if (!currentSwitchState) {
            currentData.disableUtility = true;
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Diss via SW");
            logEvent(logMessage);
        } else {
            currentData.disableUtility = false;
            manualTrigger = true;
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Enab via SW, Man Trigg");
            logEvent(logMessage);
        }
    }

    if (currentData.disableUtility) {
        if (fanActive) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            fanActive = false;
            currentData.utilityFan = false;
            currentData.offTimes[1] = 0;
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Diss via REW/SW");
            logEvent(logMessage);
        }
        lastLightState = currentLightState;
        lastSwitchState = currentSwitchState;
        return;
    }

    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp < settings.tempMinThreshold;
    bool reducedConditions = currentData.externalTemp < settings.tempLowThreshold;

    if ((manualTrigger || semiAutomaticTrigger || automaticTrigger) && !adverseConditions && !fanActive) {
        digitalWrite(PIN_UTILITY_ODVOD, HIGH);
        fanStartTime = millis();
        fanActive = true;
        currentData.utilityFan = true;
        unsigned long duration = reducedConditions ? settings.fanDuration / 2 * 1000 : settings.fanDuration * 1000;
        currentData.offTimes[1] = myTZ.now() + duration / 1000;
        const char* triggerType = manualTrigger ? "Man Trigg via SW" :
                                 (semiAutomaticTrigger ? "SemiAuto Trigg (Luč OFF)" : "Auto Trigg");
        if (manualTrigger) {
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] ON: %s, Trajanje: %u s",
                     triggerType, duration / 1000);
        } else {
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] ON: %s, Hut=%.1f%%>humThreshold=%.1f%%, Trajanje: %u s",
                     triggerType, currentData.utilityHumidity, settings.humThreshold, duration / 1000);
        }
        logEvent(logMessage);
        if (manualTrigger || (lastSwitchState == false && currentSwitchState == true)) {
            currentData.manualTriggerUtility = false;
        }
    } else if ((manualTrigger || semiAutomaticTrigger || automaticTrigger) && adverseConditions) {
        if (lastSensorRead > 0) {
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Hext=%.1f%%>humExtremeHighDS=%.1f%%",
                     currentData.externalHumidity, settings.humExtremeHighDS);
            logEvent(logMessage);
        }
    }

    if (fanActive) {
        unsigned long duration = reducedConditions ? settings.fanDuration / 2 * 1000 : settings.fanDuration * 1000;
        if (millis() - fanStartTime >= duration) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            fanActive = false;
            currentData.utilityFan = false;
            currentData.offTimes[1] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Cikel konec (%u s)", duration / 1000);
            logEvent(logMessage);
        }
    }

    static unsigned long lastSensorCheck = 0;
    if (millis() - lastSensorCheck >= SENSOR_TEST_INTERVAL * 1000) {
        if (currentData.errorFlags & 0x02) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            fanActive = false;
            currentData.utilityFan = false;
            currentData.offTimes[1] = 0;
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Senzor napaka");
            logEvent(logMessage);
        }
        lastSensorCheck = millis();
    }

    lastLightState = currentLightState;
    lastSwitchState = currentSwitchState;
}

void controlBathroom() {
    static unsigned long fanStartTime = 0;
    static unsigned long lastOffTime = 0;
    static unsigned long buttonPressStart = 0;
    static bool fanActive = false;
    static bool lastButtonState = false;
    static bool lastLightState1 = false;
    static bool lastLightState2 = false;
    static bool isLongPress = false;
    static unsigned long lastSensorCheck = 0;

    bool currentButtonState = currentData.bathroomButton;
    bool currentLightState1 = currentData.bathroomLight1;
    bool currentLightState2 = currentData.bathroomLight2;
    bool currentLightOn = currentLightState1 || currentLightState2;

    // Če ni NTP sinhronizacije - samo lokalni triggerji delujejo
    if (!timeSynced) {
        // Handle manual button triggers only
        char logMessage[256];
        if (currentButtonState && !lastButtonState) {
            buttonPressStart = millis();
            snprintf(logMessage, sizeof(logMessage), "[KOP SW] Začetek pritiska");
            logEvent(logMessage);
        } else if (!currentButtonState && lastButtonState && buttonPressStart != 0) {
            unsigned long pressDuration = millis() - buttonPressStart;
            isLongPress = pressDuration > 1000;
            snprintf(logMessage, sizeof(logMessage), "[KOP SW] Konec pritiska (%u ms)", pressDuration);
            logEvent(logMessage);
            buttonPressStart = 0;
            if (millis() - lastOffTime < 2000) {
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Zaporedni pritisk ignoriran");
                logEvent(logMessage);
            } else {
                if (fanActive) {
                    snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Prekinitev z Man Trigg");
                    logEvent(logMessage);
                    digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
                    fanActive = false;
                    currentData.bathroomFan = false;
                    currentData.offTimes[0] = 0;
                }
                digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
                fanStartTime = millis();
                fanActive = true;
                currentData.bathroomFan = true;
                unsigned long duration = isLongPress ? settings.fanDuration * 2 * 1000 : settings.fanDuration * 1000;
                const char* triggerType = isLongPress ? "Man Trigg (dolg)" : "Man Trigg (kratek)";
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: %s, Trajanje: %u s", triggerType, duration / 1000);
                logEvent(logMessage);
            }
        }

        // Handle timeout for active fans
        if (fanActive && (millis() - fanStartTime >= (isLongPress ? settings.fanDuration * 2 * 1000 : settings.fanDuration * 1000))) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Cikel konec");
            logEvent(logMessage);
        }

        lastButtonState = currentButtonState;
        lastLightState1 = currentLightState1;
        lastLightState2 = currentLightState2;
        return;
    }

    char logMessage[256];

    // Check for ignored manual triggers due to disabled state
    if (currentData.disableBathroom && currentData.manualTriggerBathroom) {
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual trigger ignored - bathroom disabled");
        logEvent(logMessage);
        currentData.manualTriggerBathroom = false;
    }

    if (currentButtonState && !lastButtonState) {
        buttonPressStart = millis();
        snprintf(logMessage, sizeof(logMessage), "[KOP SW] Začetek pritiska");
        logEvent(logMessage);
    } else if (!currentButtonState && lastButtonState && buttonPressStart != 0) {
        unsigned long pressDuration = millis() - buttonPressStart;
        isLongPress = pressDuration > 1000;
        snprintf(logMessage, sizeof(logMessage), "[KOP SW] Konec pritiska (%u ms)", pressDuration);
        logEvent(logMessage);
        buttonPressStart = 0;
        if (millis() - lastOffTime < 2000) {
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Zaporedni pritisk ignoriran");
            logEvent(logMessage);
        } else if (!isDNDTime() || settings.dndAllowableManual) {
            if (fanActive) {
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Prekinitev z Man Trigg");
                logEvent(logMessage);
                digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
                fanActive = false;
                currentData.bathroomFan = false;
                currentData.offTimes[0] = 0;
            }
            digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
            fanStartTime = millis();
            fanActive = true;
            currentData.bathroomFan = true;
            unsigned long duration = isLongPress ? settings.fanDuration * 2 * 1000 : settings.fanDuration * 1000;
            currentData.offTimes[0] = myTZ.now() + duration / 1000;
            const char* triggerType = isLongPress ? "Man Trigg (dolg)" : "Man Trigg (kratek)";
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: %s, Trajanje: %u s", triggerType, duration / 1000);
            logEvent(logMessage);
        } else {
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Man Trigg zavrnjen (DND)");
            logEvent(logMessage);
        }
    }

    if (currentData.disableBathroom) {
        if (fanActive) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Diss via REW");
            logEvent(logMessage);
        }
        lastButtonState = currentButtonState;
        lastLightState1 = currentLightState1;
        lastLightState2 = currentLightState2;
        return;
    }

    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp < settings.tempMinThreshold;
    bool reducedConditions = currentData.externalTemp < settings.tempLowThreshold;

    bool manualTriggerREW = currentData.manualTriggerBathroom && (!isDNDTime() || settings.dndAllowableManual);
    bool semiAutomaticTrigger = (lastLightState1 || lastLightState2) && !currentLightState1 && !currentLightState2 && (!isDNDTime() || settings.dndAllowableSemiautomatic);
    bool automaticTrigger = currentData.bathroomHumidity >= settings.humThreshold &&
                           (!isDNDTime() || settings.dndAllowableAutomatic) &&
                           (millis() - lastOffTime >= settings.fanOffDuration / 2 * 1000 || !fanActive) &&
                           externalDataValid;

    // Check for ignored manual triggers due to DND restrictions
    if (currentData.manualTriggerBathroom && !manualTriggerREW) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual trigger ignored - DND dovoli ročno upravljanje = Disabled");
        logEvent(logMessage);
        currentData.manualTriggerBathroom = false;
    }

    // Check for ignored manual triggers due to fan already active
    if (manualTriggerREW && fanActive) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual trigger ignored - fan already active");
        logEvent(logMessage);
        currentData.manualTriggerBathroom = false;
    }

    if ((manualTriggerREW || semiAutomaticTrigger || automaticTrigger) && !adverseConditions && !fanActive) {
        digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
        fanStartTime = millis();
        fanActive = true;
        currentData.bathroomFan = true;
        unsigned long duration = reducedConditions ? settings.fanDuration / 2 * 1000 : settings.fanDuration * 1000;
        currentData.offTimes[0] = myTZ.now() + duration / 1000;

        const char* triggerType;
        if (manualTriggerREW) {
            triggerType = "Man Trigg via REW";
            currentData.manualTriggerBathroom = false;
        } else if (semiAutomaticTrigger) {
            triggerType = "SemiAuto Trigg (Luč OFF)";
        } else {
            triggerType = "Auto Trigg";
        }

        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: %s, Hkop=%.1f%%>humThreshold=%.1f%%, Trajanje: %u s",
                 triggerType, currentData.bathroomHumidity, settings.humThreshold, duration / 1000);
        logEvent(logMessage);
    } else if ((semiAutomaticTrigger || automaticTrigger) && adverseConditions) {
        if (lastSensorRead > 0) {
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Hext=%.1f%%>humExtremeHighDS=%.1f%%",
                     currentData.externalHumidity, settings.humExtremeHighDS);
            logEvent(logMessage);
        }
    }

    if (fanActive) {
        unsigned long duration = (isLongPress && manualTriggerREW) ?
                                settings.fanDuration * 2 * 1000 :
                                (reducedConditions ? settings.fanDuration / 2 * 1000 : settings.fanDuration * 1000);
        if (millis() - fanStartTime >= duration) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Cikel konec (%u s)", duration / 1000);
            logEvent(logMessage);
        }
    }

    if (millis() - lastSensorCheck >= SENSOR_TEST_INTERVAL * 1000) {
        if (currentData.errorFlags & 0x01) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Senzor napaka");
            logEvent(logMessage);
        } else if (bmePresent && sht41Present && abs(currentData.bathroomTemp - currentData.utilityTemp) > 10.0) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Temp odstopanje >10°C");
            logEvent(logMessage);
        }
        lastSensorCheck = millis();
    }

    lastButtonState = currentButtonState;
    lastLightState1 = currentLightState1;
    lastLightState2 = currentLightState2;
}

void controlLivingRoom() {
    static unsigned long fanStartTime = 0;
    static unsigned long lastOffTime = 0;
    static unsigned long lastCycleLog = 0;
    static bool fanActive = false;
    static uint8_t currentLevel = 0;
    static bool manualMode = false;
    static bool isInitialized = false;

    if (!isInitialized) {
        lastOffTime = millis();
        isInitialized = true;
    }

    // Handle manual trigger
    handleManualTriggerLivingRoom(fanActive, manualMode, fanStartTime, currentLevel);

    // If no NTP sync, only manual mode works
    if (!timeSynced) {
        handleManualTimeout(fanActive, manualMode, fanStartTime, currentLevel);
        handleDisableConditions(fanActive, manualMode, currentLevel);
        return;
    }

    // Check all preconditions for automatic operation
    bool canRunAutomatic = checkAutomaticPreconditions();

    // Calculate duty cycle if preconditions met
    float cyclePercent = 0.0;
    if (canRunAutomatic) {
        cyclePercent = calculateDutyCycle();
    }

    // Handle cycle timing
    handleCycleTiming(cyclePercent, canRunAutomatic, fanActive, manualMode, currentLevel, fanStartTime, lastOffTime);

    // Log status every 5 minutes
    if (millis() - lastCycleLog >= 300000) {
        logLivingRoomStatus(cyclePercent, canRunAutomatic, currentLevel);
        lastCycleLog = millis();
    }
}

// Helper function: Check all preconditions for automatic DS ventilation
bool checkAutomaticPreconditions() {
    // Basic requirements
    if (!timeSynced || !externalDataValid) return false;

    // Disabled by user
    if (currentData.disableLivingRoom) return false;

    // Windows open
    if (currentData.windowSensor1 || currentData.windowSensor2) return false;

    // Time restrictions
    bool isDND = isDNDTime();
    bool isNND = isNNDTime();
    if (isNND) return false;
    if (isDND && !settings.dndAllowableAutomatic) return false;

    return true;
}

// Helper function: Calculate duty cycle from sensor data
float calculateDutyCycle() {
    float cyclePercent = settings.cycleActivePercentDS;

    // Humidity increments
    if (currentData.livingHumidity >= settings.humThresholdDS) {
        cyclePercent += settings.incrementPercentLowDS;
    }
    if (currentData.livingHumidity >= settings.humThresholdHighDS) {
        cyclePercent += settings.incrementPercentHighDS;
    }

    // CO2 increments
    if (currentData.livingCO2 >= settings.co2ThresholdLowDS) {
        cyclePercent += settings.incrementPercentLowDS;
    }
    if (currentData.livingCO2 >= settings.co2ThresholdHighDS) {
        cyclePercent += settings.incrementPercentHighDS;
    }

    // Temperature increment
    bool isNND = isNNDTime();
    if (!isNND && ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
                   (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp))) {
        cyclePercent += settings.incrementPercentTempDS;
    }

    // Adverse conditions reduction
    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp > settings.tempExtremeHighDS ||
                            currentData.externalTemp < settings.tempExtremeLowDS;
    if (adverseConditions) {
        cyclePercent *= 0.5;
    }

    // Cap at 100%
    if (cyclePercent > 100.0) cyclePercent = 100.0;

    // Update global duty cycle
    currentData.livingRoomDutyCycle = cyclePercent;

    return cyclePercent;
}

// Helper function: Handle cycle timing logic
void handleCycleTiming(float cyclePercent, bool canRunAutomatic, bool& fanActive, bool& manualMode,
                       uint8_t& currentLevel, unsigned long& fanStartTime, unsigned long& lastOffTime) {

    // Handle manual mode timeout
    if (fanActive && manualMode && (millis() - fanStartTime >= settings.fanDuration * 2 * 1000)) {
        stopLivingRoomFan(fanActive, manualMode, currentLevel);
        lastOffTime = millis();
        LOG_INFO("DS Vent", "OFF: Manual cycle end");
        return;
    }

    // If manual mode active, don't interfere
    if (manualMode) return;

    // If cannot run automatic, ensure fan is off
    if (!canRunAutomatic) {
        if (fanActive) {
            stopLivingRoomFan(fanActive, manualMode, currentLevel);
            lastOffTime = millis();
            LOG_INFO("DS Vent", "OFF: Automatic disabled");
        }
        return;
    }

    // Cycle timing
    unsigned long cycleDurationMs = settings.cycleDurationDS * 1000;
    unsigned long activeDurationMs = cycleDurationMs * (cyclePercent / 100.0);
    unsigned long inactiveDurationMs = cycleDurationMs - activeDurationMs;

    // Time to start fan
    if (!fanActive && millis() - lastOffTime >= inactiveDurationMs) {
        startLivingRoomFan(cyclePercent, fanActive, currentLevel, fanStartTime);
    }
    // Time to stop fan
    else if (fanActive && millis() - fanStartTime >= activeDurationMs) {
        stopLivingRoomFan(fanActive, manualMode, currentLevel);
        lastOffTime = millis();
        LOG_INFO("DS Vent", "OFF: Auto cycle end");
    }
}

// Helper function: Handle manual trigger
void handleManualTriggerLivingRoom(bool& fanActive, bool& manualMode, unsigned long& fanStartTime, uint8_t& currentLevel) {
    if (!currentData.manualTriggerLivingRoom) return;

    bool canActivate = checkManualPreconditions();
    if (!canActivate) {
        LOG_INFO("DS Vent", "Manual trigger ignored - preconditions not met");
        currentData.manualTriggerLivingRoom = false;
        return;
    }

    // Stop any current operation
    if (fanActive) {
        stopLivingRoomFan(fanActive, manualMode, currentLevel);
    }

    // Start manual mode
    startManualLivingRoomFan(fanActive, manualMode, currentLevel, fanStartTime);
    currentData.manualTriggerLivingRoom = false;
}

// Helper function: Check preconditions for manual activation
bool checkManualPreconditions() {
    if (currentData.disableLivingRoom) return false;
    if (currentData.windowSensor1 || currentData.windowSensor2) return false;
    if (timeSynced && isDNDTime() && !settings.dndAllowableManual) return false;
    return true;
}

// Helper function: Handle manual timeout when no NTP
void handleManualTimeout(bool& fanActive, bool& manualMode, unsigned long fanStartTime, uint8_t& currentLevel) {
    if (fanActive && manualMode && (millis() - fanStartTime >= settings.fanDuration * 2 * 1000)) {
        stopLivingRoomFan(fanActive, manualMode, currentLevel);
        LOG_INFO("DS Vent", "OFF: Manual cycle end (no NTP)");
    }
}

// Helper function: Handle disable conditions
void handleDisableConditions(bool& fanActive, bool& manualMode, uint8_t& currentLevel) {
    if ((currentData.windowSensor1 || currentData.windowSensor2) || currentData.disableLivingRoom) {
        if (fanActive) {
            stopLivingRoomFan(fanActive, manualMode, currentLevel);
            const char* reason = (currentData.windowSensor1 || currentData.windowSensor2) ? "Windows open" : "Disabled";
            LOG_INFO("DS Vent", "OFF: %s", reason);
        }
    }
}

// Helper function: Start living room fan
void startLivingRoomFan(float cyclePercent, bool& fanActive, uint8_t& currentLevel, unsigned long& fanStartTime) {
    digitalWrite(PIN_DNEVNI_VPIH, HIGH);
    digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);

    bool highIncrement = currentData.livingHumidity >= settings.humThresholdHighDS ||
                         currentData.livingCO2 >= settings.co2ThresholdHighDS;
    bool isDND = isDNDTime();
    uint8_t newLevel = isDND ? 1 : (highIncrement ? 3 : 1);

    if (newLevel == 1) digitalWrite(PIN_DNEVNI_ODVOD_1, HIGH);
    else if (newLevel == 3) digitalWrite(PIN_DNEVNI_ODVOD_3, HIGH);

    fanActive = true;
    currentLevel = newLevel;
    fanStartTime = millis();
    currentData.livingIntake = true;
    currentData.livingExhaustLevel = newLevel;

    unsigned long cycleDurationMs = settings.cycleDurationDS * 1000;
    unsigned long activeDurationMs = cycleDurationMs * (cyclePercent / 100.0);
    currentData.offTimes[4] = myTZ.now() + activeDurationMs / 1000;
    currentData.offTimes[5] = currentData.offTimes[4];

    char reason[64];
    if (highIncrement) {
        if (currentData.livingHumidity >= settings.humThresholdHighDS) {
            snprintf(reason, sizeof(reason), "H_DS=%.1f%%>humThresholdHighDS=%.1f%%", currentData.livingHumidity, settings.humThresholdHighDS);
        } else {
            snprintf(reason, sizeof(reason), "CO2=%.0f>co2ThresholdHighDS=%.0f", currentData.livingCO2, settings.co2ThresholdHighDS);
        }
    } else {
        strcpy(reason, "Auto cycle");
    }

    LOG_INFO("DS Vent", "ON: %s, Level %d, Duration: %u s", reason, newLevel, activeDurationMs / 1000);
}

// Helper function: Start manual living room fan
void startManualLivingRoomFan(bool& fanActive, bool& manualMode, uint8_t& currentLevel, unsigned long& fanStartTime) {
    digitalWrite(PIN_DNEVNI_VPIH, HIGH);
    digitalWrite(PIN_DNEVNI_ODVOD_2, HIGH);
    fanActive = true;
    manualMode = true;
    currentLevel = 2;
    fanStartTime = millis();
    currentData.livingIntake = true;
    currentData.livingExhaustLevel = 2;

    unsigned long duration = settings.fanDuration * 2 * 1000;
    if (timeSynced) {
        currentData.offTimes[4] = myTZ.now() + duration / 1000;
        currentData.offTimes[5] = currentData.offTimes[4];
    } else {
        currentData.offTimes[4] = 0;
        currentData.offTimes[5] = 0;
    }

    LOG_INFO("DS Vent", "ON: Manual trigger, Duration: %u s", duration / 1000);
}

// Helper function: Stop living room fan
void stopLivingRoomFan(bool& fanActive, bool& manualMode, uint8_t& currentLevel) {
    digitalWrite(PIN_DNEVNI_VPIH, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
    digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
    fanActive = false;
    manualMode = false;
    currentLevel = 0;
    currentData.livingIntake = false;
    currentData.livingExhaustLevel = 0;
    currentData.offTimes[4] = 0;
    currentData.offTimes[5] = 0;
}

// Helper function: Log living room status
void logLivingRoomStatus(float cyclePercent, bool canRunAutomatic, uint8_t currentLevel) {
    bool isDND = isDNDTime();
    bool isNND = isNNDTime();

    const char* status = "OK";
    if (!canRunAutomatic) {
        if (!timeSynced) status = "No NTP";
        else if (!externalDataValid) status = "No sensor data";
        else if (currentData.disableLivingRoom) status = "Disabled";
        else if (currentData.windowSensor1 || currentData.windowSensor2) status = "Windows open";
        else if (isNND) status = "NND";
        else if (isDND && !settings.dndAllowableAutomatic) status = "DND";
    }

    LOG_INFO("DS", "Cycle: %.1f%%, Status: %s, DND:%s, NND:%s, Level:%d",
             cyclePercent, status, isDND ? "YES" : "NO", isNND ? "YES" : "NO", currentLevel);
}

void calculatePower() {
    currentData.currentPower = 0.0;

    // Sum power consumption of all active fans
    if (currentData.bathroomFan) currentData.currentPower += FAN_POWER_BATHROOM;
    if (currentData.utilityFan) currentData.currentPower += FAN_POWER_UTILITY;
    if (currentData.wcFan) currentData.currentPower += FAN_POWER_WC;
    if (currentData.commonIntake) currentData.currentPower += FAN_POWER_COMMON_INTAKE;
    if (currentData.livingIntake) currentData.currentPower += FAN_POWER_LIVING_INTAKE;

    // Living room exhaust based on level
    if (currentData.livingExhaustLevel == 1) currentData.currentPower += FAN_POWER_LIVING_EXHAUST_1;
    else if (currentData.livingExhaustLevel == 2) currentData.currentPower += FAN_POWER_LIVING_EXHAUST_2;
    else if (currentData.livingExhaustLevel == 3) currentData.currentPower += FAN_POWER_LIVING_EXHAUST_3;
}

// vent.cpp