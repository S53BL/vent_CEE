// vent.cpp - Fan control implementation for CEE

#include <Arduino.h>
#include <cmath>
#include "config.h"
#include "globals.h"
#include "logging.h"
#include "vent.h"
#include "system.h"
#include "sens.h"

const unsigned long base_fan_duration_UT = 360000; // 360 s v ms

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
    // Semi-auto fires outside DND always, during DND only if allowed
    // (semiAutomaticTrigger already handles this correctly via !isDNDTime() || dndAllowableSemiautomatic)
    if ((manualTrigger || semiAutomaticTrigger) && !fanActive) {
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
    }
    // Log DND blocked semi-auto (potreben ločen check ker semiAutomaticTrigger že filtrira DND)
    if (lastLightState && !currentLightState && isDNDTime() && !settings.dndAllowableSemiautomatic) {
        snprintf(logMessage, sizeof(logMessage), "[WC Vent] SemiAuto Trigg zavrnjen (DND)");
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
    static time_t expected_end_timeUT = 0;
    static float utility_hum_history[10] = {0};
    static bool utility_drying_mode = false;
    static int utility_cycle_mode = 0;
    static int utility_burst_count = 0;
    static float utility_baseline_hum = 45.0; // privzeto 45 %
    static unsigned long off_start = 0;
    static unsigned long burst_start = 0;      // premaknjena sem za dostop v disable bloku
    static bool in_burst = false;              // premaknjena sem za dostop v disable bloku

    // Branje senzorja (vsakih 60 s, ampak klicano iz loop)
    // Predpostavi, da readSensors() posodobi currentData.utilityHumidity itd.

    // Update history (shift array)
    for (int i = 0; i < 9; i++) {
        utility_hum_history[i] = utility_hum_history[i+1];
    }
    utility_hum_history[9] = currentData.utilityHumidity;

    // Če disableUtility, prekini cikel
    if (currentData.disableUtility) {
        if (digitalRead(PIN_UTILITY_ODVOD) == HIGH) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            currentData.utilityFan = false;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Disable via switch/REW");
            logEvent(logMessage);
        }
        utility_drying_mode = false;
        utility_cycle_mode = 0;
        utility_burst_count = 0;
        in_burst = false;             // reset burst stanja pri disable
        currentData.offTimes[1] = 0;  // počisti off time
        return;
    }

    // Ignoriramo dupliciran drying trigger med aktivnim ciklom
    if (utility_drying_mode && currentData.manualTriggerUtilityDrying) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Drying trigger ignored - ze v drying ciklu");
        logEvent(logMessage);
        currentData.manualTriggerUtilityDrying = false;
    }

    // Čakanje na trigger
    if (!utility_drying_mode && utility_cycle_mode == 0) {
        // Določi trigger: manual ima prednost pred auto
        bool trigger_fired = false;
        bool is_manual_drying = false;
        float auto_trend = 0.0f;

        if (currentData.manualTriggerUtilityDrying) {
            if (!isDNDTime() || settings.dndAllowableManual) {
                trigger_fired = true;
                is_manual_drying = true;
            } else {
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual drying trigger zavrnjen (DND)");
                logEvent(logMessage);
            }
            currentData.manualTriggerUtilityDrying = false;
        } else {
            // Automatic trigger check
            float trend = utility_hum_history[9] - utility_hum_history[0];
            float rate = (utility_hum_history[9] - utility_hum_history[8]) / 1.0; // %/min
            float prev_rate = (utility_hum_history[8] - utility_hum_history[7]) / 1.0;
            bool stabilizing = (abs(rate) < 0.1 && abs(prev_rate) < 0.1);
            if (trend > 10.0 && utility_hum_history[9] > utility_baseline_hum + 10.0 && stabilizing) {
                trigger_fired = true;
                auto_trend = trend;
            }
        }

        if (trigger_fired) {
            int mode = determineCycleMode(currentData.utilityTemp, currentData.utilityHumidity, ERR_SHT41);
            if (mode > 0) {
                utility_drying_mode = true;
                utility_cycle_mode = mode;
                utility_burst_count = 0;
                float off_factor_temp = (mode == 1) ? 1.0 : (mode == 2) ? 1.5 : 2.0;
                off_start = millis() - (settings.fanOffDuration * off_factor_temp * 1000);
                time_t now = myTZ.now();
                unsigned long total_seconds = (3 * (base_fan_duration_UT * (mode == 1 ? 1.0 : mode == 2 ? 0.6 : 0.3) / 1000)) + (2 * (settings.fanOffDuration * off_factor_temp * 1000 / 1000));
                expected_end_timeUT = now + total_seconds;
                char expectedTimeStr[20];
                strftime(expectedTimeStr, sizeof(expectedTimeStr), "%H:%M:%S", localtime(&expected_end_timeUT));
                char logMessage[256];
                if (is_manual_drying) {
                    snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual drying trigger: Mode=%d End: %s", mode, expectedTimeStr);
                } else {
                    snprintf(logMessage, sizeof(logMessage), "[UT Vent] Auto trigger: Trend=%.1f%%, Mode=%d End: %s", auto_trend, mode, expectedTimeStr);
                }
                logEvent(logMessage);
            } else {
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] Trigger blocked (%s): Mode=%d",
                    is_manual_drying ? "manual" : "auto", mode);
                logEvent(logMessage);
            }
            if (is_manual_drying) return;  // manual drying vrne, auto nadaljuje
        }
        // Ne vrni return - manual trigger se mora preveriti spodaj
    }

    // Določitev duration in off
    float duration_factor = (utility_cycle_mode == 1) ? 1.0 : (utility_cycle_mode == 2) ? 0.6 : 0.3;
    float off_factor = (utility_cycle_mode == 1) ? 1.0 : (utility_cycle_mode == 2) ? 1.5 : 2.0;
    unsigned long fan_duration = base_fan_duration_UT * duration_factor; // ms // specifično za UT
    unsigned long fan_off_duration = settings.fanOffDuration * off_factor * 1000; // iz settings

    // Expected end time is calculated once at trigger, just copy to currentData

    if (utility_drying_mode) {
        if (isDNDTime() && !settings.dndAllowableAutomatic) {
            // Preskoči auto burst v DND
            return;
        }

        if (!in_burst) {
            bool ready = (utility_burst_count == 0) || (millis() - off_start >= fan_off_duration);
            if (ready) {
                // --- Preveri ponovitev / konec (za utility_burst_count >= 1) ---
                if (utility_burst_count >= 1) {
                    // Off end: log humidity and rate
                    float rate1 = (utility_hum_history[9] - utility_hum_history[8]) / 1.0;
                    float rate2 = (utility_hum_history[8] - utility_hum_history[7]) / 1.0;
                    float rate3 = (utility_hum_history[7] - utility_hum_history[6]) / 1.0;
                    float avg_rate = (rate1 + rate2 + rate3) / 3.0;
                    char logMessage[256];
                    snprintf(logMessage, sizeof(logMessage), "[UT Vent] Off end: Hum=%.1f%%, Avg_rate=%.2f", currentData.utilityHumidity, avg_rate);
                    logEvent(logMessage);
                    if (!(currentData.utilityHumidity > 65.0 && utility_burst_count < 3)) {
                        // Konec cikla
                        utility_drying_mode = false; utility_cycle_mode = 0;
                        utility_baseline_hum = 0;
                        for (int i = 0; i < 10; i++) {
                            utility_baseline_hum += utility_hum_history[i];
                        }
                        utility_baseline_hum /= 10.0;
                        const char* reason = (currentData.utilityHumidity <= 65.0) ? "Hum low" : "Bursts max";
                        char logMessage[256];
                        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Cycle end: Hum=%.1f%%, Bursts=%d, Baseline=%.1f%%, Reason=%s", currentData.utilityHumidity, utility_burst_count, utility_baseline_hum, reason);
                        logEvent(logMessage);
                        utility_burst_count = 0;
                        expected_end_timeUT = 0;
                        currentData.utilityExpectedEndTime = 0;
                        // Takoj posodobi globalní status - potrebno za change detection v STATUS_UPDATE
                        currentData.utilityDryingMode = false;
                        currentData.utilityCycleMode = 0;
                        return;  // izhod
                    } else {
                        // Prilagodi
                        if (avg_rate > 1.0) fan_off_duration *= 1.2;
                        else if (avg_rate < 0.2) fan_off_duration *= 0.8;
                        // Continue: log adjustment
                        char logMessage[256];
                        snprintf(logMessage, sizeof(logMessage), "[UT Vent] Continue: Adjust off to %lu s (rate=%.2f)", fan_off_duration/1000, avg_rate);
                        logEvent(logMessage);
                    }
                }
                // --- Zaženi burst ---
                digitalWrite(PIN_UTILITY_ODVOD, HIGH);
                currentData.utilityFan = true;
                burst_start = millis();
                in_burst = true;
                utility_burst_count++;
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[UT Vent] Burst %d start: Hum=%.1f%%, Duration=%lu s", utility_burst_count, currentData.utilityHumidity, fan_duration/1000);
                logEvent(logMessage);
            }
        } else if (millis() - burst_start >= fan_duration) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            currentData.utilityFan = false;
            off_start = millis();
            in_burst = false;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Burst %d end: Hum=%.1f%%", utility_burst_count, currentData.utilityHumidity);
            logEvent(logMessage);
            // Off start: log duration
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Off start: Duration=%lu s", fan_off_duration/1000);
            logEvent(logMessage);
        }
    }

    // Handle semi-automatic (Luč OFF → zaženi ventilator)
    static bool lastLightStateUT = false;  // static! za edge detection
    bool currentLightStateUT = currentData.utilityLight;
    bool lightOffUT = lastLightStateUT && !currentLightStateUT;
    bool isDNDUT = isDNDTime();
    bool semiAutomaticTrigger = lightOffUT && (!isDNDUT || settings.dndAllowableSemiautomatic);

    if (semiAutomaticTrigger) {
        if (utility_drying_mode) {
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Semi-auto ignored: Drying mode active");
            logEvent(logMessage);
        } else if (!in_burst) {
            // Zaženi ventilator za fanDuration sekund
            digitalWrite(PIN_UTILITY_ODVOD, HIGH);
            currentData.utilityFan = true;
            burst_start = millis();
            in_burst = true;
            currentData.offTimes[1] = myTZ.now() + settings.fanDuration;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] ON: SemiAuto Trigg (Luc OFF), Trajanje: %u s", settings.fanDuration);
            logEvent(logMessage);
        }
    }
    // Log DND blocked semi-auto
    if (lightOffUT && isDNDUT && !settings.dndAllowableSemiautomatic) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[UT Vent] SemiAuto Trigg zavrnjen (DND)");
        logEvent(logMessage);
    }
    lastLightStateUT = currentLightStateUT;

    // Handle manual timeout - ko ni drying cikla, timeout za ročni vklop
    if (!utility_drying_mode && in_burst) {
        if (millis() - burst_start >= settings.fanDuration * 1000UL) {
            digitalWrite(PIN_UTILITY_ODVOD, LOW);
            currentData.utilityFan = false;
            in_burst = false;
            currentData.offTimes[1] = 0;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] OFF: Manual cikel konec (%u s)", settings.fanDuration);
            logEvent(logMessage);
        }
    }

    // Handle manual (dovoli, podaljša burst če active)
    bool manualTrigger = currentData.manualTriggerUtility && (!isDNDTime() || settings.dndAllowableManual);
    if (manualTrigger) {
        if (utility_drying_mode && in_burst) {
            burst_start -= fan_duration; // podaljšaj za + fan_duration
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Manual extend burst");
            logEvent(logMessage);
        } else if (!utility_drying_mode && !in_burst) {
            // Normal manual - zaženi ventilator za fanDuration sekund
            digitalWrite(PIN_UTILITY_ODVOD, HIGH);
            currentData.utilityFan = true;
            burst_start = millis();
            in_burst = true;
            currentData.offTimes[1] = myTZ.now() + settings.fanDuration;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] ON: Manual trigger via REW, Trajanje: %u s", settings.fanDuration);
            logEvent(logMessage);
        }
        currentData.manualTriggerUtility = false;
    }

    // Handle switch — "zadnji ima besedo": edge-detection za fizično stikalo in REW toggle
    static bool lastSwitchState = false;
    static bool switchInitialized = false;
    bool currentSwitchState = currentData.utilitySwitch;
    if (!switchInitialized) {
        lastSwitchState = !currentSwitchState;  // prisili edge detection pri prvem klicu
        switchInitialized = true;
    }
    if (currentSwitchState != lastSwitchState) {
        if (!currentSwitchState) {
            currentData.disableUtility = true;
            utility_drying_mode = false;
            utility_cycle_mode = 0;
            utility_burst_count = 0;
            if (in_burst) {
                digitalWrite(PIN_UTILITY_ODVOD, LOW);
                currentData.utilityFan = false;
                in_burst = false;
            }
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Disabled via switch");
            logEvent(logMessage);
        } else {
            currentData.disableUtility = false;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[UT Vent] Enabled via switch");
            logEvent(logMessage);
        }
    }
    lastSwitchState = currentSwitchState;

    // Update global status
    currentData.utilityDryingMode = utility_drying_mode;
    currentData.utilityCycleMode = utility_cycle_mode;
    currentData.utilityExpectedEndTime = expected_end_timeUT;
}

void controlBathroom() {
    static time_t expected_end_timeKOP = 0;
    static float hum_history[3] = {45.0, 45.0, 45.0};
    static bool drying_mode = false;
    static int cycle_mode = 0;
    static int burst_count = 0;
    static float baseline_hum = 45.0;

    static unsigned long fanStartTime = 0;
    static unsigned long lastOffTime = 0;
    static unsigned long buttonPressStart = 0;
    static bool fanActive = false;
    static bool lastButtonState = false;
    static bool lastLightState1 = false;
    static bool lastLightState2 = false;
    static bool isLongPress = false;
    static unsigned long lastSensorCheck = 0;
    static unsigned long fanEndTime = 0;   // absolute millis kdaj se fan ugasne
    static bool timeExtended = false;      // ali je bilo podaljšanje že dodeljeno
    static unsigned long burst_start = 0;  // premaknjena sem za dostop v disable bloku
    static bool in_burst = false;          // premaknjena sem za dostop v disable bloku

    static unsigned long off_start = 0;

    bool currentButtonState = currentData.bathroomButton;
    bool currentLightState1 = currentData.bathroomLight1;
    bool currentLightState2 = currentData.bathroomLight2;
    bool currentLightOn = currentLightState1 || currentLightState2;

    // Update history (shift array)
    for (int i = 0; i < 2; i++) {
        hum_history[i] = hum_history[i+1];
    }
    hum_history[2] = currentData.bathroomHumidity;

    // Če disableBathroom, prekini cikel
    if (currentData.disableBathroom) {
        if (digitalRead(PIN_KOPALNICA_ODVOD) == HIGH) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            currentData.bathroomFan = false;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Disable via switch/REW");
            logEvent(logMessage);
        }
        drying_mode = false;
        cycle_mode = 0;
        burst_count = 0;
        fanActive = false;              // reset fanActive pri disable
        in_burst = false;              // reset burst stanja pri disable
        currentData.offTimes[0] = 0;   // počisti off time
        lastButtonState = currentButtonState;
        lastLightState1 = currentLightState1;
        lastLightState2 = currentLightState2;
        return;
    }

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
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Pritisk ignoriran (debounce)");
                logEvent(logMessage);
            } else if (!fanActive) {
                unsigned long duration = isLongPress ? settings.fanDuration * 2 * 1000 : settings.fanDuration * 1000;
                fanEndTime = millis() + duration;
                timeExtended = false;
                digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
                fanStartTime = millis();
                fanActive = true;
                currentData.bathroomFan = true;
                const char* triggerType = isLongPress ? "Man Trigg (dolg)" : "Man Trigg (kratek)";
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: %s, Trajanje: %u s", triggerType, duration / 1000);
                logEvent(logMessage);
            } else if (!timeExtended) {
                fanEndTime += settings.fanDuration * 1000;
                timeExtended = true;
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Podaljsan: +%u s", settings.fanDuration);
                logEvent(logMessage);
            } else {
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Tipka zaklenjena do izklopa");
                logEvent(logMessage);
            }
        }

        // Handle timeout for active fans
        if (fanActive && millis() >= fanEndTime) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            fanActive = false;
            currentData.bathroomFan = false;
            currentData.offTimes[0] = 0;
            lastOffTime = millis();
            isLongPress = false;
            timeExtended = false;
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

    // Ignoriramo dupliciran drying trigger med aktivnim ciklom
    if (drying_mode && currentData.manualTriggerBathroomDrying) {
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Drying trigger ignored - ze v drying ciklu");
        logEvent(logMessage);
        currentData.manualTriggerBathroomDrying = false;
    }

    // Čakanje na trigger
    if (!drying_mode && cycle_mode == 0) {
        // Določi trigger: manual ima prednost pred auto
        bool trigger_fired = false;
        bool is_manual_drying = false;
        float auto_trend = 0.0f;

        if (currentData.manualTriggerBathroomDrying) {
            if (!isDNDTime() || settings.dndAllowableManual) {
                trigger_fired = true;
                is_manual_drying = true;
            } else {
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual drying trigger zavrnjen (DND)");
                logEvent(logMessage);
            }
            currentData.manualTriggerBathroomDrying = false;
        } else {
            // Automatic trigger check
            float trend = hum_history[2] - hum_history[0];
            if (trend > 10.0 && hum_history[2] > baseline_hum + 10.0) {
                trigger_fired = true;
                auto_trend = trend;
            }
        }

        if (trigger_fired) {
            int mode = determineCycleMode(currentData.bathroomTemp, currentData.bathroomHumidity, ERR_BME280);
            if (mode > 0) {
                drying_mode = true;
                cycle_mode = mode;
                burst_count = 0;
                off_start = millis() - (settings.fanOffDurationKop * 1000);
                time_t now = myTZ.now();
                unsigned long total_seconds = (3 * (360 * (mode == 1 ? 1.0 : mode == 2 ? 0.8 : 0.5) * 1000 / 1000)) + (2 * (settings.fanOffDurationKop * 1000 / 1000));
                time_t expected_end = now + total_seconds;
                expected_end_timeKOP = expected_end;
                char expectedTimeStr[20];
                strftime(expectedTimeStr, sizeof(expectedTimeStr), "%H:%M:%S", localtime(&expected_end));
                char logMessage[256];
                if (is_manual_drying) {
                    snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual drying trigger: Mode=%d End: %s", mode, expectedTimeStr);
                } else {
                    snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Auto trigger: Trend=%.1f%%, Mode=%d End: %s", auto_trend, mode, expectedTimeStr);
                }
                logEvent(logMessage);
            } else {
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Trigger blocked (%s): Mode=%d",
                    is_manual_drying ? "manual" : "auto", mode);
                logEvent(logMessage);
            }
            if (is_manual_drying) return;  // manual drying vrne, auto nadaljuje
        }
    }

    // Določitev duration in off
    float duration_factor = (cycle_mode == 1) ? 1.0 : (cycle_mode == 2) ? 0.8 : 0.5;
    unsigned long fan_duration = 360 * duration_factor * 1000; // ms  // specifično za KOP
    unsigned long fan_off_duration = settings.fanOffDurationKop * 1000; // iz settings, brez faktorja

    // Expected end time is calculated once at trigger, just copy to currentData

    if (drying_mode) {
        if (isDNDTime() && !settings.dndAllowableAutomatic) {
            // Preskoči auto burst v DND
            lastButtonState = currentButtonState;
            lastLightState1 = currentLightState1;
            lastLightState2 = currentLightState2;
            return;
        }

        if (!in_burst) {
            bool ready = (burst_count == 0) || (millis() - off_start >= fan_off_duration);
            if (ready) {
                // --- Preveri ponovitev / konec (za burst_count >= 1) ---
                if (burst_count >= 1) {
                    // Off end: log humidity and rate
                    float rate1 = (hum_history[2] - hum_history[1]) / 1.0;
                    float rate2 = (hum_history[1] - hum_history[0]) / 1.0;
                    float avg_rate = (rate1 + rate2) / 2.0;
                    char logMessage[256];
                    snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Off end: Hum=%.1f%%, Avg_rate=%.2f", currentData.bathroomHumidity, avg_rate);
                    logEvent(logMessage);
                    if (!(currentData.bathroomHumidity > 65.0 && burst_count < 3)) {
                        // Konec cikla
                        drying_mode = false; cycle_mode = 0;
                        baseline_hum = 45.0; hum_history[0]=45.0; hum_history[1]=45.0; hum_history[2]=45.0;
                        const char* reason = (currentData.bathroomHumidity <= 65.0) ? "Hum low" : "Bursts max";
                        char logMessage[256];
                        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Cycle end: Hum=%.1f%%, Bursts=%d, Baseline=%.1f%%, Reason=%s", currentData.bathroomHumidity, burst_count, baseline_hum, reason);
                        logEvent(logMessage);
                        burst_count = 0;
                        expected_end_timeKOP = 0;
                        currentData.bathroomExpectedEndTime = 0;
                        // Takoj posodobi globalní status - potrebno za change detection v STATUS_UPDATE
                        currentData.bathroomDryingMode = false;
                        currentData.bathroomCycleMode = 0;
                        return;  // izhod
                    } else {
                        // Prilagodi
                        if (avg_rate > 1.0) fan_off_duration *= 1.2;
                        else if (avg_rate < 0.2) fan_off_duration *= 0.8;
                        // Continue: log adjustment
                        char logMessage[256];
                        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Continue: Adjust off to %lu s (rate=%.2f)", fan_off_duration/1000, avg_rate);
                        logEvent(logMessage);
                    }
                }
                // --- Zaženi burst ---
                digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
                currentData.bathroomFan = true;
                burst_start = millis();
                in_burst = true;
                burst_count++;
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Burst %d start: Hum=%.1f%%, Duration=%lu s", burst_count, currentData.bathroomHumidity, fan_duration/1000);
                logEvent(logMessage);
            }
        } else if (millis() - burst_start >= fan_duration) {
            digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
            currentData.bathroomFan = false;
            off_start = millis();
            in_burst = false;
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Burst %d end: Hum=%.1f%%", burst_count, currentData.bathroomHumidity);
            logEvent(logMessage);
            // Off start: log duration
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Off start: Duration=%lu s", fan_off_duration/1000);
            logEvent(logMessage);
        }
    }

    // Handle button presses
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
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Pritisk ignoriran (debounce)");
            logEvent(logMessage);
        } else if (!isDNDTime() || settings.dndAllowableManual) {
            if (!fanActive) {
                unsigned long duration = isLongPress ? settings.fanDuration * 2 * 1000 : settings.fanDuration * 1000;
                fanEndTime = millis() + duration;
                timeExtended = false;
                digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
                fanStartTime = millis();
                fanActive = true;
                currentData.bathroomFan = true;
                currentData.offTimes[0] = myTZ.now() + duration / 1000;
                const char* triggerType = isLongPress ? "Man Trigg (dolg)" : "Man Trigg (kratek)";
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: %s, Trajanje: %u s", triggerType, duration / 1000);
                logEvent(logMessage);
            } else if (!timeExtended) {
                fanEndTime += settings.fanDuration * 1000;
                currentData.offTimes[0] += settings.fanDuration;
                timeExtended = true;
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Podaljsan: +%u s", settings.fanDuration);
                logEvent(logMessage);
            } else {
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Tipka zaklenjena do izklopa");
                logEvent(logMessage);
            }
        } else {
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Man Trigg zavrnjen (DND)");
            logEvent(logMessage);
        }
    }

    bool manualTriggerREW = currentData.manualTriggerBathroom && (!isDNDTime() || settings.dndAllowableManual);
    bool semiAutomaticTrigger = (lastLightState1 || lastLightState2) && !currentLightState1 && !currentLightState2 && (!isDNDTime() || settings.dndAllowableSemiautomatic);

    // Check for ignored manual triggers due to DND restrictions
    if (currentData.manualTriggerBathroom && !manualTriggerREW) {
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual trigger ignored - DND dovoli ročno upravljanje = Disabled");
        logEvent(logMessage);
        currentData.manualTriggerBathroom = false;
    }

    // Handle semi-automatic (Luč OFF → zaženi ventilator, razen v drying mode)
    bool lightOffKOP = (lastLightState1 || lastLightState2) && !currentLightState1 && !currentLightState2;
    if (semiAutomaticTrigger) {
        if (drying_mode) {
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Semi-auto ignored: Drying mode active");
            logEvent(logMessage);
        } else if (!fanActive) {
            unsigned long duration = settings.fanDuration * 1000;
            fanEndTime = millis() + duration;
            timeExtended = false;
            digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
            fanStartTime = millis();
            fanActive = true;
            currentData.bathroomFan = true;
            currentData.offTimes[0] = myTZ.now() + duration / 1000;
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: SemiAuto Trigg (Luc OFF), Trajanje: %u s", duration / 1000);
            logEvent(logMessage);
        }
    }
    // Log DND blocked semi-auto
    if (lightOffKOP && isDNDTime() && !settings.dndAllowableSemiautomatic) {
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] SemiAuto Trigg zavrnjen (DND)");
        logEvent(logMessage);
    }

    // Handle manual REW trigger (start/extend-1x/lock)
    if (manualTriggerREW) {
        if (drying_mode && in_burst) {
            burst_start -= fan_duration;  // podaljšaj burst
            char logMessage[256];
            snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Manual extend burst");
            logEvent(logMessage);
        } else if (!drying_mode) {
            if (!fanActive) {
                unsigned long duration = settings.fanDuration * 1000;
                fanEndTime = millis() + duration;
                timeExtended = false;
                digitalWrite(PIN_KOPALNICA_ODVOD, HIGH);
                fanStartTime = millis();
                fanActive = true;
                currentData.bathroomFan = true;
                currentData.offTimes[0] = myTZ.now() + duration / 1000;
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] ON: Manual trigger via REW, Trajanje: %u s", duration / 1000);
                logEvent(logMessage);
            } else if (!timeExtended) {
                fanEndTime += settings.fanDuration * 1000;
                currentData.offTimes[0] += settings.fanDuration;
                timeExtended = true;
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] Podaljsan via REW: +%u s", settings.fanDuration);
                logEvent(logMessage);
            } else {
                char logMessage[256];
                snprintf(logMessage, sizeof(logMessage), "[KOP Vent] REW trigger ignoriran (ze podaljsano)");
                logEvent(logMessage);
            }
        }
        // Če drying_mode && !in_burst: ne prekinjaj cikla
        currentData.manualTriggerBathroom = false;
    }

    // Fan timeout — uporablja fanEndTime (absolute millis)
    if (fanActive && millis() >= fanEndTime) {
        digitalWrite(PIN_KOPALNICA_ODVOD, LOW);
        fanActive = false;
        currentData.bathroomFan = false;
        currentData.offTimes[0] = 0;
        lastOffTime = millis();
        isLongPress = false;
        timeExtended = false;
        snprintf(logMessage, sizeof(logMessage), "[KOP Vent] OFF: Cikel konec");
        logEvent(logMessage);
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

    // Update global status
    currentData.bathroomDryingMode = drying_mode;
    currentData.bathroomCycleMode = cycle_mode;
    currentData.bathroomExpectedEndTime = expected_end_timeKOP;
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

    // Only use sensor data if valid (not NaN and within reasonable ranges)
    bool humidityValid = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= 0 && currentData.livingHumidity <= 100;
    bool co2Valid = !isnan(currentData.livingCO2) && currentData.livingCO2 >= 0 && currentData.livingCO2 <= 10000;
    bool tempValid = !isnan(currentData.livingTemp) && currentData.livingTemp >= -50 && currentData.livingTemp <= 100;
    bool externalTempValid = !isnan(currentData.externalTemp) && currentData.externalTemp >= -50 && currentData.externalTemp <= 100;

    // Humidity increments
    if (humidityValid) {
        if (currentData.livingHumidity >= settings.humThresholdDS) {
            cyclePercent += settings.incrementPercentLowDS;
        }
        if (currentData.livingHumidity >= settings.humThresholdHighDS) {
            cyclePercent += settings.incrementPercentHighDS;
        }
    }

    // CO2 increments
    if (co2Valid) {
        if (currentData.livingCO2 >= settings.co2ThresholdLowDS) {
            cyclePercent += settings.incrementPercentLowDS;
        }
        if (currentData.livingCO2 >= settings.co2ThresholdHighDS) {
            cyclePercent += settings.incrementPercentHighDS;
        }
    }

    // Temperature increment (isNND je že izključen v checkAutomaticPreconditions() preden se ta funkcija pokliče)
    if (tempValid && externalTempValid) {
        if ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
            (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp)) {
            cyclePercent += settings.incrementPercentTempDS;
        }
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

    // Log duty cycle calculation details only when changed
    static float previousCyclePercent = -1.0;
    if (abs(cyclePercent - previousCyclePercent) > 0.1) {  // Log if change > 0.1%
        char logDetails[256];
        snprintf(logDetails, sizeof(logDetails),
                 "Duty cycle calc: Base=%.1f%%, Hum: %.1f%% (%s), CO2: %.0f (%s), Temp: %.1f°C (%s), Adverse: %s, Final: %.1f%%",
                 settings.cycleActivePercentDS,
                 humidityValid ? currentData.livingHumidity : NAN,
                 (humidityValid && (currentData.livingHumidity >= settings.humThresholdDS || currentData.livingHumidity >= settings.humThresholdHighDS)) ? "active" : "inactive",
                 co2Valid ? currentData.livingCO2 : NAN,
                 (co2Valid && (currentData.livingCO2 >= settings.co2ThresholdLowDS || currentData.livingCO2 >= settings.co2ThresholdHighDS)) ? "active" : "inactive",
                 tempValid ? currentData.livingTemp : NAN,
                 (tempValid && externalTempValid && ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
                 (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp))) ? "active" : "inactive",
                 adverseConditions ? "YES (-50%)" : "NO",
                 cyclePercent);
        LOG_INFO("DS Vent", "%s", logDetails);
        previousCyclePercent = cyclePercent;
    }

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

    // Manual mode: ustavi pri disable ali odprtih oknih (konsistentno z brez-NTP potjo)
    if (manualMode && (currentData.windowSensor1 || currentData.windowSensor2 || currentData.disableLivingRoom)) {
        stopLivingRoomFan(fanActive, manualMode, currentLevel);
        lastOffTime = millis();
        const char* reason = currentData.disableLivingRoom ? "Disable" : "Okno odprto";
        LOG_INFO("DS Vent", "OFF: Manual ustavljen (%s)", reason);
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

    // Only use valid sensor data for level determination
    bool humidityValid = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= 0 && currentData.livingHumidity <= 100;
    bool co2Valid = !isnan(currentData.livingCO2) && currentData.livingCO2 >= 0 && currentData.livingCO2 <= 10000;

    bool highIncrement = (humidityValid && currentData.livingHumidity >= settings.humThresholdHighDS) ||
                         (co2Valid && currentData.livingCO2 >= settings.co2ThresholdHighDS);
    bool isDND = isDNDTime();
    // Level 1: normalni pogoji ali DND (tiho)
    // Level 2: neugodni pogoji — visok CO2 ali visoka vlaga (highIncrement)
    // Level 3: rezerviran za ročni trigger
    uint8_t newLevel = (isDND || !highIncrement) ? 1 : 2;

    if (newLevel == 1) digitalWrite(PIN_DNEVNI_ODVOD_1, HIGH);
    else if (newLevel == 2) digitalWrite(PIN_DNEVNI_ODVOD_2, HIGH);

    fanActive = true;
    currentLevel = newLevel;
    fanStartTime = millis();
    currentData.livingIntake = true;
    currentData.livingExhaustLevel = newLevel;

    unsigned long cycleDurationMs = settings.cycleDurationDS * 1000;
    unsigned long activeDurationMs = cycleDurationMs * (cyclePercent / 100.0);
    currentData.offTimes[4] = myTZ.now() + activeDurationMs / 1000;
    currentData.offTimes[5] = currentData.offTimes[4];

    // Determine level reason
    const char* levelReason = isDND ? "DND (L1)" : (highIncrement ? "High conditions (L2)" : "Normal (L1)");

    // Determine trigger reason
    char triggerReason[128];
    if (highIncrement) {
        if (humidityValid && currentData.livingHumidity >= settings.humThresholdHighDS) {
            snprintf(triggerReason, sizeof(triggerReason), "H_DS=%.1f%%>humThresholdHighDS=%.1f%%", currentData.livingHumidity, settings.humThresholdHighDS);
        } else if (co2Valid && currentData.livingCO2 >= settings.co2ThresholdHighDS) {
            snprintf(triggerReason, sizeof(triggerReason), "CO2=%.0f>co2ThresholdHighDS=%.0f", currentData.livingCO2, settings.co2ThresholdHighDS);
        } else {
            strcpy(triggerReason, "High increment (invalid data)");
        }
    } else {
        strcpy(triggerReason, "Auto cycle");
    }

    LOG_INFO("DS Vent", "ON: %s, Duty: %.1f%%, Level %d (%s), Duration: %u s", triggerReason, cyclePercent, newLevel, levelReason, activeDurationMs / 1000);
}

// Helper function: Start manual living room fan
void startManualLivingRoomFan(bool& fanActive, bool& manualMode, uint8_t& currentLevel, unsigned long& fanStartTime) {
    digitalWrite(PIN_DNEVNI_VPIH, HIGH);
    digitalWrite(PIN_DNEVNI_ODVOD_3, HIGH);
    fanActive = true;
    manualMode = true;
    currentLevel = 3;
    fanStartTime = millis();
    currentData.livingIntake = true;
    currentData.livingExhaustLevel = 3;

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

// Helper function: Get duty cycle breakdown for web display
String getDutyCycleBreakdown() {
    String breakdown = "";
    float basePercent = settings.cycleActivePercentDS;
    
    breakdown += "Trenutni duty cycle: " + String(currentData.livingRoomDutyCycle, 1) + "% (končni izračun)\n";
    breakdown += "Osnova (base): " + String(basePercent) + "% (iz cycleActivePercentDS, vedno aktivna)\n";

    // Preveri veljavnost senzorjev
    bool humidityValid = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= 0 && currentData.livingHumidity <= 100;
    bool co2Valid = !isnan(currentData.livingCO2) && currentData.livingCO2 >= 0 && currentData.livingCO2 <= 10000;
    bool tempValid = !isnan(currentData.livingTemp) && currentData.livingTemp >= -50 && currentData.livingTemp <= 100;
    bool externalTempValid = !isnan(currentData.externalTemp) && currentData.externalTemp >= -50 && currentData.externalTemp <= 100;

    // Humidity prirastek
    float humIncrement = 0.0;
    if (humidityValid) {
        if (currentData.livingHumidity >= settings.humThresholdHighDS) {
            humIncrement = settings.incrementPercentHighDS;
        } else if (currentData.livingHumidity >= settings.humThresholdDS) {
            humIncrement = settings.incrementPercentLowDS;
        }
    }
    String humStatus = humIncrement > 0 ? "active" : "inactive";
    breakdown += "Hum prirastek: " + String(humIncrement) + "% (" + humStatus + ") - ";
    if (humidityValid) {
        breakdown += "ker hum=" + String(currentData.livingHumidity, 1) + "% ";
        if (currentData.livingHumidity >= settings.humThresholdHighDS) {
            breakdown += "> humThresholdHighDS=" + String(settings.humThresholdHighDS) + "% (uporabi incrementPercentHighDS=" + String(settings.incrementPercentHighDS) + "%)\n";
        } else if (currentData.livingHumidity >= settings.humThresholdDS) {
            breakdown += "> humThresholdDS=" + String(settings.humThresholdDS) + "% (uporabi incrementPercentLowDS=" + String(settings.incrementPercentLowDS) + "%)\n";
        } else {
            breakdown += "< humThresholdDS=" + String(settings.humThresholdDS) + "% (ni prirastka)\n";
        }
    } else {
        breakdown += "neveljavni podatki\n";
    }

    // CO2 prirastek
    float co2Increment = 0.0;
    String co2Level = "none";
    if (co2Valid) {
        if (currentData.livingCO2 >= settings.co2ThresholdHighDS) {
            co2Increment = settings.incrementPercentHighDS;
            co2Level = "high";
        } else if (currentData.livingCO2 >= settings.co2ThresholdLowDS) {
            co2Increment = settings.incrementPercentLowDS;
            co2Level = "low";
        }
    }
    String co2Status = co2Increment > 0 ? "active" : "inactive";
    breakdown += "CO2 prirastek: " + String(co2Increment) + "% (" + co2Status + ", nivo: " + co2Level + ") - ";
    if (co2Valid) {
        breakdown += "ker CO2=" + String(currentData.livingCO2, 0) + " ";
        if (currentData.livingCO2 >= settings.co2ThresholdHighDS) {
            breakdown += "> co2ThresholdHighDS=" + String(settings.co2ThresholdHighDS) + "\n";
        } else if (currentData.livingCO2 >= settings.co2ThresholdLowDS) {
            breakdown += "> co2ThresholdLowDS=" + String(settings.co2ThresholdLowDS) + "\n";
        } else {
            breakdown += "< co2ThresholdLowDS=" + String(settings.co2ThresholdLowDS) + "\n";
        }
    } else {
        breakdown += "neveljavni podatki\n";
    }

    // Temperature prirastek
    float tempIncrement = 0.0;
    bool tempActive = false;
    bool isNND = isNNDTime();
    if (!isNND && tempValid && externalTempValid) {
        if ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
            (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp)) {
            tempIncrement = settings.incrementPercentTempDS;
            tempActive = true;
        }
    }
    String tempStatus = tempActive ? "active" : "inactive";
    breakdown += "Temp prirastek: " + String(tempIncrement) + "% (" + tempStatus + ") - ";
    if (tempValid && externalTempValid) {
        breakdown += "ker internalTemp=" + String(currentData.livingTemp, 1) + "C, externalTemp=" + String(currentData.externalTemp, 1) + "C, tempIdealDS=" + String(settings.tempIdealDS) + "C";
        if (isNND) {
            breakdown += ", NND=True (temperatura ne uposteva)\n";
        } else {
            breakdown += "\n";
        }
    } else {
        breakdown += "neveljavni podatki ali NND\n";
    }

    // Adverse pogoji (zmanjšanje za 50%)
    bool adverseConditions = currentData.externalHumidity > settings.humExtremeHighDS ||
                            currentData.externalTemp > settings.tempExtremeHighDS ||
                            currentData.externalTemp < settings.tempExtremeLowDS;
    String adverseStatus = adverseConditions ? "YES (active)" : "NO (inactive)";
    breakdown += "Adverse pogoji: " + adverseStatus + " - ";
    breakdown += "externalTemp=" + String(currentData.externalTemp, 1) + "C, externalHum=" + String(currentData.externalHumidity, 1) + "%";
    if (adverseConditions) {
        breakdown += " -> zmanjšanje za 50%";
    }
    breakdown += "\n";

    // Skupni izračun
    breakdown += "Skupni izračun: base(" + String(basePercent) + "%) + hum(" + String(humIncrement) + "%) + CO2(" + String(co2Increment) + "%) + temp(" + String(tempIncrement) + "%)";
    if (adverseConditions) {
        breakdown += " -> *0.5 (adverse) = " + String(currentData.livingRoomDutyCycle, 1) + "%";
    }
    breakdown += "\n";

    // Opombe o napakah
    if (currentData.errorFlags & ERR_DEW) {
        breakdown += "Opomba: ERR_DEW aktivna - blokira nekatere prirastke\n";
    }
    if (!externalDataValid) {
        breakdown += "Opomba: externalDataValid=False - ni zunanjih podatkov\n";
    }

    return breakdown;
}

// vent.cpp
