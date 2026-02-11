#include "web.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "globals.h"
#include "logging.h"
#include "system.h"
#include "message_fields.h"

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
    return (currentData.windowSensor1 && currentData.windowSensor2) ? "Odprta" : "Zaprta";
}

String getErrorDescription(uint8_t errorFlags) {
    String errors = "";
    if (errorFlags & ERR_BME280) errors += "Senzor BME280 nedelujoč<br>";
    if (errorFlags & ERR_SHT41) errors += "Senzor SHT41 nedelujoč<br>";
    if (errorFlags & ERR_LITTLEFS) errors += "LittleFS napaka<br>";
    if (errorFlags & ERR_HTTP) errors += "HTTP napaka<br>";
    if (errorFlags & ERR_POWER) errors += "Napajalna napaka<br>";
    if (errors == "") errors = "Brez napak";
    return errors;
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
        } else {
            LOG_ERROR("HTTP", "Unknown action: %s", action.c_str());
            request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown action\"}");
            return;
        }

        request->send(200, "application/json", "{\"status\":\"OK\"}");
    }
}

void handleSensorData(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    LOG_DEBUG("HTTP", "handleSensorData called - index=%d, len=%d, total=%d", index, len, total);
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
        externalData.livingCO2 = doc[FIELD_DS_CO2] | 0;                 // ds CO2
        externalData.timestamp = doc[FIELD_TIMESTAMP] | 0;                 // timestamp

        // Update currentData for immediate use by vent control logic
        currentData.externalTemp = externalData.externalTemperature;
        currentData.externalHumidity = externalData.externalHumidity;
        currentData.externalPressure = externalData.externalPressure;
        currentData.livingTemp = externalData.livingTempDS;
        currentData.livingHumidity = externalData.livingHumidityDS;
        currentData.livingCO2 = externalData.livingCO2;

        // Update external data validation
        if (timeSynced) {
            lastSensorDataTime = myTZ.now();
            externalDataValid = true;
        }

        LOG_INFO("HTTP", "SENSOR_DATA: Received - Temp: %.1f°C, Hum: %.1f%%, CO2: %d, internal updated",
                 externalData.externalTemperature, externalData.externalHumidity, externalData.livingCO2);

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
    <h1>Ventilacijski sistem - Nastavitve</h1>
    <div id="timeDisplay" style="text-align: center; margin: 10px 0; font-size: 16px; color: #4da6ff;"></div>
    <div style="text-align: center; margin-bottom: 20px;">
        <a href="/" class="nav-link">← Nazaj na začetno stran</a>
    </div>
    <div class="tab">
        <button class="tablinks active" onclick="openTab(event, 'Settings')">Nastavitve</button>
        <button class="tablinks" onclick="openTab(event, 'Help')">Pomoč</button>
    </div>
    <div id="Settings" class="tabcontent active">
        <div class="form-container">
            <form id="settingsForm">
                <div class="error" id="error-message"></div>
                <div class="success" id="success-message"></div>
                <div class="button-group">
                    <input type="button" value="Shrani" class="submit-btn" onclick="saveSettings()">
                    <input type="button" value="Razveljavi spremembe" class="submit-btn reset-btn" onclick="updateSettings()">
                </div>
                <div class="form-group">
                    <label for="humThreshold">Meja vlage Kopalnica/Utility</label>
                    <input type="number" name="humThreshold" id="humThreshold" step="1" min="0" max="100">
                    <div class="description">Meja vlage za avtomatsko aktivacijo ventilatorjev v Kopalnici in Utility (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="fanDuration">Trajanje ventilatorjev</label>
                    <input type="number" name="fanDuration" id="fanDuration" step="1" min="60" max="6000">
                    <div class="description">Trajanje delovanja ventilatorjev po sprožilcu (60–6000 s)</div>
                </div>

                <div class="form-group">
                    <label for="fanOffDuration">Čakanje Utility/WC</label>
                    <input type="number" name="fanOffDuration" id="fanOffDuration" step="1" min="60" max="6000">
                    <div class="description">Čas čakanja pred naslednjim ciklom v Utility in WC (60–6000 s)</div>
                </div>

                <div class="form-group">
                    <label for="fanOffDurationKop">Čakanje Kopalnica</label>
                    <input type="number" name="fanOffDurationKop" id="fanOffDurationKop" step="1" min="60" max="6000">
                    <div class="description">Čas čakanja pred naslednjim ciklom v Kopalnici (60–6000 s)</div>
                </div>

                <div class="form-group">
                    <label for="tempLowThreshold">Meja nizke zunanje temperature</label>
                    <input type="number" name="tempLowThreshold" id="tempLowThreshold" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pod katero se delovanje ventilatorjev zmanjša (-20–40 °C)</div>
                </div>

                <div class="form-group">
                    <label for="tempMinThreshold">Meja minimalne zunanje temperature</label>
                    <input type="number" name="tempMinThreshold" id="tempMinThreshold" step="1" min="-20" max="40">
                    <div class="description">Zunanja temperatura, pod katero se ventilatorji ustavijo (-20–40 °C)</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowAutomatic">DND dovoli avtomatiko</label>
                    <select name="dndAllowAutomatic" id="dndAllowAutomatic">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli avtomatsko aktivacijo ventilatorjev med DND (nočni čas)</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowSemiautomatic">DND dovoli polavtomatiko</label>
                    <select name="dndAllowSemiautomatic" id="dndAllowSemiautomatic">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli polavtomatske sprožilce (npr. luči) med DND</div>
                </div>

                <div class="form-group">
                    <label for="dndAllowManual">DND dovoli ročno upravljanje</label>
                    <select name="dndAllowManual" id="dndAllowManual">
                        <option value="0">0 (Izključeno)</option>
                        <option value="1">1 (Vključeno)</option>
                    </select>
                    <div class="description">Dovoli ročne sprožilce med DND</div>
                </div>

                <div class="form-group">
                    <label for="cycleDurationDS">Trajanje cikla Dnevni prostor</label>
                    <input type="number" name="cycleDurationDS" id="cycleDurationDS" step="1" min="60" max="6000">
                    <div class="description">Trajanje cikla prezračevanja v Dnevnem prostoru (60–6000 s)</div>
                </div>

                <div class="form-group">
                    <label for="cycleActivePercentDS">Aktivni delež cikla Dnevni prostor</label>
                    <input type="number" name="cycleActivePercentDS" id="cycleActivePercentDS" step="1" min="0" max="100">
                    <div class="description">Aktivni delež cikla v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="humThresholdDS">Meja vlage Dnevni prostor</label>
                    <input type="number" name="humThresholdDS" id="humThresholdDS" step="1" min="0" max="100">
                    <div class="description">Meja vlage za povečanje cikla v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="humThresholdHighDS">Visoka meja vlage Dnevni prostor</label>
                    <input type="number" name="humThresholdHighDS" id="humThresholdHighDS" step="1" min="0" max="100">
                    <div class="description">Visoka meja vlage za večje povečanje cikla v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="humExtremeHighDS">Ekstremno visoka vlaga Dnevni prostor</label>
                    <input type="number" name="humExtremeHighDS" id="humExtremeHighDS" step="1" min="0" max="100">
                    <div class="description">Ekstremno visoka zunanja vlaga za zmanjšanje cikla (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="co2ThresholdLowDS">Nizka meja CO2 Dnevni prostor</label>
                    <input type="number" name="co2ThresholdLowDS" id="co2ThresholdLowDS" step="1" min="400" max="2000">
                    <div class="description">Nizka meja CO2 za povečanje cikla v Dnevnem prostoru (400–2000 ppm)</div>
                </div>

                <div class="form-group">
                    <label for="co2ThresholdHighDS">Visoka meja CO2 Dnevni prostor</label>
                    <input type="number" name="co2ThresholdHighDS" id="co2ThresholdHighDS" step="1" min="400" max="2000">
                    <div class="description">Visoka meja CO2 za večje povečanje cikla v Dnevnem prostoru (400–2000 ppm)</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentLowDS">Nizko povečanje Dnevni prostor</label>
                    <input type="number" name="incrementPercentLowDS" id="incrementPercentLowDS" step="1" min="0" max="100">
                    <div class="description">Nizko povečanje cikla ob mejah vlage/CO2 v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentHighDS">Visoko povečanje Dnevni prostor</label>
                    <input type="number" name="incrementPercentHighDS" id="incrementPercentHighDS" step="1" min="0" max="100">
                    <div class="description">Visoko povečanje cikla ob visokih mejah vlage/CO2 v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="incrementPercentTempDS">Povečanje za temperaturo Dnevni prostor</label>
                    <input type="number" name="incrementPercentTempDS" id="incrementPercentTempDS" step="1" min="0" max="100">
                    <div class="description">Povečanje cikla za hlajenje/ogrevanje v Dnevnem prostoru (0–100 %)</div>
                </div>

                <div class="form-group">
                    <label for="tempIdealDS">Idealna temperatura Dnevni prostor</label>
                    <input type="number" name="tempIdealDS" id="tempIdealDS" step="1" min="-20" max="40">
                    <div class="description">Idealna temperatura v Dnevnem prostoru (-20–40 °C)</div>
                </div>

                <div class="form-group">
                    <label for="tempExtremeHighDS">Ekstremno visoka temperatura Dnevni prostor</label>
                    <input type="number" name="tempExtremeHighDS" id="tempExtremeHighDS" step="1" min="-20" max="40">
                    <div class="description">Ekstremno visoka zunanja temperatura za zmanjšanje cikla (-20–40 °C)</div>
                </div>

                <div class="form-group">
                    <label for="tempExtremeLowDS">Ekstremno nizka temperatura Dnevni prostor</label>
                    <input type="number" name="tempExtremeLowDS" id="tempExtremeLowDS" step="1" min="-20" max="40">
                    <div class="description">Ekstremno nizka zunanja temperatura za zmanjšanje cikla (-20–40 °C)</div>
                </div>
            </form>
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

        function updateSettings() {
            fetch("/data")
                .then(response => response.json())
                .then(data => {
                    // Update time display
                    const timeDisplay = document.getElementById("timeDisplay");
                    timeDisplay.textContent = `Trenutni čas: ${data.CURRENT_TIME} | DND: ${data.IS_DND} | NND: ${data.IS_NND}`;

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
                })
                .catch(error => console.error('Napaka pri pridobivanju nastavitev:', error));
        }

        function saveSettings() {
            var formData = new FormData();
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

            // Clear previous messages and show loading state
            clearMessages();
            showLoadingState(true);

            fetch("/settings/update", { method: "POST", body: formData })
                .then(response => {
                    if (!response.ok) {
                        throw new Error("Napaka pri shranjevanju: " + response.statusText);
                    }
                    return response.text();
                })
                .then(data => {
                    showLoadingState(false);
                    if (data !== "Nastavitve shranjene!") {
                        showErrorMessage(data);
                        return;
                    }
                    showSuccessMessage(data);
                })
                .catch(error => {
                    showLoadingState(false);
                    showErrorMessage(error.message);
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

        function resetSettings() {
            updateSettings();
        }

        updateInterval = setInterval(updateSettings, 10000);
        updateSettings();

        // Ustavljanje updateInterval ob fokusiranju na inpute in select elemente
        const formElements = document.querySelectorAll('#settingsForm input, #settingsForm select');
        formElements.forEach(element => {
            element.addEventListener('focus', () => clearInterval(updateInterval));
            element.addEventListener('blur', () => updateInterval = setInterval(updateSettings, 10000));
        });
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
  LOG_DEBUG("Web", "Zahtevek: GET /");

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
        }
        h1 {
            text-align: center;
            font-size: 28px;
            color: #e0e0e0;
            margin-bottom: 30px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            max-width: 1200px;
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
        .nav-container {
            text-align: center;
            margin-top: 30px;
        }
        .nav-button {
            display: inline-block;
            padding: 12px 24px;
            margin: 10px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-size: 16px;
            font-weight: bold;
            border: none;
            cursor: pointer;
        }
        .nav-button:hover {
            background-color: #6bb3ff;
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
    <h1>CEE - Ventilacijski sistem</h1>
    <div id="timeDisplay" class="time-display">Trenutni čas: )rawliteral" + myTZ.dateTime() + R"rawliteral( | DND: )rawliteral" + (isDNDTime() ? "DA" : "NE") + R"rawliteral( | NND: )rawliteral" + (isNNDTime() ? "DA" : "NE") + R"rawliteral(</div>

    <div class="nav-container">
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/help" class="nav-button">Pomoč</a>
    </div>

    <div class="grid">
        <!-- WC Card -->
        <div class="card">
            <h2>WC</h2>
            <div class="status-item">
                <span class="status-label">Temperatura:</span>
                <span class="status-value" id="wc-temp">N/A</span>
            </div>
            <div class="status-item">
                <span class="status-label">Vlaga:</span>
                <span class="status-value" id="wc-humidity">N/A</span>
            </div>
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
                <span class="status-value" id="ds-window1">)rawliteral" + (currentData.windowSensor1 ? "Odprto" : "Zaprto") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Balkonska vrata:</span>
                <span class="status-value" id="ds-window2">)rawliteral" + (currentData.windowSensor2 ? "Odprto" : "Zaprto") + R"rawliteral(</span>
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

        <!-- Network Card -->
        <div class="card">
            <h2>Omrežje</h2>
            <div class="status-item">
                <span class="status-label">NTP sinhronizacija:</span>
                <span class="status-value" id="net-ntp">)rawliteral" + (timeSynced ? "DA" : "NE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">Zunanji podatki:</span>
                <span class="status-value" id="net-external">)rawliteral" + (externalDataValid ? "VELJAVNI" : "NEVELJAVNI") + R"rawliteral(</span>
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

        <!-- Devices Card -->
        <div class="card">
            <h2>Naprave</h2>
            <div class="status-item">
                <span class="status-label">REW:</span>
                <span class="status-value" id="dev-rew">)rawliteral" + (rewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">UT_DEW:</span>
                <span class="status-value" id="dev-ut-dew">)rawliteral" + (utDewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
            </div>
            <div class="status-item">
                <span class="status-label">KOP_DEW:</span>
                <span class="status-value" id="dev-kop-dew">)rawliteral" + (kopDewStatus.isOnline ? "ONLINE" : "OFFLINE") + R"rawliteral(</span>
            </div>
        </div>

        <!-- Errors Card -->
        <div class="card">
            <h2>Napake</h2>
            <div id="errors-content" class="status-value">)rawliteral" + getErrorDescription(currentData.errorFlags) + R"rawliteral(</div>
        </div>
    </div>

    <div class="nav-container">
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/help" class="nav-button">Pomoč</a>
    </div>

    <script>
        function updatePage() {
            fetch('/current-data')
                .then(response => response.json())
                .then(data => {
                    // Update time display
                    const timeDisplay = document.getElementById("timeDisplay");
                    timeDisplay.textContent = `Trenutni čas: ${data.current_time} | DND: ${data.is_dnd ? "DA" : "NE"} | NND: ${data.is_nnd ? "DA" : "NE"}`;

                    // Update WC data
                    document.getElementById("wc-pressure").textContent = data.bathroom_pressure + " hPa";
                    document.getElementById("wc-light").textContent = data.wc_light ? "ON" : "OFF";
                    document.getElementById("wc-fan").textContent = data.wc_disabled ? "DISABLED" : (data.wc_fan ? "ON" : "OFF");
                    document.getElementById("wc-remaining").textContent = data.wc_remaining + " s";

                    // Update KOP data
                    document.getElementById("kop-temp").textContent = data.bathroom_temp + " °C";
                    document.getElementById("kop-humidity").textContent = data.bathroom_humidity + " %";
                    document.getElementById("kop-light1").textContent = data.bathroom_light1 ? "ON" : "OFF";
                    document.getElementById("kop-light2").textContent = data.bathroom_light2 ? "ON" : "OFF";
                    document.getElementById("kop-fan").textContent = data.bathroom_disabled ? "DISABLED" : (data.bathroom_fan ? "ON" : "OFF");
                    document.getElementById("kop-remaining").textContent = data.bathroom_remaining + " s";

                    // Update UT data
                    document.getElementById("ut-temp").textContent = data.utility_temp + " °C";
                    document.getElementById("ut-humidity").textContent = data.utility_humidity + " %";
                    document.getElementById("ut-light").textContent = data.utility_light ? "ON" : "OFF";
                    document.getElementById("ut-switch").textContent = data.utility_switch ? "ON" : "OFF";
                    document.getElementById("ut-fan").textContent = data.utility_disabled ? "DISABLED" : (data.utility_fan ? "ON" : "OFF");
                    document.getElementById("ut-remaining").textContent = data.utility_remaining + " s";

                    // Update DS data
                    document.getElementById("ds-temp").textContent = data.living_temp + " °C";
                    document.getElementById("ds-humidity").textContent = data.living_humidity + " %";
                    document.getElementById("ds-co2").textContent = data.living_co2 + " ppm";
                    document.getElementById("ds-window1").textContent = data.living_window1 ? "Odprto" : "Zaprto";
                    document.getElementById("ds-window2").textContent = data.living_window2 ? "Odprto" : "Zaprto";
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

                    // Update network data
                    document.getElementById("net-ntp").textContent = data.time_synced ? "DA" : "NE";
                    document.getElementById("net-external").textContent = data.external_data_valid ? "VELJAVNI" : "NEVELJAVNI";

                    // Update system data
                    document.getElementById("sys-ram").textContent = data.ram_percent + " %";
                    document.getElementById("sys-uptime").textContent = data.uptime;
                    document.getElementById("sys-log").textContent = data.log_buffer_size + " B";

                    // Update devices data
                    document.getElementById("dev-rew").textContent = data.rew_online ? "ONLINE" : "OFFLINE";
                    document.getElementById("dev-ut-dew").textContent = data.ut_dew_online ? "ONLINE" : "OFFLINE";
                    document.getElementById("dev-kop-dew").textContent = data.kop_dew_online ? "ONLINE" : "OFFLINE";

                    // Update errors
                    const errorDesc = getErrorDescription(data.error_flags);
                    document.getElementById("errors-content").innerHTML = errorDesc;
                })
                .catch(error => console.error('Napaka pri osveževanju:', error));
        }

        function getErrorDescription(errorFlags) {
            let errors = "";
            if (errorFlags & 1) errors += "Senzor BME280 nedelujoč<br>";
            if (errorFlags & 2) errors += "Senzor SHT41 nedelujoč<br>";
            if (errorFlags & 4) errors += "LittleFS napaka<br>";
            if (errorFlags & 32) errors += "HTTP napaka<br>";
            if (errorFlags & 64) errors += "Napajalna napaka<br>";
            if (errors === "") errors = "Brez napak";
            return errors;
        }

        setInterval(updatePage, 10000);
        updatePage(); // Initial update
    </script>
</body>
</html>)rawliteral";

  request->send(200, "text/html", html);
}

// Handle help page
void handleHelp(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /help");

  String html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>Pomoč - CEE Ventilacijski sistem</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            font-size: 16px;
            background: #101010;
            color: #e0e0e0;
        }
        h1 {
            text-align: center;
            font-size: 24px;
            color: #e0e0e0;
            margin-bottom: 30px;
        }
        .help-container {
            max-width: 800px;
            margin: 0 auto;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 20px;
        }
        .nav-link {
            display: inline-block;
            margin-bottom: 20px;
            padding: 10px 20px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-weight: bold;
        }
        .nav-link:hover {
            background-color: #6bb3ff;
        }
        h2 {
            color: #4da6ff;
            margin-top: 30px;
            margin-bottom: 10px;
        }
        p {
            margin-bottom: 15px;
            line-height: 1.4;
        }
        ul {
            margin-bottom: 20px;
            padding-left: 20px;
        }
        li {
            margin-bottom: 8px;
        }
    </style>
</head>
<body>
    <h1>Pomoč - CEE Ventilacijski sistem</h1>

    <div class="help-container">
        <a href="/" class="nav-link">← Nazaj na začetno stran</a>

        <h2>Sistemski status</h2>
        <p>Na začetni strani vidite trenutne vrednosti senzorjev iz različnih prostorov sistema:</p>
        <ul>
            <li><strong>EXT:</strong> Zunanji senzorji (temperatura, vlaga, tlak, svetloba)</li>
            <li><strong>DS:</strong> Dnevni prostor (temperatura, vlaga, CO2)</li>
            <li><strong>UT:</strong> Utility (temperatura, vlaga)</li>
            <li><strong>KOP:</strong> Kopalnica (temperatura, vlaga)</li>
            <li><strong>WC:</strong> WC (tlak)</li>
            <li><strong>TIME_WIFI:</strong> Poraba energije in čas zadnje posodobitve</li>
        </ul>

        <h2>Nastavitve</h2>
        <p>V razdelku nastavitve lahko prilagodite parametre ventilacijskega sistema:</p>
        <ul>
            <li><strong>Meje vlage:</strong> Kdaj se ventilatorji vklopijo</li>
            <li><strong>Trajanje delovanja:</strong> Koliko časa ventilatorji delujejo</li>
            <li><strong>Čas počitka:</strong> Koliko časa počakajo pred naslednjim ciklom</li>
            <li><strong>Temperaturne meje:</strong> Kako temperatura vpliva na ventilatorje</li>
            <li><strong>DND nastavitve:</strong> Ali ventilatorji delujejo med nočnim časom</li>
            <li><strong>Dnevni prostor:</strong> Parametri za avtomatsko ventilacijo dnevnega prostora</li>
        </ul>

        <h2>Shranjevanje nastavitev</h2>
        <p>Po spremembi nastavitev kliknite "Shrani" za uveljavitev sprememb. Sistem bo prikazal potrditev uspešnega shranjevanja.</p>

        <h2>Tehnična podpora</h2>
        <p>Za dodatno pomoč ali težave s sistemom preverite log datoteke ali se obrnite na administratorja sistema.</p>
    </div>
</body>
</html>)rawliteral";

  request->send(200, "text/html", html);
}

// Handle settings page
void handleSettings(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /settings");

  // Send the HTML directly
  request->send(200, "text/html", HTML_SETTINGS);
}

void handleDataRequest(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /data");

  // Breme direktno iz NVS
  Preferences prefs;
  prefs.begin("vent", true);

  String json = "{" +
                String("\"CURRENT_TIME\":\"") + String(myTZ.dateTime().c_str()) + "\"," +
                String("\"IS_DND\":") + String(isDNDTime() ? "1" : "0") + "," +
                String("\"IS_NND\":") + String(isNNDTime() ? "1" : "0") + "," +
                String("\"HUM_THRESHOLD\":\"") + String((int)prefs.getFloat("humThreshold", 60.0f)) + "\"," +
                String("\"FAN_DURATION\":\"") + String(prefs.getUInt("fanDuration", 180)) + "\"," +
                String("\"FAN_OFF_DURATION\":\"") + String(prefs.getUInt("fanOffDuration", 1200)) + "\"," +
                String("\"FAN_OFF_DURATION_KOP\":\"") + String(prefs.getUInt("fanOffDurationKop", 1200)) + "\"," +
                String("\"TEMP_LOW_THRESHOLD\":\"") + String((int)prefs.getFloat("tempLowThreshold", 5.0f)) + "\"," +
                String("\"TEMP_MIN_THRESHOLD\":\"") + String((int)prefs.getFloat("tempMinThreshold", -10.0f)) + "\"," +
                String("\"DND_ALLOW_AUTOMATIC\":") + String(prefs.getUChar("dndAllowableAutomatic", 0) != 0 ? "1" : "0") + "," +
                String("\"DND_ALLOW_SEMIAUTOMATIC\":") + String(prefs.getUChar("dndAllowableSemiautomatic", 0) != 0 ? "1" : "0") + "," +
                String("\"DND_ALLOW_MANUAL\":") + String(prefs.getUChar("dndAllowableManual", 1) != 0 ? "1" : "0") + "," +
                String("\"CYCLE_DURATION_DS\":\"") + String(prefs.getUInt("cycleDurationDS", 60)) + "\"," +
                String("\"CYCLE_ACTIVE_PERCENT_DS\":\"") + String((int)prefs.getFloat("cycleActivePercentDS", 30.0f)) + "\"," +
                String("\"HUM_THRESHOLD_DS\":\"") + String((int)prefs.getFloat("humThresholdDS", 60.0f)) + "\"," +
                String("\"HUM_THRESHOLD_HIGH_DS\":\"") + String((int)prefs.getFloat("humThresholdHighDS", 70.0f)) + "\"," +
                String("\"HUM_EXTREME_HIGH_DS\":\"") + String((int)prefs.getFloat("humExtremeHighDS", 80.0f)) + "\"," +
                String("\"CO2_THRESHOLD_LOW_DS\":\"") + String(prefs.getUInt("co2ThresholdLowDS", 900)) + "\"," +
                String("\"CO2_THRESHOLD_HIGH_DS\":\"") + String(prefs.getUInt("co2ThresholdHighDS", 1200)) + "\"," +
                String("\"INCREMENT_PERCENT_LOW_DS\":\"") + String((int)prefs.getFloat("incrementPercentLowDS", 15.0f)) + "\"," +
                String("\"INCREMENT_PERCENT_HIGH_DS\":\"") + String((int)prefs.getFloat("incrementPercentHighDS", 50.0f)) + "\"," +
                String("\"INCREMENT_PERCENT_TEMP_DS\":\"") + String((int)prefs.getFloat("incrementPercentTempDS", 20.0f)) + "\"," +
                String("\"TEMP_IDEAL_DS\":\"") + String((int)prefs.getFloat("tempIdealDS", 24.0f)) + "\"," +
                String("\"TEMP_EXTREME_HIGH_DS\":\"") + String((int)prefs.getFloat("tempExtremeHighDS", 30.0f)) + "\"," +
                String("\"TEMP_EXTREME_LOW_DS\":\"") + String((int)prefs.getFloat("tempExtremeLowDS", -10.0f)) + "\"}";

  prefs.end();
  request->send(200, "application/json", json);
}

void handleCurrentDataRequest(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /current-data");

  float ramPercent = (ESP.getHeapSize() - ESP.getFreeHeap()) * 100.0 / ESP.getHeapSize();
  String uptimeStr = formatUptime(millis() / 1000);

  String json = "{" +
                String("\"current_time\":\"") + String(myTZ.dateTime().c_str()) + "\"," +
                String("\"is_dnd\":") + String(isDNDTime() ? "true" : "false") + "," +
                String("\"is_nnd\":") + String(isNNDTime() ? "true" : "false") + "," +
                String("\"bathroom_temp\":") + String(currentData.bathroomTemp, 1) + "," +
                String("\"bathroom_humidity\":") + String(currentData.bathroomHumidity, 1) + "," +
                String("\"bathroom_pressure\":") + String((int)currentData.bathroomPressure) + "," +
                String("\"bathroom_light1\":") + String(currentData.bathroomLight1 ? "true" : "false") + "," +
                String("\"bathroom_light2\":") + String(currentData.bathroomLight2 ? "true" : "false") + "," +
                String("\"bathroom_fan\":") + String(currentData.bathroomFan ? "true" : "false") + "," +
                String("\"bathroom_disabled\":") + String(currentData.disableBathroom ? "true" : "false") + "," +
                String("\"bathroom_remaining\":") + String(getRemainingTime(0)) + "," +
                String("\"utility_temp\":") + String(currentData.utilityTemp, 1) + "," +
                String("\"utility_humidity\":") + String(currentData.utilityHumidity, 1) + "," +
                String("\"utility_light\":") + String(currentData.utilityLight ? "true" : "false") + "," +
                String("\"utility_switch\":") + String(currentData.utilitySwitch ? "true" : "false") + "," +
                String("\"utility_fan\":") + String(currentData.utilityFan ? "true" : "false") + "," +
                String("\"utility_disabled\":") + String(currentData.disableUtility ? "true" : "false") + "," +
                String("\"utility_remaining\":") + String(getRemainingTime(1)) + "," +
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
      !request->hasParam("tempExtremeHighDS", true) || !request->hasParam("tempExtremeLowDS", true)) {
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
  newSettings.incrementPercentLowDS = request->getParam("incrementPercentLowDS", true)->value().toFloat();
  newSettings.incrementPercentHighDS = request->getParam("incrementPercentHighDS", true)->value().toFloat();
  newSettings.incrementPercentTempDS = request->getParam("incrementPercentTempDS", true)->value().toFloat();
  newSettings.tempIdealDS = request->getParam("tempIdealDS", true)->value().toFloat();
  newSettings.tempExtremeHighDS = request->getParam("tempExtremeHighDS", true)->value().toFloat();
  newSettings.tempExtremeLowDS = request->getParam("tempExtremeLowDS", true)->value().toFloat();

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

  LOG_INFO("Web", "Nastavitve shranjene");

  settingsUpdateSuccess = true;
  settingsUpdateMessage = "Nastavitve shranjene!";
  settingsUpdatePending = false;

  request->send(200, "text/plain", "Nastavitve shranjene!");
}

void handleResetSettings(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: POST /settings/reset");
  initDefaults();
  saveSettings();

  LOG_INFO("Web", "Privzete nastavitve obnovljene");
  settingsUpdateSuccess = true;
  settingsUpdateMessage = "Privzete nastavitve obnovljene!";
  settingsUpdatePending = false;

  request->send(200, "text/plain", "Privzete nastavitve obnovljene!");
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
