// vent.cpp - Fan control implementation for CEE

#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "logging.h"
#include "vent.h"
#include "system.h"

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

    // SSR relay test sequence - turn on each relay for 0.5 seconds in order
    const int relayPins[] = {PIN_KOPALNICA_ODVOD, PIN_UTILITY_ODVOD, PIN_WC_ODVOD, PIN_SKUPNI_VPIH,
                             PIN_DNEVNI_VPIH, PIN_DNEVNI_ODVOD_1, PIN_DNEVNI_ODVOD_2, PIN_DNEVNI_ODVOD_3};
    const char* relayNames[] = {"KOP odvod", "UT odvod", "WC odvod", "Skupni vpih",
                                "DS vpih", "DS odvod stopnja 1", "DS odvod stopnja 2", "DS odvod stopnja 3"};

    logEvent("[Setup] Starting SSR relay test sequence");
    for (int i = 0; i < 8; i++) {
        char logMessage[100];
        snprintf(logMessage, sizeof(logMessage), "[Setup] Testing SSR %d (GPIO %d) - %s", i+1, relayPins[i], relayNames[i]);
        logEvent(logMessage);
        digitalWrite(relayPins[i], HIGH);
        delay(500);
        digitalWrite(relayPins[i], LOW);
        delay(100); // Short pause between tests
    }
    logEvent("[Setup] SSR relay test sequence completed");
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
    static bool lastLightOn = lastLightState1 || lastLightState2;
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
        lastLightOn = currentLightOn;
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
        lastLightOn = currentLightOn;
        return;
    }

    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp < settings.tempMinThreshold;
    bool reducedConditions = currentData.externalTemp < settings.tempLowThreshold;

    bool manualTriggerREW = currentData.manualTriggerBathroom && (!isDNDTime() || settings.dndAllowableManual);
    bool semiAutomaticTrigger = lastLightOn && !currentLightOn && (!isDNDTime() || settings.dndAllowableSemiautomatic);
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
    lastLightOn = currentLightOn;
}

void controlLivingRoom() {
    static unsigned long fanStartTime = 0;
    static unsigned long lastOffTime = 0;
    static unsigned long lastCycleLog = 0;
    static unsigned long lastCyclePercentUpdate = 0;
    static bool fanActive = false;
    static uint8_t currentLevel = 0;
    static bool manualMode = false;
    static bool isInitialized = false;
    static float cyclePercent = settings.cycleActivePercentDS;

    char logMessage[256];
    if (!isInitialized) {
        lastOffTime = millis();
        isInitialized = true;
    }

    // Če ni NTP sinhronizacije - samo lokalni triggerji delujejo
    if (!timeSynced) {
        // Handle manual triggers only
        if (currentData.manualTriggerLivingRoom) {
            if (fanActive) {
                digitalWrite(PIN_DNEVNI_VPIH, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
                fanActive = false;
                currentLevel = 0;
                manualMode = false;
                currentData.livingIntake = false;
                currentData.livingExhaustLevel = 0;
                currentData.offTimes[4] = 0;
                currentData.offTimes[5] = 0;
                lastOffTime = millis();
                snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Prekinitev z Man Trigg");
                logEvent(logMessage);
            }
            digitalWrite(PIN_DNEVNI_VPIH, HIGH);
            digitalWrite(PIN_DNEVNI_ODVOD_2, HIGH);
            fanActive = true;
            manualMode = true;
            fanStartTime = millis();
            currentLevel = 2;
            currentData.livingIntake = true;
            currentData.livingExhaustLevel = 2;
            unsigned long duration = settings.fanDuration * 2 * 1000;
            currentData.manualTriggerLivingRoom = false;
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] ON: Man Trigg via CYD, Trajanje: %u s", duration / 1000);
            logEvent(logMessage);
        }

        // Handle timeout for active fans
        if (fanActive && manualMode && (millis() - fanStartTime >= settings.fanDuration * 2 * 1000)) {
            digitalWrite(PIN_DNEVNI_VPIH, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
            fanActive = false;
            manualMode = false;
            currentLevel = 0;
            currentData.livingIntake = false;
            currentData.livingExhaustLevel = 0;
            currentData.offTimes[4] = 0;
            currentData.offTimes[5] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Man cikel konec");
            logEvent(logMessage);
        }

    // Handle window/open door disable
        if ((currentData.windowSensor1 || currentData.windowSensor2) || currentData.disableLivingRoom) {
            if (fanActive) {
                digitalWrite(PIN_DNEVNI_VPIH, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
                digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
                fanActive = false;
                currentLevel = 0;
                manualMode = false;
                currentData.livingIntake = false;
                currentData.livingExhaustLevel = 0;
                currentData.offTimes[4] = 0;
                currentData.offTimes[5] = 0;
                lastOffTime = millis();
                const char* reason = (!(currentData.windowSensor1 && currentData.windowSensor2)) ? "Okna" : "CYD";
                snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Diss via %s", reason);
                logEvent(logMessage);
            }
        }
        return;
    }

    bool isDND = isDNDTime();
    bool isNND = isNNDTime();
    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp > settings.tempExtremeHighDS ||
                            currentData.externalTemp < settings.tempExtremeLowDS;

    if (millis() - lastCyclePercentUpdate >= 60000) {
        cyclePercent = settings.cycleActivePercentDS;
        bool highIncrement = false;
        const char* reason = "aktiven";

        if (currentData.livingHumidity >= settings.humThresholdDS) {
            cyclePercent += settings.incrementPercentLowDS;
            reason = "H_DS>humThresholdDS";
        }
        if (currentData.livingHumidity >= settings.humThresholdHighDS) {
            cyclePercent += settings.incrementPercentHighDS;
            highIncrement = true;
            reason = "H_DS>humThresholdHighDS";
        }
        if (currentData.livingCO2 >= settings.co2ThresholdLowDS) {
            cyclePercent += settings.incrementPercentLowDS;
            reason = "CO2>co2ThresholdLowDS";
        }
        if (currentData.livingCO2 >= settings.co2ThresholdHighDS) {
            cyclePercent += settings.incrementPercentHighDS;
            highIncrement = true;
            reason = "CO2>co2ThresholdHighDS";
        }
        if (!isNND && ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
                       (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp))) {
            cyclePercent += settings.incrementPercentTempDS;
            reason = "T_DS!=tempIdealDS";
        }
        if (adverseConditions) {
            cyclePercent *= 0.5;
            reason = "Hext>humExtremeHighDS";
        }
        if ((currentData.windowSensor1 || currentData.windowSensor2)) {
            cyclePercent = 0.0;
            reason = "Okna";
        }
        if (currentData.disableLivingRoom) {
            cyclePercent = 0.0;
            reason = "CYD";
        }
        if (isNND) {
            cyclePercent = 0.0;
            reason = "NND";
        }
        lastCyclePercentUpdate = millis();
        currentData.livingRoomDutyCycle = cyclePercent; // Update global duty cycle
    }

    if (millis() - lastCycleLog >= 300000) {
        const char* reason = "aktiven";
        if ((currentData.windowSensor1 || currentData.windowSensor2)) reason = "Okna";
        else if (currentData.disableLivingRoom) reason = "CYD";
        else if (isNND) reason = "NND";
        snprintf(logMessage, sizeof(logMessage), "[DS] Cikel %.1f%%, %s, DND:%s, NND:%s, Stopnja:%d",
                 cyclePercent, reason, isDND ? "V" : "Zunaj", isNND ? "V" : "Zunaj", currentLevel);
        logEvent(logMessage);
        lastCycleLog = millis();
    }

    if ((currentData.windowSensor1 || currentData.windowSensor2) || currentData.disableLivingRoom) {
        if (fanActive) {
            digitalWrite(PIN_DNEVNI_VPIH, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
            fanActive = false;
            currentLevel = 0;
            manualMode = false;
            currentData.livingIntake = false;
            currentData.livingExhaustLevel = 0;
            currentData.offTimes[4] = 0;
            currentData.offTimes[5] = 0;
            lastOffTime = millis();
            const char* reason = ((currentData.windowSensor1 || currentData.windowSensor2)) ? "Okna" : "CYD";
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Diss via %s", reason);
            logEvent(logMessage);
        }
        return;
    }

    bool reducedConditions = currentData.externalTemp < settings.tempLowThreshold;
    static bool lastAdverseLogged = false;
    if (lastSensorRead > 0 && adverseConditions && !lastAdverseLogged) {
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Hext=%.1f%%>humExtremeHighDS=%.1f%%",
                 currentData.externalHumidity, settings.humExtremeHighDS);
        logEvent(logMessage);
        lastAdverseLogged = true;
    } else if (lastSensorRead > 0 && !adverseConditions && lastAdverseLogged) {
        lastAdverseLogged = false;
    }

    // Check for ignored manual triggers due to disabled state
    if (currentData.disableLivingRoom && currentData.manualTriggerLivingRoom) {
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] Manual trigger ignored - living room disabled");
        logEvent(logMessage);
        currentData.manualTriggerLivingRoom = false;
    }

    bool manualTrigger = currentData.manualTriggerLivingRoom && (!isDNDTime() || settings.dndAllowableManual);

    // Check for ignored manual triggers due to DND restrictions
    if (currentData.manualTriggerLivingRoom && !manualTrigger) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] Manual trigger ignored - DND dovoli ročno upravljanje = Disabled");
        logEvent(logMessage);
        currentData.manualTriggerLivingRoom = false;
    }

    if (manualTrigger && !(currentData.windowSensor1 || currentData.windowSensor2)) {
        if (fanActive) {
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Prekinitev z Man Trigg");
            logEvent(logMessage);
            digitalWrite(PIN_DNEVNI_VPIH, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
        }
        digitalWrite(PIN_DNEVNI_VPIH, HIGH);
        digitalWrite(PIN_DNEVNI_ODVOD_2, HIGH);
        fanActive = true;
        manualMode = true;
        fanStartTime = millis();
        currentLevel = 2;
        currentData.livingIntake = true;
        currentData.livingExhaustLevel = 2;
        unsigned long duration = settings.fanDuration * 2 * 1000;
        currentData.offTimes[4] = myTZ.now() + duration / 1000;
        currentData.offTimes[5] = currentData.offTimes[4];
        time_t offTime = currentData.offTimes[4];
        struct tm* tm = localtime(&offTime);
        char offTimeStr[20];
        strftime(offTimeStr, sizeof(offTimeStr), "%Y-%m-%d %H:%M:%S", tm);
        currentData.manualTriggerLivingRoom = false;
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] ON: Man Trigg via CYD, Trajanje: %u s, Izklop ob: %s", duration / 1000, offTimeStr);
        logEvent(logMessage);
    }

    if (fanActive && manualMode) {
        if (millis() - fanStartTime >= settings.fanDuration * 2 * 1000) {
            digitalWrite(PIN_DNEVNI_VPIH, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
            fanActive = false;
            manualMode = false;
            currentLevel = 0;
            currentData.livingIntake = false;
            currentData.livingExhaustLevel = 0;
            currentData.offTimes[4] = 0;
            currentData.offTimes[5] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Man cikel konec");
            logEvent(logMessage);
        }
        return;
    }

    bool automaticAllowed = !isDND || settings.dndAllowableAutomatic;
    if (!automaticAllowed || isNND) {
        if (fanActive && !manualMode) {
            digitalWrite(PIN_DNEVNI_VPIH, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
            digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
            fanActive = false;
            currentLevel = 0;
            currentData.livingIntake = false;
            currentData.livingExhaustLevel = 0;
            currentData.offTimes[4] = 0;
            currentData.offTimes[5] = 0;
            lastOffTime = millis();
            snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Auto cikel ustavljen (DND/NND)");
            logEvent(logMessage);
        }
        return;
    }

    unsigned long cycleDurationMs = settings.cycleDurationDS * 1000;
    unsigned long activeDurationMs = cycleDurationMs * (cyclePercent / 100.0);
    unsigned long inactiveDurationMs = cycleDurationMs - activeDurationMs;

    if (!fanActive && millis() - lastOffTime >= inactiveDurationMs && externalDataValid) {
        digitalWrite(PIN_DNEVNI_VPIH, HIGH);
        digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
        digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
        digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
        bool highIncrement = currentData.livingHumidity >= settings.humThresholdHighDS ||
                             currentData.livingCO2 >= settings.co2ThresholdHighDS;
        uint8_t newLevel = isDND ? 1 : (highIncrement ? 3 : 1);
        if (newLevel == 1) digitalWrite(PIN_DNEVNI_ODVOD_1, HIGH);
        else if (newLevel == 3) digitalWrite(PIN_DNEVNI_ODVOD_3, HIGH);
        fanActive = true;
        currentLevel = newLevel;
        fanStartTime = millis();
        currentData.livingIntake = true;
        currentData.livingExhaustLevel = newLevel;
        unsigned long duration = activeDurationMs;
        currentData.offTimes[4] = myTZ.now() + duration / 1000;
        currentData.offTimes[5] = currentData.offTimes[4];
        time_t offTime = currentData.offTimes[4];
        struct tm* tm = localtime(&offTime);
        char offTimeStr[20];
        strftime(offTimeStr, sizeof(offTimeStr), "%Y-%m-%d %H:%M:%S", tm);
        const char* reason = highIncrement ? "H_DS=%.1f%%>humThresholdHighDS=%.1f%%" : "Auto cikel";
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] ON: %s, Stopnja %d, Trajanje: %u s, Izklop ob: %s", reason, newLevel, duration / 1000, offTimeStr);
        logEvent(logMessage);
    } else if (fanActive && millis() - fanStartTime >= activeDurationMs) {
        digitalWrite(PIN_DNEVNI_VPIH, LOW);
        digitalWrite(PIN_DNEVNI_ODVOD_1, LOW);
        digitalWrite(PIN_DNEVNI_ODVOD_2, LOW);
        digitalWrite(PIN_DNEVNI_ODVOD_3, LOW);
        fanActive = false;
        currentLevel = 0;
        currentData.livingIntake = false;
        currentData.livingExhaustLevel = 0;
        currentData.offTimes[4] = 0;
        currentData.offTimes[5] = 0;
        lastOffTime = millis();
        snprintf(logMessage, sizeof(logMessage), "[DS Vent] OFF: Auto cikel konec");
        logEvent(logMessage);
    }
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