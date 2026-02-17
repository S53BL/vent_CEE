#include "web.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "globals.h"
#include "logging.h"
#include "system.h"
#include "message_fields.h"
#include "help_html.h"
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

// Helper function to get duty cycle parameter status
String getDutyCycleParamStatus() {
    String status = "";

    // CO2 Low
    bool co2LowActive = !isnan(currentData.livingCO2) && currentData.livingCO2 >= settings.co2ThresholdLowDS;
    status += "CO2 Low: " + String(co2LowActive ? "✓" : "✗") + "<br>";

    // CO2 High
    bool co2HighActive = !isnan(currentData.livingCO2) && currentData.livingCO2 >= settings.co2ThresholdHighDS;
    status += "CO2 High: " + String(co2HighActive ? "✓" : "✗") + "<br>";

    // Hum Low
    bool humLowActive = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= settings.humThresholdDS;
    status += "Hum Low: " + String(humLowActive ? "✓" : "✗") + "<br>";

    // Hum High
    bool humHighActive = !isnan(currentData.livingHumidity) && currentData.livingHumidity >= settings.humThresholdHighDS;
    status += "Hum High: " + String(humHighActive ? "✓" : "✗") + "<br>";

    // Temp (cooling/heating active)
    bool tempActive = !isNNDTime() && !isnan(currentData.livingTemp) && !isnan(currentData.externalTemp) &&
                     ((currentData.livingTemp > settings.tempIdealDS && currentData.externalTemp < currentData.livingTemp) ||
                      (currentData.livingTemp < settings.tempIdealDS && currentData.externalTemp > currentData.livingTemp));
    status += "Temp: " + String(tempActive ? "✓" : "✗") + "<br>";

    // Adverse conditions
    bool adverseActive = currentData.externalHumidity > settings.humExtremeHighDS ||
                        currentData.externalTemp > settings.tempExtremeHighDS ||
                        currentData.externalTemp < settings.tempExtremeLowDS;
    status += "Adverse: " + String(adverseActive ? "✓" : "✗") + "<br>";

    return status;
}

// Helper function to get fan level reason
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

    // Slovenian day names
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

        // Handle manual control based on room and action
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
                currentData.disableUtility = !currentData.disableUtility;
                LOG_INFO("HTTP", "MANUAL_CONTROL: request received - Utility %s", currentData.disableUtility ? "disabled" : "enabled");
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
        if (clientIP == "192.168.2.190" && !rewStatus.isOnline) {
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

        // Update external data from REW
        externalData.externalTemperature = doc[FIELD_EXT_TEMP] | 0.0f;     // external temp
        externalData.externalHumidity = doc[FIELD_EXT_HUM] | 0.0f;       // external humidity
        externalData.externalPressure = doc[FIELD_EXT_PRESS] | 0.0f;       // external pressure
        externalData.livingTempDS = doc[FIELD_DS_TEMP] | 0.0f;           // ds temp (living room)
        externalData.livingHumidityDS = doc[FIELD_DS_HUM] | 0.0f;       // ds humidity
        // Eksplicitno rokovanje z null vrednostjo - ArduinoJson | 0 ne deluje pravilno za null
        JsonVariant co2Variant = doc[FIELD_DS_CO2];
        if (co2Variant.is<uint16_t>()) {
            externalData.livingCO2 = co2Variant.as<uint16_t>();
        } else {
            externalData.livingCO2 = 0;
        }
        externalData.weatherIcon = doc[FIELD_WEATHER_ICON] | 0;          // weather icon
        externalData.seasonCode = doc[FIELD_SEASON_CODE] | 0;            // season code
        externalData.timestamp = doc[FIELD_TIMESTAMP] | 0;                 // timestamp

        // Update currentData for immediate use by vent control logic
        currentData.externalTemp = externalData.externalTemperature;
        currentData.externalHumidity = externalData.externalHumidity;
        currentData.externalPressure = externalData.externalPressure;
        currentData.livingTemp = externalData.livingTempDS;
        currentData.livingHumidity = externalData.livingHumidityDS;
        currentData.livingCO2 = externalData.livingCO2;

        // Update weather icon and season code
        currentWeatherIcon = externalData.weatherIcon;
        currentSeasonCode = externalData.seasonCode;

        // Update external data validation
        if (timeSynced) {
            lastSensorDataTime = myTZ.now();
            externalDataValid = true;
        }

        LOG_INFO("HTTP", "SENSOR_DATA: Received - Ext: %.1f°C/%.1f%%/%.1fhPa, DS: %.1f°C/%.1f%%/%dppm, Icon: %d, Season: %d, TS: %u",
                 externalData.externalTemperature, externalData.externalHumidity, externalData.externalPressure,
                 externalData.livingTempDS, externalData.livingHumidityDS, externalData.livingCO2,
                 externalData.weatherIcon, externalData.seasonCode, externalData.timestamp);

        // Reactivate REW if it was offline
        String clientIP = request->client()->remoteIP().toString();
        if (clientIP == "192.168.2.190" && !rewStatus.isOnline) {
            rewStatus.isOnline = true;
            LOG_INFO("HTTP", "REW reaktiviran ob prejemu SENSOR_DATA od %s", clientIP.c_str());
        }

        request->send(200, "application/json", "{\"status\":\"OK\"}");
    }
}

// HTML template for settings page
const char* HTML_SETTINGS = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>CEE - Ventilacijski sistem</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            font-size: 16px;
            background: #101010;
            color: #e0e0e0;
            display: flex;
        }
        .sidebar {
            position: fixed;
            top: 0;
            left: 0;
            width: 160px;
            height: 100vh;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 0;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
            overflow-y: auto;
            z-index: 1000;
        }
        .sidebar h2 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 20px;
            font-size: 18px;
            text-align: center;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .sidebar .nav-button {
            display: block;
            width: 100%;
            padding: 15px;
            margin-bottom: 10px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            text-align: center;
            border: none;
            cursor: pointer;
            box-sizing: border-box;
        }
        .sidebar .nav-button:hover {
            background-color: #6bb3ff;
        }
        .main-content {
            margin-left: 140px;
            margin-right: 140px;
            padding: 20px;
        }
        .right-sidebar {
            position: fixed;
            top: 0;
            right: 0;
            width: 160px;
            height: 100vh;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 0;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
            overflow-y: auto;
            z-index: 1000;
        }
        .right-sidebar h2 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 20px;
            font-size: 18px;
            text-align: center;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .right-sidebar .action-button {
            display: block;
            width: 100%;
            padding: 15px;
            margin-bottom: 10px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            text-align: center;
            border: none;
            cursor: pointer;
            box-sizing: border-box;
        }
        .right-sidebar .action-button:hover {
            background-color: #6bb3ff;
        }
        .right-sidebar .save-button {
            background-color: #4da6ff;
        }
        .right-sidebar .reset-button {
            background-color: #ffaa00;
        }
        .right-sidebar .reset-button:hover {
            background-color: #e69500;
        }
        .right-sidebar .factory-reset-button {
            background-color: #ff4444;
        }
        .right-sidebar .factory-reset-button:hover {
            background-color: #cc3333;
        }
        h1 {
            text-align: center;
            font-size: 24px;
            color: #e0e0e0;
            margin-bottom: 30px;
        }
        .form-container {
            max-width: 800px;
            margin: 0 auto;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
        }
        table {
            width: 100%;
            border-collapse: collapse;
            font-size: 14px;
            margin-bottom: 20px;
        }
        th, td {
            padding: 12px 8px;
            text-align: left;
            border-bottom: 1px solid #333;
        }
        th {
            background-color: #2a2a2a;
            color: #e0e0e0;
            font-weight: bold;
            border-bottom: 2px solid #4da6ff;
        }
        tr:hover {
            background-color: #252525;
        }
        input[type=number], select {
            padding: 8px 12px;
            font-size: 14px;
            background: #1a1a1a;
            color: #e0e0e0;
            border: 1px solid #555;
            border-radius: 4px;
            width: 80px;
        }
        input[type=number]:focus, select:focus {
            outline: none;
            border-color: #4da6ff;
            box-shadow: 0 0 0 2px rgba(77, 166, 255, 0.2);
        }
        input[type=number] {
            width: 80px;
        }
        select {
            width: 120px;
            cursor: pointer;
        }
        .message {
            padding: 15px;
            border-radius: 4px;
            margin-bottom: 20px;
            font-weight: bold;
        }
        .message.success {
            background: #44ff44;
            color: #101010;
            border: 1px solid #22dd22;
        }
        .message.error {
            background: #ff4444;
            color: white;
            border: 1px solid #dd2222;
        }
        .error {
            color: #ff6b6b;
            text-align: center;
            font-size: 14px;
            background: rgba(255, 107, 107, 0.1);
            border: 1px solid #ff6b6b;
            border-radius: 4px;
            padding: 10px;
            margin: 10px 0;
        }
        .success {
            color: #4da6ff;
            text-align: center;
            font-size: 14px;
            background: rgba(77, 166, 255, 0.1);
            border: 1px solid #4da6ff;
            border-radius: 4px;
            padding: 10px;
            margin: 10px 0;
        }
        .button-group {
            text-align: center;
            margin: 20px 0;
        }
        .submit-btn {
            padding: 10px 20px;
            margin: 0 5px;
            background-color: #4da6ff;
            color: #101010;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
            transition: background-color 0.2s;
        }
        .submit-btn:hover {
            background-color: #6bb3ff;
        }
        .reset-btn {
            background-color: #ff6b6b;
        }
        .reset-btn:hover {
            background-color: #ff5252;
        }
        .tab {
            display: flex;
            justify-content: center;
            border-bottom: 1px solid #333;
            margin-bottom: 20px;
        }
        .tab button {
            background-color: #2a2a2a;
            border: none;
            outline: none;
            cursor: pointer;
            padding: 12px 24px;
            font-size: 16px;
            margin: 0 5px;
            color: #e0e0e0;
            border-radius: 6px 6px 0 0;
            transition: background-color 0.2s;
        }
        .tab button:hover {
            background-color: #3a3a3a;
        }
        .tab button.active {
            background-color: #4da6ff;
            color: #101010;
        }
        .tabcontent {
            display: none;
        }
        .tabcontent.active {
            display: block;
        }
        ul {
            list-style-type: none;
            padding: 0;
        }
        li {
            margin-bottom: 15px;
            padding: 10px;
            background: #252525;
            border-radius: 4px;
            border-left: 4px solid #4da6ff;
        }
        h3 {
            font-size: 18px;
            margin-bottom: 8px;
            color: #4da6ff;
            font-weight: bold;
        }
        p {
            font-size: 14px;
            margin: 0;
            color: #e0e0e0;
            line-height: 1.4;
        }
        .nav-link {
            display: inline-block;
            margin-top: 20px;
            padding: 10px 20px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-weight: bold;
            transition: background-color 0.2s;
        }
        .nav-link:hover {
            background-color: #6bb3ff;
        }
        .form-group {
            margin-bottom: 20px;
        }
        .form-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #e0e0e0;
        }
        .form-group .description {
            font-size: 14px;
            color: #aaa;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <h2>Navigacija</h2>
        <a href="/" class="nav-button">Domača stran</a>
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/help" class="nav-button">Pomoč</a>
        <h3 style="color: #4da6ff; margin: 20px 0 10px 0; font-size: 14px;">Druge enote</h3>
        <a href="http://192.168.2.190/" class="nav-button" style="font-size: 12px;">REW</a>
        <a href="http://192.168.2.191/" class="nav-button" style="font-size: 12px;">SEW</a>
        <a href="http://192.168.2.193/" class="nav-button" style="font-size: 12px;">UT_DEW</a>
        <a href="http://192.168.2.194/" class="nav-button" style="font-size: 12px;">KOP_DEW</a>
    </div>
    <div class="main-content">
        <h1>Ventilacijski sistem - Nastavitve</h1>

    <div id="message" class="message" style="display: none;"></div>

    <div class="form-container">
            <form id="settingsForm">
                <div class="form-group">
                    <label for="humThreshold">Mejna vrednost vlage za kopalnico in utility</label>
                    <input type="number" name="humThreshold" id="humThreshold" step="1" min="0" max="100">
                    <div class="description">Relativna vlaga v kopalnici ali utilityju, ki sproži samodejni vklop ventilatorja. Če se ventilator vklopi prepozno po prhanju, znižajte vrednost (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="fanDuration">Čas delovanja ventilatorjev po sprožilcu</label>
                    <input type="number" name="fanDuration" id="fanDuration" step="1" min="60" max="6000">
                    <div class="description">Čas delovanja ventilatorja po vsakem sprožilcu (ročnem, polavtomatskem ali avtomatskem). Če prezračevanje traja premalo, povečajte na 300 s za boljše sušenje (60–6000 s).</div>
                </div>

                <div class="form-group">
                    <label for="fanOffDuration">Minimalni premor med vklopi v utilityju in WC-ju</label>
                    <input type="number" name="fanOffDuration" id="fanOffDuration" step="1" min="60" max="6000">
                    <div class="description">Minimalni premor med zaporednimi vklopi ventilatorja v utilityju ali WC-ju, da se izogne prepogostim ciklom. Če se vklaplja prevečkrat, povečajte na 1800 s (60–6000 s).</div>
                </div>

                <div class="form-group">
                    <label for="fanOffDurationKop">Minimalni premor med vklopi v kopalnici</label>
                    <input type="number" name="fanOffDurationKop" id="fanOffDurationKop" step="1" min="60" max="6000">
                    <div class="description">Minimalni premor med zaporednimi vklopi v kopalnici, ločeno od drugih prostorov. Če želite hitrejše ponovno prezračevanje po prhanju, skrajšajte na 600 s (60–6000 s).</div>
                </div>

                <div class="form-group">
                    <label for="tempLowThreshold">Prag za zmanjšanje delovanja pri nizki zunanji temperaturi</label>
                    <input type="number" name="tempLowThreshold" id="tempLowThreshold" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pod katero se trajanje ventilatorjev skrajša na polovico, da se zmanjša prepih. Če je pozimi premrzlo, dvignite na 10 °C (-20–40 °C).</div>
                </div>

                <div class="form-group">
                    <label for="tempMinThreshold">Prag za ustavitev delovanja pri minimalni zunanji temperaturi</label>
                    <input type="number" name="tempMinThreshold" id="tempMinThreshold" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pod katero se ventilatorji popolnoma ustavijo, da se izogne vnosu mrzlega zraka. Če želite delovanje v globokem mrazu, znižajte na -15 °C (-20–40 °C).</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowAutomatic">Dovoljenje za samodejno prezračevanje ponoči</label>
                    <select name="dndAllowAutomatic" id="dndAllowAutomatic">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli samodejne vklope ventilatorjev zaradi vlage med nočnim časom (22:00–06:00). Če vas hrup moti ponoči, izklopite.</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowSemiautomatic">Dovoljenje za polsamodejno prezračevanje ponoči</label>
                    <select name="dndAllowSemiautomatic" id="dndAllowSemiautomatic">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli vklope ventilatorjev ob ugasnjeni luči med nočnim časom. Če nočni obiski WC-ja povzročajo hrup, izklopite.</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowManual">Dovoljenje za ročno prezračevanje ponoči</label>
                    <select name="dndAllowManual" id="dndAllowManual">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli ročne vklope ventilatorjev (preko tipke ali spleta) med nočnim časom. Če želite popoln mir, izklopite.</div>
                </div>

                <div class="form-group">
                    <label for="cycleDurationDS">Dolžina prezračevalnega cikla v dnevnem prostoru</label>
                    <input type="number" name="cycleDurationDS" id="cycleDurationDS" step="1" min="60" max="6000">
                    <div class="description">Skupni čas enega prezračevalnega cikla v dnevnem prostoru. Krajši cikli pomenijo hitrejši odziv na spremembe, ampak pogostejše vklope (60–6000 s).</div>
                </div>

                <div class="form-group">
                    <label for="cycleActivePercentDS">Osnovni delež aktivnega delovanja cikla v dnevnem prostoru</label>
                    <input type="number" name="cycleActivePercentDS" id="cycleActivePercentDS" step="1" min="0" max="100">
                    <div class="description">Osnovni odstotek časa, ko ventilator deluje v ciklu (npr. 30 % od 60 s = 18 s delovanja). Če je zrak slab, povečajte na 40 % za močnejše prezračevanje (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="humThresholdDS">Mejna vrednost vlage v dnevnem prostoru</label>
                    <input type="number" name="humThresholdDS" id="humThresholdDS" step="1" min="0" max="100">
                    <div class="description">Notranja vlaga v dnevnem prostoru, ki sproži povečanje cikla prezračevanja. Če vlaga naraste hitro (npr. pri kuhanju), znižajte na 55 % (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="humThresholdHighDS">Zgornja mejna vrednost vlage v dnevnem prostoru</label>
                    <input type="number" name="humThresholdHighDS" id="humThresholdHighDS" step="1" min="0" max="100">
                    <div class="description">Višja notranja vlaga za močnejše povečanje cikla v dnevnem prostoru. Za agresivnejši odziv na visoko vlago znižajte vrednost (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="humExtremeHighDS">Ekstremno visoka vlaga Dnevni prostor</label>
                    <input type="number" name="humExtremeHighDS" id="humExtremeHighDS" step="1" min="0" max="100">
                    <div class="description">Ekstremno visoka zunanja vlaga za zmanjšanje cikla (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="co2ThresholdLowDS">Spodnja mejna vrednost CO2 v dnevnem prostoru</label>
                    <input type="number" name="co2ThresholdLowDS" id="co2ThresholdLowDS" step="1" min="400" max="2000">
                    <div class="description">Raven CO2 v dnevnem prostoru za začetno povečanje cikla. Če zrak postane "težek" hitro (npr. pri več ljudeh), znižajte na 800 ppm (400–2000 ppm).</div>
                </div>

                <div class="form-group">
                    <label for="co2ThresholdHighDS">Zgornja mejna vrednost CO2 v dnevnem prostoru</label>
                    <input type="number" name="co2ThresholdHighDS" id="co2ThresholdHighDS" step="1" min="400" max="2000">
                    <div class="description">Višja raven CO2 za maksimalno povečanje cikla v dnevnem prostoru. Za močnejši odziv na slabo kakovost zraka znižajte vrednost (400–2000 ppm).</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentLowDS">Dodatno povečanje cikla pri spodnjih mejah v dnevnem prostoru</label>
                    <input type="number" name="incrementPercentLowDS" id="incrementPercentLowDS" step="1" min="0" max="100">
                    <div class="description">Dodatni odstotek delovanja cikla ob dosegu nizke meje vlage ali CO2. Če se prezračevanje poveča premalo, dvignite na 20 % (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentHighDS">Dodatno povečanje cikla pri zgornjih mejah v dnevnem prostoru</label>
                    <input type="number" name="incrementPercentHighDS" id="incrementPercentHighDS" step="1" min="0" max="100">
                    <div class="description">Dodatni odstotek delovanja cikla ob dosegu visoke meje vlage ali CO2. Za ekstremne situacije povečajte na 60 % za hitrejše prezračevanje (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentTempDS">Dodatno povečanje cikla za uravnavanje temperature v dnevnem prostoru</label>
                    <input type="number" name="incrementPercentTempDS" id="incrementPercentTempDS" step="1" min="0" max="100">
                    <div class="description">Dodatni odstotek cikla za uravnavanje temperature (hlajenje ali ogrevanje z zunanjim zrakom). Če poleti želite več hlajenja, povečajte na 30 % (0–100 %).</div>
                </div>

                <div class="form-group">
                    <label for="tempIdealDS">Želena temperatura v dnevnem prostoru</label>
                    <input type="number" name="tempIdealDS" id="tempIdealDS" step="1" min="-20" max="40">
                    <div class="description">Ciljna notranja temperatura v dnevnem prostoru, ki usmerja prilagajanje cikla. Če želite hladnejši dom, znižajte na 22 °C (-20–40 °C).</div>
                </div>

                <div class="form-group">
                    <label for="tempExtremeHighDS">Prag za zmanjšanje cikla pri ekstremno visoki zunanji temperaturi</label>
                    <input type="number" name="tempExtremeHighDS" id="tempExtremeHighDS" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pri kateri se cikel prezračevanja zmanjša, da se izogne vnosu vročega zraka. Če poleti ne želite dodatne toplote, znižajte na 28 °C (-20–40 °C).</div>
                </div>

                <div class="form-group">
                    <label for="tempExtremeLowDS">Prag za zmanjšanje cikla pri ekstremno nizki zunanji temperaturi</label>
                    <input type="number" name="tempExtremeLowDS" id="tempExtremeLowDS" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pri kateri se cikel prezračevanja zmanjša, da se izogne vnosu mrzlega zraka. Če pozimi želite manj prepiha, dvignite na -5 °C (-20–40 °C).</div>
                </div>

                <h3 style="color: #4da6ff; margin: 40px 0 20px 0; font-size: 20px; border-bottom: 2px solid #4da6ff; padding-bottom: 5px;">Nastavitve senzorjev</h3>

                <div class="form-group">
                    <label for="bmeTempOffset">Offset temperature BME280 (°C)</label>
                    <input type="number" id="bmeTempOffset" name="bmeTempOffset" step="0.1" min="-10.0" max="10.0">
                    <div class="description">Prilagoditev temperature BME280 senzorja (-10.0 do +10.0 °C)</div>
                </div>

                <div class="form-group">
                    <label for="bmeHumidityOffset">Offset vlažnosti BME280 (%)</label>
                    <input type="number" id="bmeHumidityOffset" name="bmeHumidityOffset" step="0.1" min="-20.0" max="20.0">
                    <div class="description">Prilagoditev vlažnosti BME280 senzorja (-20.0 do +20.0 %)</div>
                </div>

                <div class="form-group">
                    <label for="bmePressureOffset">Offset tlaka BME280 (hPa)</label>
                    <input type="number" id="bmePressureOffset" name="bmePressureOffset" step="0.1" min="-50.0" max="50.0">
                    <div class="description">Prilagoditev tlaka BME280 senzorja (-50.0 do +50.0 hPa)</div>
                </div>

                <div class="form-group">
                    <label for="shtTempOffset">Offset temperature SHT41 (°C)</label>
                    <input type="number" id="shtTempOffset" name="shtTempOffset" step="0.1" min="-10.0" max="10.0">
                    <div class="description">Prilagoditev temperature SHT41 senzorja (-10.0 do +10.0 °C)</div>
                </div>

                <div class="form-group">
                    <label for="shtHumidityOffset">Offset vlažnosti SHT41 (%)</label>
                    <input type="number" id="shtHumidityOffset" name="shtHumidityOffset" step="0.1" min="-20.0" max="20.0">
                    <div class="description">Prilagoditev vlažnosti SHT41 senzorja (-20.0 do +20.0 %)</div>
                </div>
            </form>
        </div>
    </div>
    <div class="right-sidebar">
        <h2>Akcije</h2>
        <button type="button" class="action-button save-button" onclick="saveSettings()">Shrani</button>
        <button type="button" class="action-button reset-button" onclick="resetToDefaults()">Ponastavi</button>

        <div style="margin-top: 40px; padding-top: 20px; border-top: 2px solid #ff4444;">
            <button type="button" class="action-button factory-reset-button" onclick="resetToFactoryDefaults()">Reset na tovarniške nastavitve</button>
        </div>
    </div>
    <div id="Help" class="tabcontent">
        <div class="form-container">
            <ul>
                <li>
                    <h3>HUM_THRESHOLD</h3>
                    <p>Meja vlage za avtomatsko aktivacijo ventilatorjev v Kopalnici in Utility (0–100 %).</p>
                </li>
                <li>
                    <h3>FAN_DURATION</h3>
                    <p>Trajanje delovanja ventilatorjev po sprožilcu (60–6000 s).</p>
                </li>
                <li>
                    <h3>FAN_OFF_DURATION</h3>
                    <p>Čas čakanja pred naslednjim ciklom v Utility in WC (60–6000 s).</p>
                </li>
                <li>
                    <h3>FAN_OFF_DURATION_KOP</h3>
                    <p>Čas čakanja pred naslednjim ciklom v Kopalnici (60–6000 s).</p>
                </li>
                <li>
                    <h3>TEMP_LOW_THRESHOLD</h3>
                    <p>Zunanja temperatura, pod katero se delovanje ventilatorjev zmanjša (-20–40 °C).</p>
                </li>
                <li>
                    <h3>TEMP_MIN_THRESHOLD</h3>
                    <p>Zunanja temperatura, pod katero se ventilatorji ustavijo (-20–40 °C).</p>
                </li>
                <li>
                    <h3>DND_ALLOW_AUTOMATIC</h3>
                    <p>Dovoli avtomatsko aktivacijo ventilatorjev med DND (nočni čas).</p>
                </li>
                <li>
                    <h3>DND_ALLOW_SEMIAUTOMATIC</h3>
                    <p>Dovoli polavtomatske sprožilce (npr. luči) med DND.</p>
                </li>
                <li>
                    <h3>DND_ALLOW_MANUAL</h3>
                    <p>Dovoli ročne sprožilce med DND.</p>
                </li>
                <li>
                    <h3>CYCLE_DURATION_DS</h3>
                    <p>Trajanje cikla prezračevanja v Dnevnem prostoru (60–6000 s).</p>
                </li>
                <li>
                    <h3>CYCLE_ACTIVE_PERCENT_DS</h3>
                    <p>Aktivni delež cikla v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>HUM_THRESHOLD_DS</h3>
                    <p>Meja vlage za povečanje cikla v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>HUM_THRESHOLD_HIGH_DS</h3>
                    <p>Visoka meja vlage za večje povečanje cikla v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>HUM_EXTREME_HIGH_DS</h3>
                    <p>Ekstremno visoka zunanja vlaga za zmanjšanje cikla (0–100 %).</p>
                </li>
                <li>
                    <h3>CO2_THRESHOLD_LOW_DS</h3>
                    <p>Nizka meja CO2 za povečanje cikla v Dnevnem prostoru (400–2000 ppm).</p>
                </li>
                <li>
                    <h3>CO2_THRESHOLD_HIGH_DS</h3>
                    <p>Visoka meja CO2 za večje povečanje cikla v Dnevnem prostoru (400–2000 ppm).</p>
                </li>
                <li>
                    <h3>INCREMENT_PERCENT_LOW_DS</h3>
                    <p>Nizko povečanje cikla ob mejah vlage/CO2 v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>INCREMENT_PERCENT_HIGH_DS</h3>
                    <p>Visoko povečanje cikla ob visokih mejah vlage/CO2 v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>INCREMENT_PERCENT_TEMP_DS</h3>
                    <p>Povečanje cikla za hlajenje/ogrevanje v Dnevnem prostoru (0–100 %).</p>
                </li>
                <li>
                    <h3>TEMP_IDEAL_DS</h3>
                    <p>Idealna temperatura v Dnevnem prostoru (-20–40 °C).</p>
                </li>
                <li>
                    <h3>TEMP_EXTREME_HIGH_DS</h3>
                    <p>Ekstremno visoka zunanja temperatura za zmanjšanje cikla (-20–40 °C).</p>
                </li>
                <li>
                    <h3>TEMP_EXTREME_LOW_DS</h3>
                    <p>Ekstremno nizka zunanja temperatura za zmanjšanje cikla (-20–40 °C).</p>
                </li>
            </ul>
        </div>
    </div>
    <script>
        function openTab(evt, tabName) {
            var i, tabcontent, tablinks;
            tabcontent = document.getElementsByClassName("tabcontent");
            for (i = 0; i < tabcontent.length; i++) {
                tabcontent[i].style.display = "none";
            }
            tablinks = document.getElementsByClassName("tablinks");
            for (i = 0; i < tablinks.length; i++) {
                tablinks[i].className = tablinks[i].className.replace(" active", "");
            }
            document.getElementById(tabName).style.display = "block";
            evt.currentTarget.className += " active";
        }

        let updateInterval;

        function loadCurrentSettings() {
            fetch("/data")
                .then(response => response.json())
                .then(data => {
                    document.getElementById("humThreshold").value = data.HUM_THRESHOLD;
                    document.getElementById("fanDuration").value = data.FAN_DURATION;
                    document.getElementById("fanOffDuration").value = data.FAN_OFF_DURATION;
                    document.getElementById("fanOffDurationKop").value = data.FAN_OFF_DURATION_KOP;
                    document.getElementById("tempLowThreshold").value = data.TEMP_LOW_THRESHOLD;
                    document.getElementById("tempMinThreshold").value = data.TEMP_MIN_THRESHOLD;
                    document.getElementById("dndAllowAutomatic").value = data.DND_ALLOW_AUTOMATIC ? "1" : "0";
                    document.getElementById("dndAllowSemiautomatic").value = data.DND_ALLOW_SEMIAUTOMATIC ? "1" : "0";
                    document.getElementById("dndAllowManual").value = data.DND_ALLOW_MANUAL ? "1" : "0";
                    document.getElementById("cycleDurationDS").value = data.CYCLE_DURATION_DS;
                    document.getElementById("cycleActivePercentDS").value = data.CYCLE_ACTIVE_PERCENT_DS;
                    document.getElementById("humThresholdDS").value = data.HUM_THRESHOLD_DS;
                    document.getElementById("humThresholdHighDS").value = data.HUM_THRESHOLD_HIGH_DS;
                    document.getElementById("humExtremeHighDS").value = data.HUM_EXTREME_HIGH_DS;
                    document.getElementById("co2ThresholdLowDS").value = data.CO2_THRESHOLD_LOW_DS;
                    document.getElementById("co2ThresholdHighDS").value = data.CO2_THRESHOLD_HIGH_DS;
                    document.getElementById("incrementPercentLowDS").value = data.INCREMENT_PERCENT_LOW_DS;
                    document.getElementById("incrementPercentHighDS").value = data.INCREMENT_PERCENT_HIGH_DS;
                    document.getElementById("incrementPercentTempDS").value = data.INCREMENT_PERCENT_TEMP_DS;
                    document.getElementById("tempIdealDS").value = data.TEMP_IDEAL_DS;
                    document.getElementById("tempExtremeHighDS").value = data.TEMP_EXTREME_HIGH_DS;
                    document.getElementById("tempExtremeLowDS").value = data.TEMP_EXTREME_LOW_DS;
                    document.getElementById("bmeTempOffset").value = data.BME_TEMP_OFFSET;
                    document.getElementById("bmeHumidityOffset").value = data.BME_HUMIDITY_OFFSET;
                    document.getElementById("bmePressureOffset").value = data.BME_PRESSURE_OFFSET;
                    document.getElementById("shtTempOffset").value = data.SHT_TEMP_OFFSET;
                    document.getElementById("shtHumidityOffset").value = data.SHT_HUMIDITY_OFFSET;
                })
                .catch(error => console.error('Napaka pri nalaganju nastavitev:', error));
        }

        function updateSettings() {
            loadCurrentSettings();
        }

        function saveSettings() {
            const formData = new FormData();
            formData.append('humThreshold', document.getElementById('humThreshold').value);
            formData.append('fanDuration', document.getElementById('fanDuration').value);
            formData.append('fanOffDuration', document.getElementById('fanOffDuration').value);
            formData.append('fanOffDurationKop', document.getElementById('fanOffDurationKop').value);
            formData.append('tempLowThreshold', document.getElementById('tempLowThreshold').value);
            formData.append('tempMinThreshold', document.getElementById('tempMinThreshold').value);
            formData.append('dndAllowAutomatic', document.getElementById('dndAllowAutomatic').value);
            formData.append('dndAllowSemiautomatic', document.getElementById('dndAllowSemiautomatic').value);
            formData.append('dndAllowManual', document.getElementById('dndAllowManual').value);
            formData.append('cycleDurationDS', document.getElementById('cycleDurationDS').value);
            formData.append('cycleActivePercentDS', document.getElementById('cycleActivePercentDS').value);
            formData.append('humThresholdDS', document.getElementById('humThresholdDS').value);
            formData.append('humThresholdHighDS', document.getElementById('humThresholdHighDS').value);
            formData.append('humExtremeHighDS', document.getElementById('humExtremeHighDS').value);
            formData.append('co2ThresholdLowDS', document.getElementById('co2ThresholdLowDS').value);
            formData.append('co2ThresholdHighDS', document.getElementById('co2ThresholdHighDS').value);
            formData.append('incrementPercentLowDS', document.getElementById('incrementPercentLowDS').value);
            formData.append('incrementPercentHighDS', document.getElementById('incrementPercentHighDS').value);
            formData.append('incrementPercentTempDS', document.getElementById('incrementPercentTempDS').value);
            formData.append('tempIdealDS', document.getElementById('tempIdealDS').value);
            formData.append('tempExtremeHighDS', document.getElementById('tempExtremeHighDS').value);
            formData.append('tempExtremeLowDS', document.getElementById('tempExtremeLowDS').value);
            formData.append('bmeTempOffset', document.getElementById('bmeTempOffset').value);
            formData.append('bmeHumidityOffset', document.getElementById('bmeHumidityOffset').value);
            formData.append('bmePressureOffset', document.getElementById('bmePressureOffset').value);
            formData.append('shtTempOffset', document.getElementById('shtTempOffset').value);
            formData.append('shtHumidityOffset', document.getElementById('shtHumidityOffset').value);

            const data = {};
            for (let [key, value] of formData.entries()) {
                data[key] = value;
            }

            fetch('/settings/update', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: new URLSearchParams(data)
            })
            .then(response => response.text())
            .then(result => {
                if (result === "Nastavitve shranjene!") {
                    showMessage(result, 'success');
                    loadCurrentSettings(); // Reload current values
                } else {
                    showMessage(result, 'error');
                }
            })
            .catch(error => {
                showMessage('Napaka pri shranjevanju: ' + error, 'error');
            });
        }

        function clearMessages() {
            document.getElementById("error-message").textContent = "";
            document.getElementById("success-message").textContent = "";
            // Clear any existing timeouts
            if (window.messageTimeout) {
                clearTimeout(window.messageTimeout);
            }
        }

        function showErrorMessage(message) {
            const errorElement = document.getElementById("error-message");
            errorElement.textContent = message;
            errorElement.style.display = "block";

            // Auto-hide after 5 seconds
            window.messageTimeout = setTimeout(() => {
                errorElement.style.display = "none";
            }, 5000);
        }

        function showSuccessMessage(message) {
            const successElement = document.getElementById("success-message");
            successElement.textContent = message;
            successElement.style.display = "block";

            // Auto-hide after 5 seconds
            window.messageTimeout = setTimeout(() => {
                successElement.style.display = "none";
            }, 5000);
        }

        function showLoadingState(isLoading) {
            const submitBtn = document.querySelector('.submit-btn');
            const resetBtn = document.querySelector('.reset-btn');

            if (isLoading) {
                submitBtn.disabled = true;
                submitBtn.textContent = "Shranjujem...";
                submitBtn.style.opacity = "0.6";
                resetBtn.disabled = true;
                resetBtn.style.opacity = "0.6";
            } else {
                submitBtn.disabled = false;
                submitBtn.textContent = "Shrani";
                submitBtn.style.opacity = "1";
                resetBtn.disabled = false;
                resetBtn.style.opacity = "1";
            }
        }

        function showMessage(text, type) {
            const messageDiv = document.getElementById('message');
            messageDiv.textContent = text;
            messageDiv.className = 'message ' + type;
            messageDiv.style.display = 'block';

            // Auto-hide after 5 seconds
            setTimeout(() => {
                messageDiv.style.display = 'none';
            }, 5000);
        }

        function resetToDefaults() {
            fetch('/settings/reset', {
                method: 'POST'
            })
            .then(response => response.json())
            .then(result => {
                if (result.success) {
                    showMessage(result.message, 'success');
                    updateSettings(); // Reload current values
                } else {
                    showMessage(result.error || 'Napaka pri ponastavitvi', 'error');
                }
            })
            .catch(error => {
                showMessage('Napaka pri ponastavitvi: ' + error, 'error');
            });
        }

        function resetToFactoryDefaults() {
            if (confirm('Ali ste prepričani da želite ponastaviti na tovarniške nastavitve?')) {
                if (confirm('Te operacije ni mogoče preklicati.')) {
                    fetch('/settings/factory-reset', {
                        method: 'POST'
                    })
                    .then(response => response.json())
                    .then(result => {
                        if (result.success) {
                            showMessage(result.message, 'success');
                            updateSettings(); // Reload current values
                        } else {
                            showMessage(result.error || 'Napaka pri resetu', 'error');
                        }
                    })
                    .catch(error => {
                        showMessage('Napaka pri resetu: ' + error, 'error');
                    });
                }
            }
        }

        loadCurrentSettings();
    </script>
</body>
</html>
)rawliteral";



bool settingsUpdatePending = false;
unsigned long settingsUpdateStartTime = 0;
bool settingsUpdateSuccess = false;
String settingsUpdateMessage = "";

// Handle root page with cards layout
void handleRoot(AsyncWebServerRequest *request) {
  // Log only for initial page loads, not AJAX refreshes
  if (!request->hasHeader("X-Requested-With") ||
      request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
    LOG_DEBUG("Web", "Zahtevek: GET /");
  }

  // Calculate system data
  float ramPercent = (ESP.getHeapSize() - ESP.getFreeHeap()) * 100.0 / ESP.getHeapSize();
  String uptimeStr = formatUptime(millis() / 1000);

  // Build HTML with cards
  String html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>CEE - Ventilacijski sistem</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background: #101010;
            color: #e0e0e0;
            display: flex;
        }
        .sidebar {
            position: fixed;
            top: 0;
            left: 0;
            width: 160px;
            height: 100vh;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 0;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
            overflow-y: auto;
            z-index: 1000;
        }
        .sidebar h2 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 20px;
            font-size: 18px;
            text-align: center;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .sidebar .nav-button {
            display: block;
            width: 100%;
            padding: 15px;
            margin-bottom: 10px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            text-align: center;
            border: none;
            cursor: pointer;
            box-sizing: border-box;
        }
        .sidebar .nav-button:hover {
            background-color: #6bb3ff;
        }
        .main-content {
            margin-left: 180px;
            padding: 20px;
        }
        h1 {
            text-align: center;
            font-size: 28px;
            color: #e0e0e0;
            margin-bottom: 30px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 20px;
            max-width: 1400px;
            margin: 0 auto;
        }
        .card {
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
        }
        .card h2 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 15px;
            font-size: 20px;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .status-item {
            display: flex;
            justify-content: space-between;
            margin-bottom: 8px;
            padding: 5px 0;
            border-bottom: 1px solid #333;
        }
        .status-item:last-child {
            border-bottom: none;
        }
        .status-label {
            font-weight: bold;
            color: #ccc;
        }
        .status-value {
            color: #e0e0e0;
        }
        .time-display {
            text-align: center;
            margin-bottom: 20px;
            font-size: 16px;
            color: #4da6ff;
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <h2>Navigacija</h2>
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/help" class="nav-button">Pomoč</a>
        <h3 style="color: #4da6ff; margin: 20px 0 10px 0; font-size: 14px;">Druge enote</h3>
        <a href="http://192.168.2.190/" class="nav-button" style="font-size: 12px;">REW</a>
        <a href="http://192.168.2.191/" class="nav-button" style="font-size: 12px;">SEW</a>
        <a href="http://192.168.2.193/" class="nav-button" style="font-size: 12px;">UT_DEW</a>
        <a href="http://192.168.2.194/" class="nav-button" style="font-size: 12px;">KOP_DEW</a>
    </div>
    <div class="main-content">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
            <h1 style="margin: 0;">Ventilacijski sistem</h1>
            <div id="dateDisplay" style="text-align: right; color: #4da6ff; font-size: 16px; font-weight: bold;">)rawliteral" + getSlovenianDateTime() + R"rawliteral(</div>
        </div>

        <div class="grid">
        <!-- WC Card -->
        <div class="card">
            <h2>WC</h2>
            <div class="status-item">
                <span class="status-label">Tlak:</span>
                <span class="status-value" id="wc-pressure">)rawliteral" + String((int)currentData.bathroomPressure) + R"rawliteral( hPa</span>
            </div>
            <div class="status-item">
                <span class="status-label">Luč:</span>
                <span class="status-value" id="wc-light">)rawliteral" + getLightStatus(currentData.wcLight) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Ventilator:</span>
                <span class="status-value" id="wc-fan">)rawliteral" + getFanStatus(currentData.wcFan, currentData.disableWc) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Preostali čas:</span>
                <span class="status-value" id="wc-remaining">)rawliteral" + String(getRemainingTime(2)) + R"rawliteral( s</span>
            </div>
        </div>

        <!-- KOP Card -->
        <div class="card">
            <h2>Kopalnica</h2>
            <div class="status-item">
                <span class="status-label">Temperatura:</span>
                <span class="status-value" id="kop-temp">)rawliteral" + String(currentData.bathroomTemp, 1) + R"rawliteral( °C</span>
            </div>
            <div class="status-item">
                <span class="status-label">Vlaga:</span>
                <span class="status-value" id="kop-humidity">)rawliteral" + String(currentData.bathroomHumidity, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">Tipka:</span>
                <span class="status-value" id="kop-button">)rawliteral" + (currentData.bathroomButton ? "Pritisnjena" : "Nepritisnjena") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Luč 1:</span>
                <span class="status-value" id="kop-light1">)rawliteral" + getLightStatus(currentData.bathroomLight1) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Luč 2:</span>
                <span class="status-value" id="kop-light2">)rawliteral" + getLightStatus(currentData.bathroomLight2) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Ventilator:</span>
                <span class="status-value" id="kop-fan">)rawliteral" + getFanStatus(currentData.bathroomFan, currentData.disableBathroom) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Preostali čas:</span>
                <span class="status-value" id="kop-remaining">)rawliteral" + String(getRemainingTime(0)) + R"rawliteral( s</span>
            </div>
            <div class="status-item">
                <span class="status-label">Drying mode:</span>
                <span class="status-value" id="kop-drying-mode">)rawliteral" + (currentData.bathroomDryingMode ? "DA" : "NE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Cycle mode:</span>
                <span class="status-value" id="kop-cycle-mode">)rawliteral" + String(currentData.bathroomCycleMode) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Expected end:</span>
                <span class="status-value" id="kop-expected-end">)rawliteral" + (currentData.bathroomDryingMode ? String(currentData.bathroomExpectedEndTime) : "N/A") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <button onclick="triggerDrying('kop')" style="padding: 5px 10px; background-color: #4da6ff; color: white; border: none; border-radius: 4px; cursor: pointer;">Start Drying Cycle</button>
            </div>
        </div>

        <!-- UT Card -->
        <div class="card">
            <h2>Utility</h2>
            <div class="status-item">
                <span class="status-label">Temperatura:</span>
                <span class="status-value" id="ut-temp">)rawliteral" + String(currentData.utilityTemp, 1) + R"rawliteral( °C</span>
            </div>
            <div class="status-item">
                <span class="status-label">Vlaga:</span>
                <span class="status-value" id="ut-humidity">)rawliteral" + String(currentData.utilityHumidity, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">Luč:</span>
                <span class="status-value" id="ut-light">)rawliteral" + getLightStatus(currentData.utilityLight) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Stikalo:</span>
                <span class="status-value" id="ut-switch">)rawliteral" + (currentData.utilitySwitch ? "ON" : "OFF") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Ventilator:</span>
                <span class="status-value" id="ut-fan">)rawliteral" + getFanStatus(currentData.utilityFan, currentData.disableUtility) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Preostali čas:</span>
                <span class="status-value" id="ut-remaining">)rawliteral" + String(getRemainingTime(1)) + R"rawliteral( s</span>
            </div>
            <div class="status-item">
                <span class="status-label">Drying mode:</span>
                <span class="status-value" id="ut-drying-mode">)rawliteral" + (currentData.utilityDryingMode ? "DA" : "NE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Cycle mode:</span>
                <span class="status-value" id="ut-cycle-mode">)rawliteral" + String(currentData.utilityCycleMode) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Expected end:</span>
                <span class="status-value" id="ut-expected-end">)rawliteral" + (currentData.utilityDryingMode ? String(currentData.utilityExpectedEndTime) : "N/A") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <button onclick="triggerDrying('ut')" style="padding: 5px 10px; background-color: #4da6ff; color: white; border: none; border-radius: 4px; cursor: pointer;">Start Drying Cycle</button>
            </div>
        </div>

        <!-- DS Card -->
        <div class="card">
            <h2>Dnevni prostor</h2>
            <div class="status-item">
                <span class="status-label">Temperatura:</span>
                <span class="status-value" id="ds-temp">)rawliteral" + String(currentData.livingTemp, 1) + R"rawliteral( °C</span>
            </div>
            <div class="status-item">
                <span class="status-label">Vlaga:</span>
                <span class="status-value" id="ds-humidity">)rawliteral" + String(currentData.livingHumidity, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">CO2:</span>
                <span class="status-value" id="ds-co2">)rawliteral" + String((int)currentData.livingCO2) + R"rawliteral( ppm</span>
            </div>
            <div class="status-item">
                <span class="status-label">Strešno okno:</span>
                <span class="status-value" id="ds-window1">)rawliteral" + (currentData.windowSensor2 ? "Odprto" : "Zaprto") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Balkonska vrata:</span>
                <span class="status-value" id="ds-window2">)rawliteral" + (currentData.windowSensor1 ? "Odprto" : "Zaprto") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Stopnja ventilatorja:</span>
                <span class="status-value" id="ds-fan-level">)rawliteral" + String(currentData.livingExhaustLevel) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Razmerje:</span>
                <span class="status-value" id="ds-duty-cycle">)rawliteral" + String(currentData.livingRoomDutyCycle, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">Preostali čas:</span>
                <span class="status-value" id="ds-remaining">)rawliteral" + String(getRemainingTime(4)) + R"rawliteral( s</span>
            </div>
        </div>

        <!-- External Data Card -->
        <div class="card">
            <h2>Zunanji podatki</h2>
            <div class="status-item">
                <span class="status-label">Temperatura:</span>
                <span class="status-value" id="ext-temp">)rawliteral" + String(currentData.externalTemp, 1) + R"rawliteral( °C</span>
            </div>
            <div class="status-item">
                <span class="status-label">Vlaga:</span>
                <span class="status-value" id="ext-humidity">)rawliteral" + String(currentData.externalHumidity, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">Tlak:</span>
                <span class="status-value" id="ext-pressure">)rawliteral" + String(currentData.externalPressure, 1) + R"rawliteral( hPa</span>
            </div>
            <div class="status-item">
                <span class="status-label">Svetloba:</span>
                <span class="status-value" id="ext-light">)rawliteral" + String(currentData.externalLight, 1) + R"rawliteral( lux</span>
            </div>
            <div class="status-item">
                <span class="status-label">Ikona:</span>
                <span class="status-value" id="ext-weather-icon">)rawliteral" + String(currentWeatherIcon) + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Sezona:</span>
                <span class="status-value" id="ext-season">)rawliteral" + getSeasonName() + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">DND:</span>
                <span class="status-value" id="ext-dnd">)rawliteral" + (isDNDTime() ? "DA" : "NE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">NND:</span>
                <span class="status-value" id="ext-nnd">)rawliteral" + (isNNDTime() ? "DA" : "NE") + R"rawliteral(</span>
            </div>
        </div>

        <!-- Power Card -->
        <div class="card">
            <h2>Napajanje</h2>
            <div class="status-item">
                <span class="status-label">3.3V:</span>
                <span class="status-value" id="power-3v3">)rawliteral" + String(currentData.supply3V3, 3) + R"rawliteral( V</span>
            </div>
            <div class="status-item">
                <span class="status-label">5V:</span>
                <span class="status-value" id="power-5v">)rawliteral" + String(currentData.supply5V, 3) + R"rawliteral( V</span>
            </div>
            <div class="status-item">
                <span class="status-label">Poraba:</span>
                <span class="status-value" id="power-current">)rawliteral" + String(currentData.currentPower, 1) + R"rawliteral( W</span>
            </div>
            <div class="status-item">
                <span class="status-label">Energija:</span>
                <span class="status-value" id="power-energy">)rawliteral" + String(currentData.energyConsumption, 1) + R"rawliteral( Wh</span>
            </div>
        </div>

        <!-- System Card -->
        <div class="card">
            <h2>Sistem</h2>
            <div class="status-item">
                <span class="status-label">Prosti RAM:</span>
                <span class="status-value" id="sys-ram">)rawliteral" + String(ramPercent, 1) + R"rawliteral( %</span>
            </div>
            <div class="status-item">
                <span class="status-label">Uptime:</span>
                <span class="status-value" id="sys-uptime">)rawliteral" + uptimeStr + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Log buffer:</span>
                <span class="status-value" id="sys-log">)rawliteral" + String(logBuffer.length()) + R"rawliteral( B</span>
            </div>
        </div>

        <!-- Status Card -->
        <div class="card">
            <h2>Status</h2>
            <div id="status-content" class="status-value">
                <div class="status-item">
                    <span class="status-label">Zunanji podatki:</span>
                    <span class="status-value" id="status-external">)rawliteral" + (externalDataValid ? "VELJAVNI" : "NEVELJAVNI") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Senzor BME280:</span>
                    <span class="status-value" id="status-bme280">)rawliteral" + ((currentData.errorFlags & ERR_BME280) ? "NAPAKA" : "OK") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Senzor SHT41:</span>
                    <span class="status-value" id="status-sht41">)rawliteral" + ((currentData.errorFlags & ERR_SHT41) ? "NAPAKA" : "OK") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Napajanje:</span>
                    <span class="status-value" id="status-power">)rawliteral" + ((currentData.errorFlags & ERR_POWER) ? "NAPAKA" : "OK") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">NTP sinhronizacija:</span>
                    <span class="status-value" id="status-ntp">)rawliteral" + (timeSynced ? "OK" : "NESINHRONIZIRAN") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">Čas z REW:</span>
                    <span class="status-value" id="status-time-rew">)rawliteral" + (externalDataValid && timeSynced && abs((int32_t)(myTZ.now() - externalData.timestamp)) <= 300 ? "OK" : "RAZHAJANJE >5min") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">REW:</span>
                    <span class="status-value" id="status-rew">)rawliteral" + (rewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">UT_DEW:</span>
                    <span class="status-value" id="status-ut-dew">)rawliteral" + (utDewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
                </div>
                <div class="status-item">
                    <span class="status-label">KOP_DEW:</span>
                    <span class="status-value" id="status-kop-dew">)rawliteral" + (kopDewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
                </div>
            </div>
        </div>

        <!-- DS Duty Cycle Card - Full width at bottom -->
        <div class="card" style="grid-column: 1 / -1;">
            <h2>DS Duty Cycle Parametri</h2>
            <div class="status-item">
                <span class="status-label">Razlog za level:</span>
                <span class="status-value">)rawliteral" + getFanLevelReason() + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Podroben izračun:</span>
            </div>
            <pre style="font-size: 14px; white-space: pre-wrap; word-wrap: break-word;">)rawliteral" + getDutyCycleBreakdown() + R"rawliteral(</pre>
        </div>
    </div>

    <script>
        function updatePage(isAjax = false) {
            const options = {};
            if (isAjax) {
                options.headers = { 'X-Requested-With': 'XMLHttpRequest' };
            }
            fetch('/current-data', options)
                .then(response => response.json())
                .then(data => {
                    // Update time display
                    const dateDisplay = document.getElementById("dateDisplay");
                    dateDisplay.textContent = data.current_time;

                    // Update WC data
                    document.getElementById("wc-pressure").textContent = data.bathroom_pressure + " hPa";
                    document.getElementById("wc-light").textContent = data.wc_light ? "ON" : "OFF";
                    document.getElementById("wc-fan").textContent = data.wc_disabled ? "DISABLED" : (data.wc_fan ? "ON" : "OFF");
                    document.getElementById("wc-remaining").textContent = data.wc_remaining + " s";

                    // Update KOP data
                    document.getElementById("kop-temp").textContent = data.bathroom_temp + " °C";
                    document.getElementById("kop-humidity").textContent = data.bathroom_humidity + " %";
                    document.getElementById("kop-button").textContent = data.bathroom_button ? "Pritisnjena" : "Nepritisnjena";
                    document.getElementById("kop-light1").textContent = data.bathroom_light1 ? "ON" : "OFF";
                    document.getElementById("kop-light2").textContent = data.bathroom_light2 ? "ON" : "OFF";
                    document.getElementById("kop-fan").textContent = data.bathroom_disabled ? "DISABLED" : (data.bathroom_fan ? "ON" : "OFF");
                    document.getElementById("kop-remaining").textContent = data.bathroom_remaining + " s";
                    document.getElementById("kop-drying-mode").textContent = data.bathroom_drying_mode ? "DA" : "NE";
                    document.getElementById("kop-cycle-mode").textContent = data.bathroom_cycle_mode;
                    // Format expected end time as HH:MM:SS
                    if (data.bathroom_expected_end_time && data.bathroom_expected_end_time > 0) {
                        const expectedDate = new Date(data.bathroom_expected_end_time * 1000);
                        const hours = expectedDate.getHours().toString().padStart(2, '0');
                        const minutes = expectedDate.getMinutes().toString().padStart(2, '0');
                        const seconds = expectedDate.getSeconds().toString().padStart(2, '0');
                        document.getElementById("kop-expected-end").textContent = `${hours}:${minutes}:${seconds}`;
                    } else {
                        document.getElementById("kop-expected-end").textContent = "N/A";
                    }

                    // Update UT data
                    document.getElementById("ut-temp").textContent = data.utility_temp + " °C";
                    document.getElementById("ut-humidity").textContent = data.utility_humidity + " %";
                    document.getElementById("ut-light").textContent = data.utility_light ? "ON" : "OFF";
                    document.getElementById("ut-switch").textContent = data.utility_switch ? "ON" : "OFF";
                    document.getElementById("ut-fan").textContent = data.utility_disabled ? "DISABLED" : (data.utility_fan ? "ON" : "OFF");
                    document.getElementById("ut-remaining").textContent = data.utility_remaining + " s";
                    document.getElementById("ut-drying-mode").textContent = data.utility_drying_mode ? "DA" : "NE";
                    document.getElementById("ut-cycle-mode").textContent = data.utility_cycle_mode;
                    // Format expected end time as HH:MM:SS for UT
                    if (data.utility_expected_end_time && data.utility_expected_end_time > 0) {
                        const expectedDate = new Date(data.utility_expected_end_time * 1000);
                        const hours = expectedDate.getHours().toString().padStart(2, '0');
                        const minutes = expectedDate.getMinutes().toString().padStart(2, '0');
                        const seconds = expectedDate.getSeconds().toString().padStart(2, '0');
                        document.getElementById("ut-expected-end").textContent = `${hours}:${minutes}:${seconds}`;
                    } else {
                        document.getElementById("ut-expected-end").textContent = "N/A";
                    }

                    // Update DS data
                    document.getElementById("ds-temp").textContent = data.living_temp + " °C";
                    document.getElementById("ds-humidity").textContent = data.living_humidity + " %";
                    document.getElementById("ds-co2").textContent = data.living_co2 + " ppm";
                    document.getElementById("ds-window1").textContent = data.living_window2 ? "Odprto" : "Zaprto";
                    document.getElementById("ds-window2").textContent = data.living_window1 ? "Odprto" : "Zaprto";
                    document.getElementById("ds-fan-level").textContent = data.living_fan_level;
                    document.getElementById("ds-duty-cycle").textContent = data.living_duty_cycle + " %";
                    document.getElementById("ds-remaining").textContent = data.living_remaining + " s";

                    // Update external data
                    document.getElementById("ext-temp").textContent = data.external_temp + " °C";
                    document.getElementById("ext-humidity").textContent = data.external_humidity + " %";
                    document.getElementById("ext-pressure").textContent = data.external_pressure + " hPa";
                    document.getElementById("ext-light").textContent = data.external_light + " lux";

                    // Update power data
                    document.getElementById("power-3v3").textContent = data.supply_3v3 + " V";
                    document.getElementById("power-5v").textContent = data.supply_5v + " V";
                    document.getElementById("power-current").textContent = data.current_power + " W";
                    document.getElementById("power-energy").textContent = data.energy_consumption + " Wh";

                    // Update system data
                    document.getElementById("sys-ram").textContent = data.ram_percent + " %";
                    document.getElementById("sys-uptime").textContent = data.uptime;
                    document.getElementById("sys-log").textContent = data.log_buffer_size + " B";

                    // Update status items
                    document.getElementById("status-external").textContent = data.external_data_valid ? "VELJAVNI" : "NEVELJAVNI";
                    document.getElementById("status-bme280").textContent = (data.error_flags & 1) ? "NAPAKA" : "OK";
                    document.getElementById("status-sht41").textContent = (data.error_flags & 2) ? "NAPAKA" : "OK";
                    document.getElementById("status-power").textContent = (data.error_flags & 64) ? "NAPAKA" : "OK";
                    document.getElementById("status-ntp").textContent = data.time_synced ? "OK" : "NESINHRONIZIRAN";
                    document.getElementById("status-time-rew").textContent = (data.external_data_valid && data.time_synced && Math.abs(Date.now()/1000 - data.external_timestamp) <= 300) ? "OK" : "RAZHAJANJE >5min";
                    document.getElementById("status-rew").textContent = data.rew_online ? "ONLINE" : "OFFLINE";
                    document.getElementById("status-ut-dew").textContent = data.ut_dew_online ? "ONLINE" : "OFFLINE";
                    document.getElementById("status-kop-dew").textContent = data.kop_dew_online ? "ONLINE" : "OFFLINE";
                })
                .catch(error => console.error('Napaka pri osveževanju:', error));
        }

        function triggerDrying(room) {
            fetch('/api/manual-control', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ room: room, action: 'drying' })
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'OK') {
                    console.log('Drying cycle triggered for ' + room);
                } else {
                    console.error('Error triggering drying cycle:', data.message);
                }
            })
            .catch(error => {
                console.error('Error:', error);
            });
        }

        function getErrorDescription(errorFlags) {
            let errors = "";
            if (errorFlags & 1) errors += "Senzor BME280 nedelujoč<br>";
            if (errorFlags & 2) errors += "Senzor SHT41 nedelujoč<br>";
            if (errorFlags & 64) errors += "Napajalna napaka<br>";
            if (errors === "") errors = "Brez napak";
            return errors;
        }

        setInterval(() => updatePage(true), 10000);
        updatePage(); // Initial update
    </script>
</body>
</html>)rawliteral";

  request->send(200, "text/html", html);
}

// Handle help page
void handleHelp(AsyncWebServerRequest *request) {
  // Log only for initial page loads, not AJAX refreshes
  if (!request->hasHeader("X-Requested-With") ||
      request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
    LOG_DEBUG("Web", "Zahtevek: GET /help");
  }

  request->send(200, "text/html", HTML_HELP);
}

// Handle settings page
void handleSettings(AsyncWebServerRequest *request) {
  // Log only for initial page loads, not AJAX refreshes
  if (!request->hasHeader("X-Requested-With") ||
      request->getHeader("X-Requested-With")->value() != "XMLHttpRequest") {
    LOG_DEBUG("Web", "Zahtevek: GET /settings");
  }

  // Send the HTML directly
  request->send(200, "text/html", HTML_SETTINGS);
}

void handleDataRequest(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /data");

  // Create a temporary settings struct to read from NVS
  Settings tempSettings;

  // Read from NVS using the same method as loadSettings()
  Preferences prefs;
  prefs.begin("settings", true);

  // Check marker for data validation
  uint8_t marker = prefs.getUChar("marker", 0x00);
  bool dataValid = (marker == SETTINGS_MARKER);

  if (dataValid) {
    // Read settings as blob
    size_t bytesRead = prefs.getBytes("settings", (uint8_t*)&tempSettings, sizeof(Settings));
    uint16_t stored_crc = prefs.getUShort("settings_crc", 0);

    if (bytesRead == sizeof(Settings)) {
      // Calculate CRC
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

  // Use default values if data is invalid
  if (!dataValid) {
    initDefaults();
    tempSettings = settings;  // Use the global defaults
  }

  // Build JSON response using the tempSettings struct
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
                String("\"SHT_HUMIDITY_OFFSET\":\"") + String(tempSettings.shtHumidityOffset, 2) + "\"}" ;

  request->send(200, "application/json", json);
}

void handleCurrentDataRequest(AsyncWebServerRequest *request) {
  // Log only for initial page loads, not AJAX refreshes
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
  
  // Validacija minimalnih vrednosti za CO2
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

  // Validacija relacij med parametri
  if (newSettings.humThreshold >= newSettings.humExtremeHighDS) {
    errorMessage = "Napaka: humThreshold mora biti < humExtremeHighDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.humThresholdDS >= newSettings.humThresholdHighDS) {
    errorMessage = "Napaka: humThresholdDS mora biti < humThresholdHighDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.humThresholdHighDS >= newSettings.humExtremeHighDS) {
    errorMessage = "Napaka: humThresholdHighDS mora biti < humExtremeHighDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.co2ThresholdLowDS >= newSettings.co2ThresholdHighDS) {
    errorMessage = "Napaka: co2ThresholdLowDS mora biti < co2ThresholdHighDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.tempMinThreshold >= newSettings.tempLowThreshold) {
    errorMessage = "Napaka: tempMinThreshold mora biti < tempLowThreshold";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.tempLowThreshold >= newSettings.tempIdealDS) {
    errorMessage = "Napaka: tempLowThreshold mora biti < tempIdealDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }
  if (newSettings.tempIdealDS >= newSettings.tempExtremeHighDS) {
    errorMessage = "Napaka: tempIdealDS mora biti < tempExtremeHighDS";
    LOG_WARN("Web", "Validacijska napaka: %s", errorMessage.c_str());
    request->send(400, "text/plain", errorMessage);
    return;
  }

  // Shrani v NVS
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

  // Web UI endpoints
  LOG_DEBUG("Web", "Registriram web UI endpoint-e");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/help", HTTP_GET, handleHelp);
  server.on("/data", HTTP_GET, handleDataRequest);
  server.on("/current-data", HTTP_GET, handleCurrentDataRequest);
  server.on("/settings/update", HTTP_POST, handlePostSettings);
  server.on("/settings/reset", HTTP_POST, handleResetSettings);
  server.on("/settings/factory-reset", HTTP_POST, handleFactoryResetSettings);
  server.on("/settings/status", HTTP_GET, handleSettingsStatus);



  // Status endpoint
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

  // API endpoints for external control
  LOG_DEBUG("Web", "Registriram API endpoint-e");
  server.on("/api/manual-control", HTTP_POST,
    [](AsyncWebServerRequest *request){},  // empty header handler
    NULL,  // upload handler (not needed)
    handleManualControl  // body handler
  );
  LOG_INFO("Web", "Registered /api/manual-control endpoint");
  server.on("/api/sensor-data", HTTP_POST,
    [](AsyncWebServerRequest *request){},  // empty header handler
    NULL,  // upload handler (not needed)
    handleSensorData  // body handler
  );
  LOG_INFO("Web", "Registered /api/sensor-data endpoint");

  // Heartbeat endpoint
  server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    String ip = request->client()->remoteIP().toString();
    String source = ip;
    if (ip == "192.168.2.190") source = "REW";
    else if (ip == "192.168.2.193") source = "UT_DEW";
    else if (ip == "192.168.2.194") source = "KOP_DEW";
    LOG_DEBUG("Web", "Zahtevek: GET /api/ping from %s", source.c_str());
    request->send(200, "text/plain", "pong");
  });

  // Test endpoint for debugging
  server.on("/api/test", HTTP_ANY, [](AsyncWebServerRequest *request){
    LOG_INFO("Web", "TEST ENDPOINT called: %s %s from %s", request->methodToString(), request->url().c_str(), request->client()->remoteIP().toString().c_str());
    request->send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Test endpoint working\"}");
  });
  LOG_INFO("Web", "Registered /api/test endpoint");

  // Catch-all handler for debugging - log all requests
  server.onNotFound([](AsyncWebServerRequest *request) {
    LOG_WARN("Web", "UNHANDLED REQUEST: %s %s from %s", request->methodToString(), request->url().c_str(), request->client()->remoteIP().toString().c_str());
    request->send(404, "text/plain", "Not found");
  });

  LOG_INFO("Web", "Začenjam strežnik na portu 80");
  server.begin();
  LOG_INFO("Web", "Web UI strežnik zagnan - vsi endpoint-i registrirani");
}
