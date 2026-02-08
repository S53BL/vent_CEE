#include "web.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "globals.h"
#include "logging.h"
#include "system.h"
#include "message_fields.h"

AsyncWebServer server(80);

// HTTP endpoint handlers
void handleManualControl(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    LOG_DEBUG("HTTP", "handleManualControl called - index=%d, len=%d, total=%d", index, len, total);
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    if (index + len == total) {
        LOG_INFO("HTTP", "Received MANUAL_CONTROL: %s", body.c_str());

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
                LOG_INFO("HTTP", "Manual trigger WC activated");
            } else if (room == "ut") {
                currentData.manualTriggerUtility = true;
                LOG_INFO("HTTP", "Manual trigger Utility activated");
            } else if (room == "kop") {
                currentData.manualTriggerBathroom = true;
                LOG_INFO("HTTP", "Manual trigger Bathroom activated");
            } else if (room == "ds") {
                currentData.manualTriggerLivingRoom = true;
                LOG_INFO("HTTP", "Manual trigger Living Room activated");
            } else {
                LOG_ERROR("HTTP", "Unknown room: %s", room.c_str());
                request->send(400, "application/json", "{\"status\":\"ERROR\",\"message\":\"Unknown room\"}");
                return;
            }
        } else if (action == "toggle") {
            if (room == "wc") {
                currentData.disableWc = !currentData.disableWc;
                LOG_INFO("HTTP", "WC disable toggled to: %s", currentData.disableWc ? "1" : "0");
            } else if (room == "ut") {
                currentData.disableUtility = !currentData.disableUtility;
                LOG_INFO("HTTP", "Utility disable toggled to: %s", currentData.disableUtility ? "1" : "0");
            } else if (room == "kop") {
                currentData.disableBathroom = !currentData.disableBathroom;
                LOG_INFO("HTTP", "Bathroom disable toggled to: %s", currentData.disableBathroom ? "1" : "0");
            } else if (room == "ds") {
                currentData.disableLivingRoom = !currentData.disableLivingRoom;
                LOG_INFO("HTTP", "Living Room disable toggled to: %s", currentData.disableLivingRoom ? "1" : "0");
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
        LOG_INFO("HTTP", "Received SENSOR_DATA: %s", body.c_str());

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

        LOG_INFO("HTTP", "Updated external data - Temp: %.1f°C, Hum: %.1f%%, CO2: %d",
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
                <table>
                    <tr>
                        <th>Parameter</th>
                        <th>Vnos</th>
                    </tr>
                    <tr>
                        <td>Meja vlage Kopalnica/Utility (0–100 %)</td>
                        <td><input type="number" name="humThreshold" id="humThreshold" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Trajanje ventilatorjev (60–6000 s)</td>
                        <td><input type="number" name="fanDuration" id="fanDuration" step="1" min="60" max="6000"></td>
                    </tr>
                    <tr>
                        <td>Čakanje Utility/WC (60–6000 s)</td>
                        <td><input type="number" name="fanOffDuration" id="fanOffDuration" step="1" min="60" max="6000"></td>
                    </tr>
                    <tr>
                        <td>Čakanje Kopalnica (60–6000 s)</td>
                        <td><input type="number" name="fanOffDurationKop" id="fanOffDurationKop" step="1" min="60" max="6000"></td>
                    </tr>
                    <tr>
                        <td>Meja nizke zunanje temperature (-20–40 °C)</td>
                        <td><input type="number" name="tempLowThreshold" id="tempLowThreshold" step="1" min="-20" max="40"></td>
                    </tr>
                    <tr>
                        <td>Meja minimalne zunanje temperature (-20–40 °C)</td>
                        <td><input type="number" name="tempMinThreshold" id="tempMinThreshold" step="1" min="-20" max="40"></td>
                    </tr>
                    <tr>
                        <td>DND dovoli avtomatiko</td>
                        <td>
                            <select name="dndAllowAutomatic" id="dndAllowAutomatic">
                                <option value="0">0 (Izključeno)</option>
                                <option value="1">1 (Vključeno)</option>
                            </select>
                        </td>
                    </tr>
                    <tr>
                        <td>DND dovoli polavtomatiko</td>
                        <td>
                            <select name="dndAllowSemiautomatic" id="dndAllowSemiautomatic">
                                <option value="0">0 (Izključeno)</option>
                                <option value="1">1 (Vključeno)</option>
                            </select>
                        </td>
                    </tr>
                    <tr>
                        <td>DND dovoli ročno upravljanje</td>
                        <td>
                            <select name="dndAllowManual" id="dndAllowManual">
                                <option value="0">0 (Izključeno)</option>
                                <option value="1">1 (Vključeno)</option>
                            </select>
                        </td>
                    </tr>
                    <tr>
                        <td>Trajanje cikla Dnevni prostor (60–6000 s)</td>
                        <td><input type="number" name="cycleDurationDS" id="cycleDurationDS" step="1" min="60" max="6000"></td>
                    </tr>
                    <tr>
                        <td>Aktivni delež cikla Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="cycleActivePercentDS" id="cycleActivePercentDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Meja vlage Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="humThresholdDS" id="humThresholdDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Visoka meja vlage Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="humThresholdHighDS" id="humThresholdHighDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Ekstremno visoka vlaga Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="humExtremeHighDS" id="humExtremeHighDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Nizka meja CO2 Dnevni prostor (400–2000 ppm)</td>
                        <td><input type="number" name="co2ThresholdLowDS" id="co2ThresholdLowDS" step="1" min="400" max="2000"></td>
                    </tr>
                    <tr>
                        <td>Visoka meja CO2 Dnevni prostor (400–2000 ppm)</td>
                        <td><input type="number" name="co2ThresholdHighDS" id="co2ThresholdHighDS" step="1" min="400" max="2000"></td>
                    </tr>
                    <tr>
                        <td>Nizko povečanje Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="incrementPercentLowDS" id="incrementPercentLowDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Visoko povečanje Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="incrementPercentHighDS" id="incrementPercentHighDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Povečanje za temperaturo Dnevni prostor (0–100 %)</td>
                        <td><input type="number" name="incrementPercentTempDS" id="incrementPercentTempDS" step="1" min="0" max="100"></td>
                    </tr>
                    <tr>
                        <td>Idealna temperatura Dnevni prostor (-20–40 °C)</td>
                        <td><input type="number" name="tempIdealDS" id="tempIdealDS" step="1" min="-20" max="40"></td>
                    </tr>
                    <tr>
                        <td>Ekstremno visoka temperatura Dnevni prostor (-20–40 °C)</td>
                        <td><input type="number" name="tempExtremeHighDS" id="tempExtremeHighDS" step="1" min="-20" max="40"></td>
                    </tr>
                    <tr>
                        <td>Ekstremno nizka temperatura Dnevni prostor (-20–40 °C)</td>
                        <td><input type="number" name="tempExtremeLowDS" id="tempExtremeLowDS" step="1" min="-20" max="40"></td>
                    </tr>
                </table>
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

// Handle root page with navigation
void handleRoot(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /");

  // Create status content
  String statusContent = "<table>\n";
  statusContent += "<tr><th>Senzor</th><th>Vrednosti</th></tr>\n";
  statusContent += "<tr><td>EXT</td><td>Temp=" + String(currentData.externalTemp, 1) + "°C, Hum=" + String(currentData.externalHumidity, 1) + "%</td></tr>\n";
  statusContent += "<tr><td>DS</td><td>Temp=" + String(currentData.livingTemp, 1) + "°C, Hum=" + String(currentData.livingHumidity, 1) + "%, CO2=" + String((int)currentData.livingCO2) + " ppm</td></tr>\n";
  statusContent += "<tr><td>UT</td><td>Temp=" + String(currentData.utilityTemp, 1) + "°C, Hum=" + String(currentData.utilityHumidity, 1) + "%</td></tr>\n";
  statusContent += "<tr><td>KOP</td><td>Temp=" + String(currentData.bathroomTemp, 1) + "°C, Hum=" + String(currentData.bathroomHumidity, 1) + "%</td></tr>\n";
  statusContent += "<tr><td>WC</td><td>Pres=" + String((int)currentData.bathroomPressure) + " hPa</td></tr>\n";
  statusContent += "<tr><td>5 V supply</td><td>" + String(currentData.supply5V, 3) + " V</td></tr>\n";
  statusContent += "<tr><td>3.3 V supply</td><td>" + String(currentData.supply3V3, 3) + " V</td></tr>\n";
  statusContent += "<tr><td>Power error</td><td>" + String((currentData.errorFlags & ERR_POWER) ? "Yes" : "No") + "</td></tr>\n";
  statusContent += "<tr><td>TIME_WIFI</td><td>Power=" + String(currentData.currentPower, 1) + " W, Energy=" + String(currentData.energyConsumption, 1) + " Wh</td></tr>\n";
  statusContent += "<tr><td>Zadnja posodobitev</td><td>" + myTZ.dateTime("H:i:s d.m.y") + "</td></tr>\n";
  statusContent += "</table>\n";

  // Basic HTML with navigation
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
        .status-container {
            max-width: 600px;
            margin: 0 auto 30px auto;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 20px;
        }
        .status-content {
            font-family: monospace;
            font-size: 14px;
            line-height: 1.4;
            white-space: pre-line;
        }
        .nav-container {
            max-width: 600px;
            margin: 0 auto;
            text-align: center;
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
    </style>
</head>
<body>
    <h1>CEE - Ventilacijski sistem</h1>

    <div class="status-container">
        <h2>Sistemski status</h2>
        <div class="status-content">)rawliteral" + statusContent + R"rawliteral(</div>
    </div>

    <div class="nav-container">
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/help" class="nav-button">Pomoč</a>
    </div>
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
                String("\"DND_ALLOW_AUTOMATIC\":") + String(prefs.getUChar("dndAllowableAutomatic", 1) != 0 ? "1" : "0") + "," +
                String("\"DND_ALLOW_SEMIAUTOMATIC\":") + String(prefs.getUChar("dndAllowableSemiautomatic", 1) != 0 ? "1" : "0") + "," +
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
    LOG_DEBUG("Web", "Zahtevek: GET /api/ping");
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
