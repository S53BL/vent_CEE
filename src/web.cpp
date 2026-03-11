#include "web.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "globals.h"
#include "logging.h"
#include "system.h"
#include "message_fields.h"
#include "help_html.h"
#include "html.h"
#include "vent.h"

// Helper functions for root page
String formatUptime(unsigned long seconds) {
    unsigned long days = seconds / 86400;
    unsigned long hours = (seconds % 86400) / 3600;
    unsigned long minutes = (seconds % 3600) / 60;
    unsigned long secs = seconds % 60;
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02lu %02lu:%02lu:%02lu", days, hours, minutes, secs);
    return String(buffer);
}

int getRemainingTime(int index) {
    if (currentData.offTimes[index] > 0) {
        time_t now = myTZ.now();
        if (currentData.offTimes[index] > now) {
            return currentData.offTimes[index] - now;
        }
    }
    return 0;
}

String getFanStatus(bool fanActive, bool disabled) {
    if (disabled) return "DISABLED";
    return fanActive ? "ON" : "OFF";
}

String getLightStatus(bool light) {
    return light ? "ON" : "OFF";
}

String getWindowStatus() {
    return (currentData.windowSensor1 || currentData.windowSensor2) ? "Odprta" : "Zaprta";
}

String getSeasonName() {
    switch (currentSeasonCode) {
        case 0: return "Pomlad";
        case 1: return "Poletje";
        case 2: return "Jesen";
        case 3: return "Zima";
        default: return "Neznano";
    }
}

String getErrorDescription(uint8_t errorFlags) {
    String errors = "";
    if (errorFlags & ERR_BME280) errors += "Senzor BME280 nedelujoč<br>";
    if (errorFlags & ERR_SHT41) errors += "Senzor SHT41 nedelujoč<br>";
    if (errorFlags & ERR_POWER) errors += "Napajalna napaka<br>";
    if (errors == "") errors = "Brez napak";
    return errors;
}

String getDutyCycleParamStatus() {
    String status = "";

    bool co2LowActive = !isnan(currentData.livingCO2) && currentData.livingCO2 >= settings.co2ThresholdLowDS;
    status += "CO2 Low: " + String(co2LowActive ? "✓" : "✗") + "<br>";

    bool co2HighActive = !isnan(currentData.livingCO2) && currentData.livingCO2 >= settings.co2ThresholdHighDS;
    status += "CO2 High: " + String(co2HighActive ? "✓" : "✗") + "<br>";

    bool humLowActive = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= settings.humThresholdDS;
    status += "Hum Low: " + String(humLowActive ? "✓" : "✗") + "<br>";

    bool humHighActive = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= settings.humThresholdHighDS;
    status += "Hum High: " + String(humHighActive ? "✓" : "✗") + "<br>";

    bool tempActive = !isNNDTime() && !isnan(currentData.livingTemp) && !isnan(currentData.externalTemp) &&
                     ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
                      (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp));
    status += "Temp: " + String(tempActive ? "✓" : "✗") + "<br>";

    bool adverseActive = currentData.externalHumidity > settings.humExtremeHighDS ||
                        currentData.externalTemp > settings.tempExtremeHighDS ||
                        currentData.externalTemp < settings.tempExtremeLowDS;
    status += "Adverse: " + String(adverseActive ? "✓" : "✗") + "<br>";

    return status;
}

String getFanLevelReason() {
    if (isDNDTime()) return "DND";

    bool highIncrement = (!isnan(currentData.livingHumidity) && currentData.livingHumidity >= settings.humThresholdHighDS) ||
                         (!isnan(currentData.livingCO2) && currentData.livingCO2 >= settings.co2ThresholdHighDS);
    if (highIncrement) return "High increment";

    return "Normal";
}

String getSlovenianDateTime() {
    if (!timeSynced) return "Čas ni sinhroniziran";

    time_t now = myTZ.now();
    struct tm *timeinfo = localtime(&now);

    const char* days[] = {"nedelja", "ponedeljek", "torek", "sreda", "četrtek", "petek", "sobota"};

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%s %02d-%s-%04d %02d:%02d:%02d",
             days[timeinfo->tm_wday],
             timeinfo->tm_mday,
             timeinfo->tm_mon + 1 < 10 ? String("0") + String(timeinfo->tm_mon + 1) : String(timeinfo->tm_mon + 1),
             timeinfo->tm_year + 1900,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);

    return String(buffer);
}

AsyncWebServer server(80);

// HTTP endpoint handlers
void handleManualControl(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    if (index + len == total) {
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            LOG_ERROR("HTTP", "JSON parse error: %s", error.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Invalid JSON\"}");
            return;
        }

        String room = doc[FIELD_ROOM] | "";
        String action = doc[FIELD_ACTION] | "";

        if (room == "" || action == "") {
            LOG_ERROR("HTTP", "Missing room or action in MANUAL_CONTROL");
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Missing room or action\"}");
            return;
        }

        if (action == "manual") {
            if (room == "wc") {
                currentData.manualTriggerWC = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - WC activated");
            } else if (room == "ut") {
                currentData.manualTriggerUtility = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Utility activated");
            } else if (room == "kop") {
                currentData.manualTriggerBathroom = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Bathroom activated");
            } else if (room == "ds") {
                currentData.manualTriggerLivingRoom = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Living Room activated");
            } else {
                LOG_ERROR("HTTP", "Unknown room: %s", room.c_str());
                request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room\"}");
                return;
            }
        } else if (action == "toggle") {
            if (room == "wc") {
                currentData.disableWc = !currentData.disableWc;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - WC %s", currentData.disableWc ? "disabled" : "enabled");
            } else if (room == "ut") {
                if (!currentData.utilitySwitch) {
                    // Stikalo je OFF — REW toggle ignoriran (hardware lock)
                    LOG_INFO("HTTP", "MANUAL_CONTROL: UT toggle ignoriran - switch OFF (hardware lock)");
                } else {
                    currentData.disableUtility = !currentData.disableUtility;
                    LOG_INFO("HTTP", "MANUAL_CONTROL: Utility %s via REW", currentData.disableUtility ? "disabled" : "enabled");
                }
            } else if (room == "kop") {
                currentData.disableBathroom = !currentData.disableBathroom;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Bathroom %s", currentData.disableBathroom ? "disabled" : "enabled");
            } else if (room == "ds") {
                currentData.disableLivingRoom = !currentData.disableLivingRoom;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Living Room %s", currentData.disableLivingRoom ? "disabled" : "enabled");
            } else {
                LOG_ERROR("HTTP", "Unknown room: %s", room.c_str());
                request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room\"}");
                return;
            }
        } else if (action == "drying") {
            if (room == "ut") {
                currentData.manualTriggerUtilityDrying = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Utility drying mode triggered");
            } else if (room == "kop") {
                currentData.manualTriggerBathroomDrying = true;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Bathroom drying mode triggered");
            } else {
                LOG_ERROR("HTTP", "Unknown room for drying action: %s", room.c_str());
                request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room for drying action\"}");
                return;
            }
        } else {
            LOG_ERROR("HTTP", "Unknown action: %s", action.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown action\"}");
            return;
        }

        // Reactivate REW if it was offline
        String clientIP = request->client()->remoteIP().toString();
        if (clientIP == IP_REW && !rewStatus.isOnline) {
            rewStatus.isOnline = true;
            LOG_INFO("HTTP", "REW reaktiviran ob prejemu MANUAL_CONTROL od %s", clientIP.c_str());
        }

        request->send(200, "application/json", "{\"status\":\"OK\"}");
    }
}

void handleSensorData(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    if (index + len == total) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            LOG_ERROR("HTTP", "JSON parse error: %s", error.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Invalid JSON\"}");
            return;
        }

        externalData.externalTemperature = doc[FIELD_EXT_TEMP] | 0.0f;
        externalData.externalHumidity = doc[FIELD_EXT_HUM] | 0.0f;
        externalData.externalPressure = doc[FIELD_EXT_PRESS] | 0.0f;
        externalData.livingTempDS = doc[FIELD_DS_TEMP] | 0.0f;
        externalData.livingHumidityDS = doc[FIELD_DS_HUM] | 0.0f;
        JsonVariant co2Variant = doc[FIELD_DS_CO2];
        if (co2Variant.is<uint16_t>()) {
            externalData.livingCO2 = co2Variant.as<uint16_t>();
        } else {
            externalData.livingCO2 = 0;
        }
        externalData.weatherIcon = doc[FIELD_WEATHER_ICON] | "";
        externalData.seasonCode = doc[FIELD_SEASON_CODE] | 0;
        externalData.timestamp = doc[FIELD_TIMESTAMP] | 0;

        currentData.externalTemp = externalData.externalTemperature;
        currentData.externalHumidity = externalData.externalHumidity;
        currentData.externalPressure = externalData.externalPressure;
        currentData.livingTemp = externalData.livingTempDS;
        currentData.livingHumidity = externalData.livingHumidityDS;
        currentData.livingCO2 = externalData.livingCO2;

        currentWeatherIcon = externalData.weatherIcon;
        currentSeasonCode = externalData.seasonCode;

        if (timeSynced) {
            lastSensorDataTime = myTZ.now();
            externalDataValid = true;
        }

        LOG_INFO("HTTP", "SENSOR_DATA: Received - Ext: %.1f°C/%.1f%%/%.1fhPa, DS: %.1f°C/%.1f%%/%dppm, Icon: %s, Season: %d, TS: %u",
                 externalData.externalTemperature, externalData.externalHumidity, externalData.externalPressure,
                 externalData.livingTempDS, externalData.livingHumidityDS, externalData.livingCO2,
                 externalData.weatherIcon.c_str(), externalData.seasonCode, externalData.timestamp);

        // Reactivate REW if it was offline
        String clientIP = request->client()->remoteIP().toString();
        if (clientIP == IP_REW && !rewStatus.isOnline) {
            rewStatus.isOnline = true;
            LOG_INFO("HTTP", "REW reaktiviran ob prejemu SENSOR_DATA od %s", clientIP.c_str());
        }

        request->send(200, "application/json", "{\"status\":\"OK\"}");
    }
}

// ============================================================
// Settings page HTML
// ============================================================
const char HTML_SETTINGS[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>CEE - Nastavitve</title>
    <style>)rawliteral"
    // CSS iz html.h - skupni nav bar + lokalni styles za settings
    "\n" /* HTML_NAV_CSS se vključi v handleSettings() */ R"rawliteral(
        .wrap{max-width:860px;margin:0 auto;padding:16px;}
        h1{font-size:22px;color:#e0e0e0;margin:16px 0;}
        .form-container{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,.3);}
        .form-group{margin-bottom:18px;}
        .form-group label{display:block;margin-bottom:5px;font-weight:bold;color:#e0e0e0;}
        .form-group .description{font-size:13px;color:#aaa;margin-top:4px;}
        input[type=number],select{padding:7px 10px;font-size:14px;background:#1a1a1a;color:#e0e0e0;border:1px solid #555;border-radius:4px;}
        input[type=number]{width:90px;}
        input[type=number]:focus,select:focus{outline:none;border-color:#4da6ff;box-shadow:0 0 0 2px rgba(77,166,255,.2);}
        select{width:130px;cursor:pointer;}
        h2.section{color:#4da6ff;margin:30px 0 16px 0;font-size:18px;border-bottom:2px solid #4da6ff;padding-bottom:5px;}
        .action-bar{display:flex;gap:10px;margin:20px 0;}
        .btn{padding:9px 20px;border:none;border-radius:5px;font-size:14px;font-weight:bold;cursor:pointer;}
        .btn-save{background:#4da6ff;color:#101010;}
        .btn-save:hover{background:#6bb3ff;}
        .btn-reset{background:#ffaa00;color:#101010;}
        .btn-reset:hover{background:#e69500;}
        .btn-factory{background:#ff4444;color:#fff;}
        .btn-factory:hover{background:#cc3333;}
    </style>
</head>
<body>)rawliteral";

// Opomba: HTML_SETTINGS je razdeljen - nadaljevanje se gradi dinamično v handleSettings()
// ker moramo vključiti PROGMEM stringe HTML_NAV_CSS in HTML_NAV_BAR

bool settingsUpdatePending = false;
unsigned long settingsUpdateStartTime = 0;
bool settingsUpdateSuccess = false;
String settingsUpdateMessage = "";

// Handle root page
void handleRoot(AsyncWebServerRequest *request) {
    if (!request->hasHeader("X-Requested-With") ||
        request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
        LOG_DEBUG("Web", "Zahtevek: GET /");
    }

    float ramPercent = (ESP.getHeapSize() - ESP.getFreeHeap()) * 100.0 / ESP.getHeapSize();
    String uptimeStr = formatUptime(millis() / 1000);

    String html = F("<!DOCTYPE HTML><html><head>"
        "<meta charset='UTF-8'>"
        "<title>CEE - Ventilacijski sistem</title>"
        "<style>");
    html += FPSTR(HTML_NAV_CSS);
    html += F("</style></head><body>");
    html += FPSTR(HTML_NAV_BAR);
    html += F("<div class='wrap'>"
        "<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:16px;'>"
            "<h1 style='margin:0;'>Ventilacijski sistem</h1>"
            "<div id='dateDisplay' style='color:#4da6ff;font-size:15px;font-weight:bold;'>");
    html += getSlovenianDateTime();
    html += F("</div></div><div class='grid'>");

    // WC Card
    html += F("<div class='card'><h2>WC</h2>");
    html += "<div class='status-item'><span class='status-label'>Tlak:</span><span class='status-value' id='wc-pressure'>" + String((int)currentData.bathroomPressure) + " hPa</span></div>";
    html += "<div class='status-item'><span class='status-label'>Luč:</span><span class='status-value' id='wc-light'>" + getLightStatus(currentData.wcLight) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Ventilator:</span><span class='status-value' id='wc-fan'>" + getFanStatus(currentData.wcFan, currentData.disableWc) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Preostali čas:</span><span class='status-value' id='wc-remaining'>" + String(getRemainingTime(2)) + " s</span></div>";
    html += F("</div>");

    // KOP Card
    html += F("<div class='card'><h2>Kopalnica</h2>");
    html += "<div class='status-item'><span class='status-label'>Temperatura:</span><span class='status-value' id='kop-temp'>" + String(currentData.bathroomTemp, 1) + " °C</span></div>";
    html += "<div class='status-item'><span class='status-label'>Vlaga:</span><span class='status-value' id='kop-humidity'>" + String(currentData.bathroomHumidity, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>Tipka:</span><span class='status-value' id='kop-button'>" + String(currentData.bathroomButton ? "Pritisnjena" : "Nepritisnjena") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Luč 1:</span><span class='status-value' id='kop-light1'>" + getLightStatus(currentData.bathroomLight1) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Luč 2:</span><span class='status-value' id='kop-light2'>" + getLightStatus(currentData.bathroomLight2) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Ventilator:</span><span class='status-value' id='kop-fan'>" + getFanStatus(currentData.bathroomFan, currentData.disableBathroom) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Preostali čas:</span><span class='status-value' id='kop-remaining'>" + String(getRemainingTime(0)) + " s</span></div>";
    html += "<div class='status-item'><span class='status-label'>Drying mode:</span><span class='status-value' id='kop-drying-mode'>" + String(currentData.bathroomDryingMode ? "DA" : "NE") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Cycle mode:</span><span class='status-value' id='kop-cycle-mode'>" + String(currentData.bathroomCycleMode) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Expected end:</span><span class='status-value' id='kop-expected-end'>" + (currentData.bathroomDryingMode ? String(currentData.bathroomExpectedEndTime) : "N/A") + "</span></div>";
    html += F("<div class='status-item'><button onclick=\"triggerDrying('kop')\" style='padding:5px 10px;background:#4da6ff;color:#101010;border:none;border-radius:4px;cursor:pointer;'>Start Drying Cycle</button></div>");
    html += F("</div>");

    // UT Card
    html += F("<div class='card'><h2>Utility</h2>");
    html += "<div class='status-item'><span class='status-label'>Temperatura:</span><span class='status-value' id='ut-temp'>" + String(currentData.utilityTemp, 1) + " °C</span></div>";
    html += "<div class='status-item'><span class='status-label'>Vlaga:</span><span class='status-value' id='ut-humidity'>" + String(currentData.utilityHumidity, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>Luč:</span><span class='status-value' id='ut-light'>" + getLightStatus(currentData.utilityLight) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Stikalo:</span><span class='status-value' id='ut-switch'>" + String(currentData.utilitySwitch ? "ON" : "OFF") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Ventilator:</span><span class='status-value' id='ut-fan'>" + getFanStatus(currentData.utilityFan, currentData.disableUtility) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Preostali čas:</span><span class='status-value' id='ut-remaining'>" + String(getRemainingTime(1)) + " s</span></div>";
    html += "<div class='status-item'><span class='status-label'>Drying mode:</span><span class='status-value' id='ut-drying-mode'>" + String(currentData.utilityDryingMode ? "DA" : "NE") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Cycle mode:</span><span class='status-value' id='ut-cycle-mode'>" + String(currentData.utilityCycleMode) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Expected end:</span><span class='status-value' id='ut-expected-end'>" + (currentData.utilityDryingMode ? String(currentData.utilityExpectedEndTime) : "N/A") + "</span></div>";
    html += F("<div class='status-item'><button onclick=\"triggerDrying('ut')\" style='padding:5px 10px;background:#4da6ff;color:#101010;border:none;border-radius:4px;cursor:pointer;'>Start Drying Cycle</button></div>");
    html += F("</div>");

    // DS Card
    html += F("<div class='card'><h2>Dnevni prostor</h2>");
    html += "<div class='status-item'><span class='status-label'>Temperatura:</span><span class='status-value' id='ds-temp'>" + String(currentData.livingTemp, 1) + " °C</span></div>";
    html += "<div class='status-item'><span class='status-label'>Vlaga:</span><span class='status-value' id='ds-humidity'>" + String(currentData.livingHumidity, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>CO2:</span><span class='status-value' id='ds-co2'>" + String((int)currentData.livingCO2) + " ppm</span></div>";
    html += "<div class='status-item'><span class='status-label'>Strešno okno:</span><span class='status-value' id='ds-window1'>" + String(currentData.windowSensor2 ? "Odprto" : "Zaprto") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Balkonska vrata:</span><span class='status-value' id='ds-window2'>" + String(currentData.windowSensor1 ? "Odprto" : "Zaprto") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Stopnja ventilatorja:</span><span class='status-value' id='ds-fan-level'>" + String(currentData.livingExhaustLevel) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Razmerje:</span><span class='status-value' id='ds-duty-cycle'>" + String(currentData.livingRoomDutyCycle, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>Preostali čas:</span><span class='status-value' id='ds-remaining'>" + String(getRemainingTime(4)) + " s</span></div>";
    html += F("</div>");

    // External Data Card
    html += F("<div class='card'><h2>Zunanji podatki</h2>");
    html += "<div class='status-item'><span class='status-label'>Temperatura:</span><span class='status-value' id='ext-temp'>" + String(currentData.externalTemp, 1) + " °C</span></div>";
    html += "<div class='status-item'><span class='status-label'>Vlaga:</span><span class='status-value' id='ext-humidity'>" + String(currentData.externalHumidity, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>Tlak:</span><span class='status-value' id='ext-pressure'>" + String(currentData.externalPressure, 1) + " hPa</span></div>";
    html += "<div class='status-item'><span class='status-label'>Svetloba:</span><span class='status-value' id='ext-light'>" + String(currentData.externalLight, 1) + " lux</span></div>";
    html += "<div class='status-item'><span class='status-label'>Ikona:</span><span class='status-value' id='ext-weather-icon'>" + String(currentWeatherIcon) + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Sezona:</span><span class='status-value' id='ext-season'>" + getSeasonName() + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>DND:</span><span class='status-value' id='ext-dnd'>" + String(isDNDTime() ? "DA" : "NE") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>NND:</span><span class='status-value' id='ext-nnd'>" + String(isNNDTime() ? "DA" : "NE") + "</span></div>";
    html += F("</div>");

    // Power Card
    html += F("<div class='card'><h2>Napajanje</h2>");
    html += "<div class='status-item'><span class='status-label'>3.3V:</span><span class='status-value' id='power-3v3'>" + String(currentData.supply3V3, 3) + " V</span></div>";
    html += "<div class='status-item'><span class='status-label'>5V:</span><span class='status-value' id='power-5v'>" + String(currentData.supply5V, 3) + " V</span></div>";
    html += "<div class='status-item'><span class='status-label'>Poraba:</span><span class='status-value' id='power-current'>" + String(currentData.currentPower, 1) + " W</span></div>";
    html += "<div class='status-item'><span class='status-label'>Energija:</span><span class='status-value' id='power-energy'>" + String(currentData.energyConsumption, 1) + " Wh</span></div>";
    html += F("</div>");

    // System Card
    html += F("<div class='card'><h2>Sistem</h2>");
    html += "<div class='status-item'><span class='status-label'>Prosti RAM:</span><span class='status-value' id='sys-ram'>" + String(ramPercent, 1) + " %</span></div>";
    html += "<div class='status-item'><span class='status-label'>Uptime:</span><span class='status-value' id='sys-uptime'>" + uptimeStr + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Log buffer:</span><span class='status-value' id='sys-log'>" + String(logBuffer.length()) + " B</span></div>";
    html += F("</div>");

    // Status Card
    html += F("<div class='card'><h2>Status</h2>");
    html += "<div class='status-item'><span class='status-label'>Zunanji podatki:</span><span class='status-value' id='status-external'>" + String(externalDataValid ? "VELJAVNI" : "NEVELJAVNI") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Senzor BME280:</span><span class='status-value' id='status-bme280'>" + String((currentData.errorFlags & ERR_BME280) ? "NAPAKA" : "OK") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Senzor SHT41:</span><span class='status-value' id='status-sht41'>" + String((currentData.errorFlags & ERR_SHT41) ? "NAPAKA" : "OK") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Napajanje:</span><span class='status-value' id='status-power'>" + String((currentData.errorFlags & ERR_POWER) ? "NAPAKA" : "OK") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>NTP sinhronizacija:</span><span class='status-value' id='status-ntp'>" + String(timeSynced ? "OK" : "NESINHRONIZIRAN") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>Čas z REW:</span><span class='status-value' id='status-time-rew'>" + String(externalDataValid && timeSynced && abs((int32_t)(myTZ.now() - externalData.timestamp)) <= 300 ? "OK" : "RAZHAJANJE >5min") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>REW:</span><span class='status-value' id='status-rew'>" + String(rewStatus.isOnline ? "ONLINE" : "OFFLINE") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>UT_DEW:</span><span class='status-value' id='status-ut-dew'>" + String(utDewStatus.isOnline ? "ONLINE" : "OFFLINE") + "</span></div>";
    html += "<div class='status-item'><span class='status-label'>KOP_DEW:</span><span class='status-value' id='status-kop-dew'>" + String(kopDewStatus.isOnline ? "ONLINE" : "OFFLINE") + "</span></div>";
    html += F("</div>");

    // DS Duty Cycle Card - full width
    html += F("<div class='card' style='grid-column:1/-1;'><h2>DS Duty Cycle Parametri</h2>");
    html += "<div class='status-item'><span class='status-label'>Razlog za level:</span><span class='status-value'>" + getFanLevelReason() + "</span></div>";
    html += "<pre style='font-size:13px;white-space:pre-wrap;word-wrap:break-word;'>" + getDutyCycleBreakdown() + "</pre>";
    html += F("</div>");

    html += F("</div>"); // end .grid

    // JS
    html += F("<script>"
        "function updatePage(isAjax=false){"
            "const opts=isAjax?{headers:{'X-Requested-With':'XMLHttpRequest'}}:{};"
            "fetch('/current-data',opts).then(r=>r.json()).then(d=>{"
                "document.getElementById('dateDisplay').textContent=d.current_time;"
                "document.getElementById('wc-pressure').textContent=d.bathroom_pressure+' hPa';"
                "document.getElementById('wc-light').textContent=d.wc_light?'ON':'OFF';"
                "document.getElementById('wc-fan').textContent=d.wc_disabled?'DISABLED':(d.wc_fan?'ON':'OFF');"
                "document.getElementById('wc-remaining').textContent=d.wc_remaining+' s';"
                "document.getElementById('kop-temp').textContent=d.bathroom_temp+' °C';"
                "document.getElementById('kop-humidity').textContent=d.bathroom_humidity+' %';"
                "document.getElementById('kop-button').textContent=d.bathroom_button?'Pritisnjena':'Nepritisnjena';"
                "document.getElementById('kop-light1').textContent=d.bathroom_light1?'ON':'OFF';"
                "document.getElementById('kop-light2').textContent=d.bathroom_light2?'ON':'OFF';"
                "document.getElementById('kop-fan').textContent=d.bathroom_disabled?'DISABLED':(d.bathroom_fan?'ON':'OFF');"
                "document.getElementById('kop-remaining').textContent=d.bathroom_remaining+' s';"
                "document.getElementById('kop-drying-mode').textContent=d.bathroom_drying_mode?'DA':'NE';"
                "document.getElementById('kop-cycle-mode').textContent=d.bathroom_cycle_mode;"
                "if(d.bathroom_expected_end_time&&d.bathroom_expected_end_time>0){"
                    "const dt=new Date(d.bathroom_expected_end_time*1000);"
                    "document.getElementById('kop-expected-end').textContent=dt.getHours().toString().padStart(2,'0')+':'+dt.getMinutes().toString().padStart(2,'0')+':'+dt.getSeconds().toString().padStart(2,'0');"
                "}else{document.getElementById('kop-expected-end').textContent='N/A';}"
                "document.getElementById('ut-temp').textContent=d.utility_temp+' °C';"
                "document.getElementById('ut-humidity').textContent=d.utility_humidity+' %';"
                "document.getElementById('ut-light').textContent=d.utility_light?'ON':'OFF';"
                "document.getElementById('ut-switch').textContent=d.utility_switch?'ON':'OFF';"
                "document.getElementById('ut-fan').textContent=d.utility_disabled?'DISABLED':(d.utility_fan?'ON':'OFF');"
                "document.getElementById('ut-remaining').textContent=d.utility_remaining+' s';"
                "document.getElementById('ut-drying-mode').textContent=d.utility_drying_mode?'DA':'NE';"
                "document.getElementById('ut-cycle-mode').textContent=d.utility_cycle_mode;"
                "if(d.utility_expected_end_time&&d.utility_expected_end_time>0){"
                    "const dt=new Date(d.utility_expected_end_time*1000);"
                    "document.getElementById('ut-expected-end').textContent=dt.getHours().toString().padStart(2,'0')+':'+dt.getMinutes().toString().padStart(2,'0')+':'+dt.getSeconds().toString().padStart(2,'0');"
                "}else{document.getElementById('ut-expected-end').textContent='N/A';}"
                "document.getElementById('ds-temp').textContent=d.living_temp+' °C';"
                "document.getElementById('ds-humidity').textContent=d.living_humidity+' %';"
                "document.getElementById('ds-co2').textContent=d.living_co2+' ppm';"
                "document.getElementById('ds-window1').textContent=d.living_window2?'Odprto':'Zaprto';"
                "document.getElementById('ds-window2').textContent=d.living_window1?'Odprto':'Zaprto';"
                "document.getElementById('ds-fan-level').textContent=d.living_fan_level;"
                "document.getElementById('ds-duty-cycle').textContent=d.living_duty_cycle+' %';"
                "document.getElementById('ds-remaining').textContent=d.living_remaining+' s';"
                "document.getElementById('ext-temp').textContent=d.external_temp+' °C';"
                "document.getElementById('ext-humidity').textContent=d.external_humidity+' %';"
                "document.getElementById('ext-pressure').textContent=d.external_pressure+' hPa';"
                "document.getElementById('ext-light').textContent=d.external_light+' lux';"
                "document.getElementById('power-3v3').textContent=d.supply_3v3+' V';"
                "document.getElementById('power-5v').textContent=d.supply_5v+' V';"
                "document.getElementById('power-current').textContent=d.current_power+' W';"
                "document.getElementById('power-energy').textContent=d.energy_consumption+' Wh';"
                "document.getElementById('sys-ram').textContent=d.ram_percent+' %';"
                "document.getElementById('sys-uptime').textContent=d.uptime;"
                "document.getElementById('sys-log').textContent=d.log_buffer_size+' B';"
                "document.getElementById('status-external').textContent=d.external_data_valid?'VELJAVNI':'NEVELJAVNI';"
                "document.getElementById('status-bme280').textContent=(d.error_flags&1)?'NAPAKA':'OK';"
                "document.getElementById('status-sht41').textContent=(d.error_flags&2)?'NAPAKA':'OK';"
                "document.getElementById('status-power').textContent=(d.error_flags&64)?'NAPAKA':'OK';"
                "document.getElementById('status-ntp').textContent=d.time_synced?'OK':'NESINHRONIZIRAN';"
                "document.getElementById('status-time-rew').textContent=(d.external_data_valid&&d.time_synced&&Math.abs(Date.now()/1000-d.external_timestamp)<=300)?'OK':'RAZHAJANJE >5min';"
                "document.getElementById('status-rew').textContent=d.rew_online?'ONLINE':'OFFLINE';"
                "document.getElementById('status-ut-dew').textContent=d.ut_dew_online?'ONLINE':'OFFLINE';"
                "document.getElementById('status-kop-dew').textContent=d.kop_dew_online?'ONLINE':'OFFLINE';"
            "}).catch(e=>console.error('Napaka pri osveževanju:',e));"
        "}"
        "function triggerDrying(room){"
            "fetch('/api/manual-control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({room:room,action:'drying'})})"
            ".then(r=>r.json()).then(d=>{if(d.status!=='OK')console.error('Error:',d.message);})"
            ".catch(e=>console.error('Error:',e));"
        "}"
        "setInterval(()=>updatePage(true),10000);"
        "updatePage();"
    "</script></div></body></html>");

    request->send(200, "text/html", html);
}

// Handle help page
void handleHelp(AsyncWebServerRequest *request) {
    if (!request->hasHeader("X-Requested-With") ||
        request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
        LOG_DEBUG("Web", "Zahtevek: GET /help");
    }
    request->send(200, "text/html", HTML_HELP);
}

// Handle settings page - gradi HTML dinamično z nav barom iz html.h
void handleSettings(AsyncWebServerRequest *request) {
    if (!request->hasHeader("X-Requested-With") ||
        request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
        LOG_DEBUG("Web", "Zahtevek: GET /settings");
    }

    String html = F("<!DOCTYPE HTML><html><head>"
        "<meta charset='UTF-8'>"
        "<title>CEE - Nastavitve</title>"
        "<style>");
    html += FPSTR(HTML_NAV_CSS);
    html += F(".wrap{max-width:860px;margin:0 auto;padding:16px;}"
        "h1{font-size:22px;color:#e0e0e0;margin:16px 0;}"
        ".form-container{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,.3);}"
        ".form-group{margin-bottom:18px;}"
        ".form-group label{display:block;margin-bottom:5px;font-weight:bold;color:#e0e0e0;}"
        ".form-group .description{font-size:13px;color:#aaa;margin-top:4px;}"
        "input[type=number],select{padding:7px 10px;font-size:14px;background:#1a1a1a;color:#e0e0e0;border:1px solid #555;border-radius:4px;}"
        "input[type=number]{width:90px;}"
        "input[type=number]:focus,select:focus{outline:none;border-color:#4da6ff;box-shadow:0 0 0 2px rgba(77,166,255,.2);}"
        "select{width:130px;cursor:pointer;}"
        "h2.section{color:#4da6ff;margin:30px 0 16px 0;font-size:18px;border-bottom:2px solid #4da6ff;padding-bottom:5px;}"
        ".action-bar{display:flex;gap:10px;margin:20px 0;}"
        ".btn{padding:9px 20px;border:none;border-radius:5px;font-size:14px;font-weight:bold;cursor:pointer;}"
        ".btn-save{background:#4da6ff;color:#101010;}.btn-save:hover{background:#6bb3ff;}"
        ".btn-reset{background:#ffaa00;color:#101010;}.btn-reset:hover{background:#e69500;}"
        ".btn-factory{background:#ff4444;color:#fff;}.btn-factory:hover{background:#cc3333;}"
    "</style></head><body>");
    html += FPSTR(HTML_NAV_BAR);
    html += F("<div class='wrap'>"
        "<h1>Ventilacijski sistem - Nastavitve</h1>"
        "<div id='message' class='message' style='display:none;'></div>"
        "<div class='action-bar'>"
            "<button class='btn btn-save' onclick='saveSettings()'>Shrani</button>"
            "<button class='btn btn-reset' onclick='resetToDefaults()'>Ponastavi</button>"
            "<button class='btn btn-factory' onclick='resetToFactoryDefaults()'>Tovarniški reset</button>"
        "</div>"
        "<div class='form-container'>"

        "<div class='form-group'>"
            "<label for='humThreshold'>Mejna vrednost vlage za kopalnico in utility</label>"
            "<input type='number' name='humThreshold' id='humThreshold' step='1' min='0' max='100'>"
            "<div class='description'>Relativna vlaga, ki sproži samodejni vklop ventilatorja (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='fanDuration'>Čas delovanja ventilatorjev po sprožilcu</label>"
            "<input type='number' name='fanDuration' id='fanDuration' step='1' min='60' max='6000'>"
            "<div class='description'>Čas delovanja po vsakem sprožilcu (60–6000 s).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='fanOffDuration'>Minimalni premor med vklopi v utilityju in WC-ju</label>"
            "<input type='number' name='fanOffDuration' id='fanOffDuration' step='1' min='60' max='6000'>"
            "<div class='description'>Minimalni premor med zaporednimi vklopi v utilityju ali WC-ju (60–6000 s).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='fanOffDurationKop'>Minimalni premor med vklopi v kopalnici</label>"
            "<input type='number' name='fanOffDurationKop' id='fanOffDurationKop' step='1' min='60' max='6000'>"
            "<div class='description'>Minimalni premor med zaporednimi vklopi v kopalnici (60–6000 s).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='tempLowThreshold'>Prag za zmanjšanje delovanja pri nizki zunanji temperaturi</label>"
            "<input type='number' name='tempLowThreshold' id='tempLowThreshold' step='1' min='-20' max='40'>"
            "<div class='description'>Zunanja T, pod katero se trajanje skrajša na polovico (-20–40 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='tempMinThreshold'>Prag za ustavitev delovanja pri minimalni zunanji temperaturi</label>"
            "<input type='number' name='tempMinThreshold' id='tempMinThreshold' step='1' min='-20' max='40'>"
            "<div class='description'>Zunanja T, pod katero se ventilatorji popolnoma ustavijo (-20–40 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='dndAllowAutomatic'>Dovoljenje za samodejno prezračevanje ponoči</label>"
            "<select name='dndAllowAutomatic' id='dndAllowAutomatic'>"
                "<option value='0'>0 (Izključeno)</option>"
                "<option value='1'>1 (Vključeno)</option>"
            "</select>"
            "<div class='description'>Dovoli samodejne vklope ventilatorjev med nočnim časom (22:00–06:00).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='dndAllowSemiautomatic'>Dovoljenje za polsamodejno prezračevanje ponoči</label>"
            "<select name='dndAllowSemiautomatic' id='dndAllowSemiautomatic'>"
                "<option value='0'>0 (Izključeno)</option>"
                "<option value='1'>1 (Vključeno)</option>"
            "</select>"
            "<div class='description'>Dovoli vklope ob ugasnjeni luči med nočnim časom.</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='dndAllowManual'>Dovoljenje za ročno prezračevanje ponoči</label>"
            "<select name='dndAllowManual' id='dndAllowManual'>"
                "<option value='0'>0 (Izključeno)</option>"
                "<option value='1'>1 (Vključeno)</option>"
            "</select>"
            "<div class='description'>Dovoli ročne vklope med nočnim časom.</div>"
        "</div>"

        "<h2 class='section'>Dnevni prostor</h2>"

        "<div class='form-group'>"
            "<label for='cycleDurationDS'>Dolžina prezračevalnega cikla</label>"
            "<input type='number' name='cycleDurationDS' id='cycleDurationDS' step='1' min='60' max='6000'>"
            "<div class='description'>Skupni čas enega prezračevalnega cikla (60–6000 s).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='cycleActivePercentDS'>Osnovni delež aktivnega delovanja cikla</label>"
            "<input type='number' name='cycleActivePercentDS' id='cycleActivePercentDS' step='1' min='0' max='100'>"
            "<div class='description'>Osnovni odstotek časa, ko ventilator deluje v ciklu (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='humThresholdDS'>Mejna vrednost vlage</label>"
            "<input type='number' name='humThresholdDS' id='humThresholdDS' step='1' min='0' max='100'>"
            "<div class='description'>Vlaga, ki sproži povečanje cikla (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='humThresholdHighDS'>Zgornja mejna vrednost vlage</label>"
            "<input type='number' name='humThresholdHighDS' id='humThresholdHighDS' step='1' min='0' max='100'>"
            "<div class='description'>Višja vlaga za večje povečanje cikla (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='humExtremeHighDS'>Ekstremno visoka zunanja vlaga</label>"
            "<input type='number' name='humExtremeHighDS' id='humExtremeHighDS' step='1' min='0' max='100'>"
            "<div class='description'>Ekstremno visoka zunanja vlaga za zmanjšanje cikla (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='co2ThresholdLowDS'>Spodnja mejna vrednost CO2</label>"
            "<input type='number' name='co2ThresholdLowDS' id='co2ThresholdLowDS' step='1' min='400' max='2000'>"
            "<div class='description'>CO2 za začetno povečanje cikla (400–2000 ppm).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='co2ThresholdHighDS'>Zgornja mejna vrednost CO2</label>"
            "<input type='number' name='co2ThresholdHighDS' id='co2ThresholdHighDS' step='1' min='400' max='2000'>"
            "<div class='description'>Višji CO2 za maksimalno povečanje cikla (400–2000 ppm).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='incrementPercentLowDS'>Dodatno povečanje cikla pri spodnjih mejah</label>"
            "<input type='number' name='incrementPercentLowDS' id='incrementPercentLowDS' step='1' min='0' max='100'>"
            "<div class='description'>Dodatni % delovanja ob dosegu nizke meje (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='incrementPercentHighDS'>Dodatno povečanje cikla pri zgornjih mejah</label>"
            "<input type='number' name='incrementPercentHighDS' id='incrementPercentHighDS' step='1' min='0' max='100'>"
            "<div class='description'>Dodatni % delovanja ob dosegu visoke meje (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='incrementPercentTempDS'>Dodatno povečanje cikla za uravnavanje temperature</label>"
            "<input type='number' name='incrementPercentTempDS' id='incrementPercentTempDS' step='1' min='0' max='100'>"
            "<div class='description'>Dodatni % cikla za hlajenje/ogrevanje (0–100 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='tempIdealDS'>Želena temperatura</label>"
            "<input type='number' name='tempIdealDS' id='tempIdealDS' step='1' min='-20' max='40'>"
            "<div class='description'>Ciljna notranja temperatura (-20–40 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='tempExtremeHighDS'>Prag za zmanjšanje cikla pri visoki zunanji temperaturi</label>"
            "<input type='number' name='tempExtremeHighDS' id='tempExtremeHighDS' step='1' min='-20' max='40'>"
            "<div class='description'>Zunanja T za zmanjšanje cikla (preprečitev vnosa vročega zraka) (-20–40 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='tempExtremeLowDS'>Prag za zmanjšanje cikla pri nizki zunanji temperaturi</label>"
            "<input type='number' name='tempExtremeLowDS' id='tempExtremeLowDS' step='1' min='-20' max='40'>"
            "<div class='description'>Zunanja T za zmanjšanje cikla (preprečitev vnosa mrzlega zraka) (-20–40 °C).</div>"
        "</div>"

        "<h2 class='section'>Nastavitve senzorjev</h2>"

        "<div class='form-group'>"
            "<label for='bmeTempOffset'>Offset temperature BME280 (°C)</label>"
            "<input type='number' id='bmeTempOffset' name='bmeTempOffset' step='0.1' min='-10.0' max='10.0'>"
            "<div class='description'>Prilagoditev temperature BME280 (-10.0 do +10.0 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='bmeHumidityOffset'>Offset vlažnosti BME280 (%)</label>"
            "<input type='number' id='bmeHumidityOffset' name='bmeHumidityOffset' step='0.1' min='-20.0' max='20.0'>"
            "<div class='description'>Prilagoditev vlažnosti BME280 (-20.0 do +20.0 %).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='bmePressureOffset'>Offset tlaka BME280 (hPa)</label>"
            "<input type='number' id='bmePressureOffset' name='bmePressureOffset' step='0.1' min='-50.0' max='50.0'>"
            "<div class='description'>Prilagoditev tlaka BME280 (-50.0 do +50.0 hPa).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='shtTempOffset'>Offset temperature SHT41 (°C)</label>"
            "<input type='number' id='shtTempOffset' name='shtTempOffset' step='0.1' min='-10.0' max='10.0'>"
            "<div class='description'>Prilagoditev temperature SHT41 (-10.0 do +10.0 °C).</div>"
        "</div>"
        "<div class='form-group'>"
            "<label for='shtHumidityOffset'>Offset vlažnosti SHT41 (%)</label>"
            "<input type='number' id='shtHumidityOffset' name='shtHumidityOffset' step='0.1' min='-20.0' max='20.0'>"
            "<div class='description'>Prilagoditev vlažnosti SHT41 (-20.0 do +20.0 %).</div>"
        "</div>"

        "</div></div>"); // end .form-container, .wrap

    html += F("<script>"
        "function loadCurrentSettings(){"
            "fetch('/data').then(r=>r.json()).then(d=>{"
                "document.getElementById('humThreshold').value=d.HUM_THRESHOLD;"
                "document.getElementById('fanDuration').value=d.FAN_DURATION;"
                "document.getElementById('fanOffDuration').value=d.FAN_OFF_DURATION;"
                "document.getElementById('fanOffDurationKop').value=d.FAN_OFF_DURATION_KOP;"
                "document.getElementById('tempLowThreshold').value=d.TEMP_LOW_THRESHOLD;"
                "document.getElementById('tempMinThreshold').value=d.TEMP_MIN_THRESHOLD;"
                "document.getElementById('dndAllowAutomatic').value=d.DND_ALLOW_AUTOMATIC?'1':'0';"
                "document.getElementById('dndAllowSemiautomatic').value=d.DND_ALLOW_SEMIAUTOMATIC?'1':'0';"
                "document.getElementById('dndAllowManual').value=d.DND_ALLOW_MANUAL?'1':'0';"
                "document.getElementById('cycleDurationDS').value=d.CYCLE_DURATION_DS;"
                "document.getElementById('cycleActivePercentDS').value=d.CYCLE_ACTIVE_PERCENT_DS;"
                "document.getElementById('humThresholdDS').value=d.HUM_THRESHOLD_DS;"
                "document.getElementById('humThresholdHighDS').value=d.HUM_THRESHOLD_HIGH_DS;"
                "document.getElementById('humExtremeHighDS').value=d.HUM_EXTREME_HIGH_DS;"
                "document.getElementById('co2ThresholdLowDS').value=d.CO2_THRESHOLD_LOW_DS;"
                "document.getElementById('co2ThresholdHighDS').value=d.CO2_THRESHOLD_HIGH_DS;"
                "document.getElementById('incrementPercentLowDS').value=d.INCREMENT_PERCENT_LOW_DS;"
                "document.getElementById('incrementPercentHighDS').value=d.INCREMENT_PERCENT_HIGH_DS;"
                "document.getElementById('incrementPercentTempDS').value=d.INCREMENT_PERCENT_TEMP_DS;"
                "document.getElementById('tempIdealDS').value=d.TEMP_IDEAL_DS;"
                "document.getElementById('tempExtremeHighDS').value=d.TEMP_EXTREME_HIGH_DS;"
                "document.getElementById('tempExtremeLowDS').value=d.TEMP_EXTREME_LOW_DS;"
                "document.getElementById('bmeTempOffset').value=d.BME_TEMP_OFFSET;"
                "document.getElementById('bmeHumidityOffset').value=d.BME_HUMIDITY_OFFSET;"
                "document.getElementById('bmePressureOffset').value=d.BME_PRESSURE_OFFSET;"
                "document.getElementById('shtTempOffset').value=d.SHT_TEMP_OFFSET;"
                "document.getElementById('shtHumidityOffset').value=d.SHT_HUMIDITY_OFFSET;"
            "}).catch(e=>console.error('Napaka:',e));"
        "}"
        "function showMessage(text,type){"
            "const m=document.getElementById('message');"
            "m.textContent=text;m.className='message '+type;m.style.display='block';"
            "setTimeout(()=>m.style.display='none',5000);"
        "}"
        "function saveSettings(){"
            "const ids=['humThreshold','fanDuration','fanOffDuration','fanOffDurationKop','tempLowThreshold','tempMinThreshold',"
                "'dndAllowAutomatic','dndAllowSemiautomatic','dndAllowManual','cycleDurationDS','cycleActivePercentDS',"
                "'humThresholdDS','humThresholdHighDS','humExtremeHighDS','co2ThresholdLowDS','co2ThresholdHighDS',"
                "'incrementPercentLowDS','incrementPercentHighDS','incrementPercentTempDS','tempIdealDS',"
                "'tempExtremeHighDS','tempExtremeLowDS','bmeTempOffset','bmeHumidityOffset','bmePressureOffset',"
                "'shtTempOffset','shtHumidityOffset'];"
            "const params=new URLSearchParams();"
            "ids.forEach(id=>params.append(id,document.getElementById(id).value));"
            "fetch('/settings/update',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params})"
            ".then(r=>r.text()).then(t=>{"
                "showMessage(t,t==='Nastavitve shranjene!'?'success':'error');"
                "if(t==='Nastavitve shranjene!')loadCurrentSettings();"
            "}).catch(e=>showMessage('Napaka: '+e,'error'));"
        "}"
        "function resetToDefaults(){"
            "fetch('/settings/reset',{method:'POST'}).then(r=>r.json())"
            ".then(d=>{showMessage(d.success?d.message:(d.error||'Napaka'),d.success?'success':'error');if(d.success)loadCurrentSettings();})"
            ".catch(e=>showMessage('Napaka: '+e,'error'));"
        "}"
        "function resetToFactoryDefaults(){"
            "if(confirm('Ali ste prepričani?')&&confirm('Te operacije ni mogoče preklicati.')){"
                "fetch('/settings/factory-reset',{method:'POST'}).then(r=>r.json())"
                ".then(d=>{showMessage(d.success?d.message:(d.error||'Napaka'),d.success?'success':'error');if(d.success)loadCurrentSettings();})"
                ".catch(e=>showMessage('Napaka: '+e,'error'));"
            "}"
        "}"
        "loadCurrentSettings();"
    "</script></body></html>");

    request->send(200, "text/html", html);
}

void handleDataRequest(AsyncWebServerRequest *request) {
    LOG_DEBUG("Web", "Zahtevek: GET /data");

    Settings tempSettings;
    Preferences prefs;
    prefs.begin("settings", true);

    uint8_t marker = prefs.getUChar("marker", 0x00);
    bool dataValid = (marker == SETTINGS_MARKER);

    if (dataValid) {
        size_t bytesRead = prefs.getBytes("settings", (uint8_t*)&tempSettings, sizeof(Settings));
        uint16_t stored_crc = prefs.getUShort("settings_crc", 0);

        if (bytesRead == sizeof(Settings)) {
            uint16_t calculated_crc = calculateCRC((const uint8_t*)&tempSettings, sizeof(Settings));
            if (calculated_crc == stored_crc) {
                LOG_INFO("Web", "naloženo (CRC %04X)", calculated_crc);
            } else {
                LOG_WARN("Web", "CRC neustreza za /data (izračunan: 0x%04X, shranjen: 0x%04X) - uporabljene privzete vrednosti", calculated_crc, stored_crc);
                dataValid = false;
            }
        } else {
            LOG_WARN("Web", "Neveljavna velikost podatkov za /data (%d != %d)", bytesRead, sizeof(Settings));
            dataValid = false;
        }
    } else {
        LOG_DEBUG("Web", "Marker neveljaven za /data - uporabljene privzete vrednosti");
    }

    prefs.end();

    if (!dataValid) {
        initDefaults();
        tempSettings = settings;
    }

    String json = "{" +
                  String("\"CURRENT_TIME\":\"") + String(myTZ.dateTime().c_str()) + "\"," +
                  String("\"IS_DND\":") + String(isDNDTime() ? "1" : "0") + "," +
                  String("\"IS_NND\":") + String(isNNDTime() ? "1" : "0") + "," +
                  String("\"HUM_THRESHOLD\":\"") + String((int)tempSettings.humThreshold) + "\"," +
                  String("\"FAN_DURATION\":\"") + String(tempSettings.fanDuration) + "\"," +
                  String("\"FAN_OFF_DURATION\":\"") + String(tempSettings.fanOffDuration) + "\"," +
                  String("\"FAN_OFF_DURATION_KOP\":\"") + String(tempSettings.fanOffDurationKop) + "\"," +
                  String("\"TEMP_LOW_THRESHOLD\":\"") + String((int)tempSettings.tempLowThreshold) + "\"," +
                  String("\"TEMP_MIN_THRESHOLD\":\"") + String((int)tempSettings.tempMinThreshold) + "\"," +
                  String("\"DND_ALLOW_AUTOMATIC\":") + String(tempSettings.dndAllowableAutomatic ? "1" : "0") + "," +
                  String("\"DND_ALLOW_SEMIAUTOMATIC\":") + String(tempSettings.dndAllowableSemiautomatic ? "1" : "0") + "," +
                  String("\"DND_ALLOW_MANUAL\":") + String(tempSettings.dndAllowableManual ? "1" : "0") + "," +
                  String("\"CYCLE_DURATION_DS\":\"") + String(tempSettings.cycleDurationDS) + "\"," +
                  String("\"CYCLE_ACTIVE_PERCENT_DS\":\"") + String((int)tempSettings.cycleActivePercentDS) + "\"," +
                  String("\"HUM_THRESHOLD_DS\":\"") + String((int)tempSettings.humThresholdDS) + "\"," +
                  String("\"HUM_THRESHOLD_HIGH_DS\":\"") + String((int)tempSettings.humThresholdHighDS) + "\"," +
                  String("\"HUM_EXTREME_HIGH_DS\":\"") + String((int)tempSettings.humExtremeHighDS) + "\"," +
                  String("\"CO2_THRESHOLD_LOW_DS\":\"") + String(tempSettings.co2ThresholdLowDS) + "\"," +
                  String("\"CO2_THRESHOLD_HIGH_DS\":\"") + String(tempSettings.co2ThresholdHighDS) + "\"," +
                  String("\"INCREMENT_PERCENT_LOW_DS\":\"") + String((int)tempSettings.incrementPercentLowDS) + "\"," +
                  String("\"INCREMENT_PERCENT_HIGH_DS\":\"") + String((int)tempSettings.incrementPercentHighDS) + "\"," +
                  String("\"INCREMENT_PERCENT_TEMP_DS\":\"") + String((int)tempSettings.incrementPercentTempDS) + "\"," +
                  String("\"TEMP_IDEAL_DS\":\"") + String((int)tempSettings.tempIdealDS) + "\"," +
                  String("\"TEMP_EXTREME_HIGH_DS\":\"") + String((int)tempSettings.tempExtremeHighDS) + "\"," +
                  String("\"TEMP_EXTREME_LOW_DS\":\"") + String((int)tempSettings.tempExtremeLowDS) + "\"," +
                  String("\"BME_TEMP_OFFSET\":\"") + String(tempSettings.bmeTempOffset, 2) + "\"," +
                  String("\"BME_HUMIDITY_OFFSET\":\"") + String(tempSettings.bmeHumidityOffset, 2) + "\"," +
                  String("\"BME_PRESSURE_OFFSET\":\"") + String(tempSettings.bmePressureOffset, 2) + "\"," +
                  String("\"SHT_TEMP_OFFSET\":\"") + String(tempSettings.shtTempOffset, 2) + "\"," +
                  String("\"SHT_HUMIDITY_OFFSET\":\"") + String(tempSettings.shtHumidityOffset, 2) + "\"}";

    request->send(200, "application/json", json);
}

void handleCurrentDataRequest(AsyncWebServerRequest *request) {
    if (!request->hasHeader("X-Requested-With") ||
        request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
        LOG_DEBUG("Web", "Zahtevek: GET /current-data");
    }

    float ramPercent = (ESP.getHeapSize() - ESP.getFreeHeap()) * 100.0 / ESP.getHeapSize();
    String uptimeStr = formatUptime(millis() / 1000);

    String json = "{" +
                  String("\"current_time\":\"") + String(myTZ.dateTime().c_str()) + "\"," +
                  String("\"is_dnd\":") + String(isDNDTime() ? "true" : "false") + "," +
                  String("\"is_nnd\":") + String(isNNDTime() ? "true" : "false") + "," +
                  String("\"bathroom_temp\":") + String(currentData.bathroomTemp, 1) + "," +
                  String("\"bathroom_humidity\":") + String(currentData.bathroomHumidity, 1) + "," +
                  String("\"bathroom_button\":") + String(currentData.bathroomButton ? "true" : "false") + "," +
                  String("\"bathroom_pressure\":") + String((int)currentData.bathroomPressure) + "," +
                  String("\"bathroom_light1\":") + String(currentData.bathroomLight1 ? "true" : "false") + "," +
                  String("\"bathroom_light2\":") + String(currentData.bathroomLight2 ? "true" : "false") + "," +
                  String("\"bathroom_fan\":") + String(currentData.bathroomFan ? "true" : "false") + "," +
                  String("\"bathroom_disabled\":") + String(currentData.disableBathroom ? "true" : "false") + "," +
                  String("\"bathroom_remaining\":") + String(getRemainingTime(0)) + "," +
                  String("\"bathroom_drying_mode\":") + String(currentData.bathroomDryingMode ? "true" : "false") + "," +
                  String("\"bathroom_cycle_mode\":") + String(currentData.bathroomCycleMode) + "," +
                  String("\"bathroom_expected_end_time\":") + String(currentData.bathroomExpectedEndTime) + "," +
                  String("\"utility_temp\":") + String(currentData.utilityTemp, 1) + "," +
                  String("\"utility_humidity\":") + String(currentData.utilityHumidity, 1) + "," +
                  String("\"utility_light\":") + String(currentData.utilityLight ? "true" : "false") + "," +
                  String("\"utility_switch\":") + String(currentData.utilitySwitch ? "true" : "false") + "," +
                  String("\"utility_fan\":") + String(currentData.utilityFan ? "true" : "false") + "," +
                  String("\"utility_disabled\":") + String(currentData.disableUtility ? "true" : "false") + "," +
                  String("\"utility_remaining\":") + String(getRemainingTime(1)) + "," +
                  String("\"utility_drying_mode\":") + String(currentData.utilityDryingMode ? "true" : "false") + "," +
                  String("\"utility_cycle_mode\":") + String(currentData.utilityCycleMode) + "," +
                  String("\"utility_expected_end_time\":") + String(currentData.utilityExpectedEndTime) + "," +
                  String("\"wc_light\":") + String(currentData.wcLight ? "true" : "false") + "," +
                  String("\"wc_fan\":") + String(currentData.wcFan ? "true" : "false") + "," +
                  String("\"wc_disabled\":") + String(currentData.disableWc ? "true" : "false") + "," +
                  String("\"wc_remaining\":") + String(getRemainingTime(2)) + "," +
                  String("\"living_temp\":") + String(currentData.livingTemp, 1) + "," +
                  String("\"living_humidity\":") + String(currentData.livingHumidity, 1) + "," +
                  String("\"living_co2\":") + String((int)currentData.livingCO2) + "," +
                  String("\"living_window1\":") + String(currentData.windowSensor1 ? "true" : "false") + "," +
                  String("\"living_window2\":") + String(currentData.windowSensor2 ? "true" : "false") + "," +
                  String("\"living_fan_level\":") + String(currentData.livingExhaustLevel) + "," +
                  String("\"living_duty_cycle\":") + String(currentData.livingRoomDutyCycle, 1) + "," +
                  String("\"living_disabled\":") + String(currentData.disableLivingRoom ? "true" : "false") + "," +
                  String("\"living_remaining\":") + String(getRemainingTime(4)) + "," +
                  String("\"external_temp\":") + String(currentData.externalTemp, 1) + "," +
                  String("\"external_humidity\":") + String(currentData.externalHumidity, 1) + "," +
                  String("\"external_pressure\":") + String(currentData.externalPressure, 1) + "," +
                  String("\"external_light\":") + String(currentData.externalLight, 1) + "," +
                  String("\"supply_3v3\":") + String(currentData.supply3V3, 3) + "," +
                  String("\"supply_5v\":") + String(currentData.supply5V, 3) + "," +
                  String("\"current_power\":") + String(currentData.currentPower, 1) + "," +
                  String("\"energy_consumption\":") + String(currentData.energyConsumption, 1) + "," +
                  String("\"time_synced\":") + String(timeSynced ? "true" : "false") + "," +
                  String("\"external_data_valid\":") + String(externalDataValid ? "true" : "false") + "," +
                  String("\"ram_percent\":") + String(ramPercent, 1) + "," +
                  String("\"uptime\":\"") + uptimeStr + "\"," +
                  String("\"log_buffer_size\":") + String(logBuffer.length()) + "," +
                  String("\"rew_online\":") + String(rewStatus.isOnline ? "true" : "false") + "," +
                  String("\"ut_dew_online\":") + String(utDewStatus.isOnline ? "true" : "false") + "," +
                  String("\"kop_dew_online\":") + String(kopDewStatus.isOnline ? "true" : "false") + "," +
                  String("\"external_timestamp\":") + String(externalData.timestamp) + "," +
                  String("\"error_flags\":") + String((int)currentData.errorFlags) +
                  "}";

    request->send(200, "application/json", json);
}

void handlePostSettings(AsyncWebServerRequest *request) {
    LOG_DEBUG("Web", "Zahtevek: POST /settings/update");
    String errorMessage = "";

    if (!request->hasParam("humThreshold", true) || !request->hasParam("fanDuration", true) ||
        !request->hasParam("fanOffDuration", true) || !request->hasParam("fanOffDurationKop", true) ||
        !request->hasParam("tempLowThreshold", true) || !request->hasParam("tempMinThreshold", true) ||
        !request->hasParam("cycleDurationDS", true) ||
        !request->hasParam("cycleActivePercentDS", true) || !request->hasParam("humThresholdDS", true) ||
        !request->hasParam("humThresholdHighDS", true) || !request->hasParam("humExtremeHighDS", true) ||
        !request->hasParam("co2ThresholdLowDS", true) || !request->hasParam("co2ThresholdHighDS", true) ||
        !request->hasParam("incrementPercentLowDS", true) || !request->hasParam("incrementPercentHighDS", true) ||
        !request->hasParam("incrementPercentTempDS", true) || !request->hasParam("tempIdealDS", true) ||
        !request->hasParam("tempExtremeHighDS", true) || !request->hasParam("tempExtremeLowDS", true) ||
        !request->hasParam("bmeTempOffset", true) || !request->hasParam("bmeHumidityOffset", true) ||
        !request->hasParam("bmePressureOffset", true) || !request->hasParam("shtTempOffset", true) ||
        !request->hasParam("shtHumidityOffset", true)) {
        errorMessage = "Manjkajoči parametri";
        LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
        request->send(400, "text/plain", errorMessage);
        return;
    }

    Settings newSettings;
    newSettings.humThreshold = request->getParam("humThreshold", true)->value().toFloat();
    newSettings.fanDuration = request->getParam("fanDuration", true)->value().toInt();
    newSettings.fanOffDuration = request->getParam("fanOffDuration", true)->value().toInt();
    newSettings.fanOffDurationKop = request->getParam("fanOffDurationKop", true)->value().toInt();
    newSettings.tempLowThreshold = request->getParam("tempLowThreshold", true)->value().toFloat();
    newSettings.tempMinThreshold = request->getParam("tempMinThreshold", true)->value().toFloat();
    newSettings.dndAllowableAutomatic = request->getParam("dndAllowAutomatic", true)->value() == "1";
    newSettings.dndAllowableSemiautomatic = request->getParam("dndAllowSemiautomatic", true)->value() == "1";
    newSettings.dndAllowableManual = request->getParam("dndAllowManual", true)->value() == "1";
    newSettings.cycleDurationDS = request->getParam("cycleDurationDS", true)->value().toInt();
    newSettings.cycleActivePercentDS = request->getParam("cycleActivePercentDS", true)->value().toFloat();
    newSettings.humThresholdDS = request->getParam("humThresholdDS", true)->value().toFloat();
    newSettings.humThresholdHighDS = request->getParam("humThresholdHighDS", true)->value().toFloat();
    newSettings.humExtremeHighDS = request->getParam("humExtremeHighDS", true)->value().toFloat();
    newSettings.co2ThresholdLowDS = request->getParam("co2ThresholdLowDS", true)->value().toInt();
    newSettings.co2ThresholdHighDS = request->getParam("co2ThresholdHighDS", true)->value().toInt();

    if (newSettings.co2ThresholdHighDS < 400) {
        newSettings.co2ThresholdHighDS = 1200;
        LOG_WARN("Web", "co2ThresholdHighDS < 400, nastavljeno na 1200");
    }
    if (newSettings.co2ThresholdLowDS < 400) {
        newSettings.co2ThresholdLowDS = 400;
        LOG_WARN("Web", "co2ThresholdLowDS < 400, nastavljeno na 400");
    }

    newSettings.incrementPercentLowDS = request->getParam("incrementPercentLowDS", true)->value().toFloat();
    newSettings.incrementPercentHighDS = request->getParam("incrementPercentHighDS", true)->value().toFloat();
    newSettings.incrementPercentTempDS = request->getParam("incrementPercentTempDS", true)->value().toFloat();
    newSettings.tempIdealDS = request->getParam("tempIdealDS", true)->value().toFloat();
    newSettings.tempExtremeHighDS = request->getParam("tempExtremeHighDS", true)->value().toFloat();
    newSettings.tempExtremeLowDS = request->getParam("tempExtremeLowDS", true)->value().toFloat();
    newSettings.bmeTempOffset = request->getParam("bmeTempOffset", true)->value().toFloat();
    newSettings.bmeHumidityOffset = request->getParam("bmeHumidityOffset", true)->value().toFloat();
    newSettings.bmePressureOffset = request->getParam("bmePressureOffset", true)->value().toFloat();
    newSettings.shtTempOffset = request->getParam("shtTempOffset", true)->value().toFloat();
    newSettings.shtHumidityOffset = request->getParam("shtHumidityOffset", true)->value().toFloat();

    // Validacija relacij
    if (newSettings.humThreshold >= newSettings.humExtremeHighDS) {
        request->send(400, "text/plain", "Napaka: humThreshold mora biti < humExtremeHighDS"); return;
    }
    if (newSettings.humThresholdDS >= newSettings.humThresholdHighDS) {
        request->send(400, "text/plain", "Napaka: humThresholdDS mora biti < humThresholdHighDS"); return;
    }
    if (newSettings.humThresholdHighDS >= newSettings.humExtremeHighDS) {
        request->send(400, "text/plain", "Napaka: humThresholdHighDS mora biti < humExtremeHighDS"); return;
    }
    if (newSettings.co2ThresholdLowDS >= newSettings.co2ThresholdHighDS) {
        request->send(400, "text/plain", "Napaka: co2ThresholdLowDS mora biti < co2ThresholdHighDS"); return;
    }
    if (newSettings.tempMinThreshold >= newSettings.tempLowThreshold) {
        request->send(400, "text/plain", "Napaka: tempMinThreshold mora biti < tempLowThreshold"); return;
    }
    if (newSettings.tempLowThreshold >= newSettings.tempIdealDS) {
        request->send(400, "text/plain", "Napaka: tempLowThreshold mora biti < tempIdealDS"); return;
    }
    if (newSettings.tempIdealDS >= newSettings.tempExtremeHighDS) {
        request->send(400, "text/plain", "Napaka: tempIdealDS mora biti < tempExtremeHighDS"); return;
    }

    settings = newSettings;
    saveSettings();
    uint16_t new_crc = calculateCRC((const uint8_t*)&settings, sizeof(Settings));

    settingsUpdateSuccess = true;
    settingsUpdateMessage = "Nastavitve shranjene!";
    settingsUpdatePending = false;

    request->send(200, "text/plain", "Nastavitve shranjene!");
}

void handleResetSettings(AsyncWebServerRequest *request) {
    LOG_DEBUG("Web", "Zahtevek: POST /settings/reset");
    LOG_INFO("Web", "Nastavitve ponovno naložene iz NVS");
    settingsUpdateSuccess = true;
    settingsUpdateMessage = "Nastavitve ponovno naložene!";
    settingsUpdatePending = false;

    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["message"] = "Nastavitve ponovno naložene!";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleFactoryResetSettings(AsyncWebServerRequest *request) {
    LOG_DEBUG("Web", "Zahtevek: POST /settings/factory-reset");
    LOG_INFO("Web", "Reset na tovarniške nastavitve");
    initDefaults();
    saveSettings();
    settingsUpdateSuccess = true;
    settingsUpdateMessage = "Reset na tovarniške nastavitve uspešen!";
    settingsUpdatePending = false;

    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["message"] = "Reset na tovarniške nastavitve uspešen!";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleSettingsStatus(AsyncWebServerRequest *request) {
    LOG_DEBUG("Web", "Zahtevek: GET /settings/status");
    DynamicJsonDocument doc(200);
    doc["waiting"] = false;
    doc["success"] = settingsUpdateSuccess;
    doc["message"] = settingsUpdateMessage;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void setupWebServer() {
    LOG_INFO("Web", "Inicializacija web UI strežnika");

    server.on("/", HTTP_GET, handleRoot);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/help", HTTP_GET, handleHelp);
    server.on("/data", HTTP_GET, handleDataRequest);
    server.on("/current-data", HTTP_GET, handleCurrentDataRequest);
    server.on("/settings/update", HTTP_POST, handlePostSettings);
    server.on("/settings/reset", HTTP_POST, handleResetSettings);
    server.on("/settings/factory-reset", HTTP_POST, handleFactoryResetSettings);
    server.on("/settings/status", HTTP_GET, handleSettingsStatus);

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        LOG_DEBUG("Web", "Zahtevek: GET /status");
        String status = "vent_CEE Status\n";
        status += "Uptime: " + String(millis() / 1000) + " seconds\n";
        status += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
        status += "Time Synced: " + String(timeSynced ? "Yes" : "No") + "\n";
        if (timeSynced) {
            status += "Current Time: " + myTZ.dateTime() + "\n";
        }
        status += "External Data Valid: " + String(externalDataValid ? "Yes" : "No") + "\n";
        status += "Last Sensor Update: " + String(lastSensorDataTime) + "\n";
        request->send(200, "text/plain", status);
    });

    server.on("/api/manual-control", HTTP_POST,
        [](AsyncWebServerRequest *request){},
        NULL,
        handleManualControl
    );
    server.on("/api/sensor-data", HTTP_POST,
        [](AsyncWebServerRequest *request){},
        NULL,
        handleSensorData
    );

    server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request){
        String ip = request->client()->remoteIP().toString();
        String source = ip;
        if (ip == IP_REW)     source = "REW";
        else if (ip == IP_UT_DEW)  source = "UT_DEW";
        else if (ip == IP_KOP_DEW) source = "KOP_DEW";
        LOG_DEBUG("Web", "Zahtevek: GET /api/ping from %s", source.c_str());
        request->send(200, "text/plain", "pong");
    });

    server.on("/api/test", HTTP_ANY, [](AsyncWebServerRequest *request){
        LOG_INFO("Web", "TEST ENDPOINT called: %s %s from %s",
            request->methodToString(), request->url().c_str(),
            request->client()->remoteIP().toString().c_str());
        request->send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Test endpoint working\"}");
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        LOG_WARN("Web", "UNHANDLED REQUEST: %s %s from %s",
            request->methodToString(), request->url().c_str(),
            request->client()->remoteIP().toString().c_str());
        request->send(404, "text/plain", "Not found");
    });

    LOG_INFO("Web", "Začenjam strežnik na portu 80");
    server.begin();
    LOG_INFO("Web", "Web UI strežnik zagnan - vsi endpoint-i registrirani");
}
