#ifndef HTML_H
#define HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>CEE - Ventilacijski sistem</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 10px;
            font-size: 16px;
            background: #101010;
            color: #e0e0e0;
        }
        h1 {
            text-align: center;
            font-size: 20px;
            color: #e0e0e0;
        }
        .form-container {
            max-width: 500px;
            margin: auto;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            font-size: 14px;
        }
        th, td {
            padding: 5px;
            text-align: left;
            border-bottom: 1px solid #333;
        }
        th {
            background-color: #2a2a2a;
            color: #e0e0e0;
        }
        input[type=number], input[type=checkbox] {
            padding: 3px;
            font-size: 14px;
            background: #1a1a1a;
            color: #e0e0e0;
            border: 1px solid #555;
        }
        input[type=number] {
            width: 60px;
        }
        .error {
            color: #ff4444;
            text-align: center;
            font-size: 14px;
        }
        .success {
            color: #4da6ff;
            text-align: center;
            font-size: 14px;
        }
        .button-group {
            text-align: center;
            margin: 10px 0;
        }
        .submit-btn {
            padding: 5px 10px;
            margin-right: 5px;
            background-color: #4da6ff;
            color: #101010;
            border: none;
            cursor: pointer;
            font-size: 14px;
        }
        .reset-btn {
            background-color: #ff4444;
        }
        .tab {
            display: flex;
            justify-content: center;
            border-bottom: 1px solid #ccc;
            margin-bottom: 10px;
        }
        .tab button {
            background-color: #f1f1f1;
            border: none;
            outline: none;
            cursor: pointer;
            padding: 10px 16px;
            font-size: 14px;
            margin: 0 5px;
            background: #2a2a2a;
            color: #e0e0e0;
        }
        .tab button:hover {
            background-color: #4da6ff;
            color: #101010;
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
            margin-bottom: 10px;
        }
        h3 {
            font-size: 18px;
            margin-bottom: 5px;
            color: #4da6ff;
        }
        p {
            font-size: 14px;
            margin: 0;
            color: #e0e0e0;
        }
    </style>
</head>
<body>
    <h1>Ventilacijski sistem - Nastavitve</h1>
    <div id="timeDisplay" style="text-align: center; margin: 10px 0; font-size: 16px; color: #4da6ff;"></div>
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
                        <td><input type="checkbox" name="dndAllowAutomatic" id="dndAllowAutomatic"></td>
                    </tr>
                    <tr>
                        <td>DND dovoli polavtomatiko</td>
                        <td><input type="checkbox" name="dndAllowSemiautomatic" id="dndAllowSemiautomatic"></td>
                    </tr>
                    <tr>
                        <td>DND dovoli ročno upravljanje</td>
                        <td><input type="checkbox" name="dndAllowManual" id="dndAllowManual" checked></td>
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
                    document.getElementById("dndAllowAutomatic").checked = data.DND_ALLOW_AUTOMATIC;
                    document.getElementById("dndAllowSemiautomatic").checked = data.DND_ALLOW_SEMIAUTOMATIC;
                    document.getElementById("dndAllowManual").checked = data.DND_ALLOW_MANUAL;
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
            formData.append('dndAllowAutomatic', document.getElementById('dndAllowAutomatic').checked ? 'true' : 'false');
            formData.append('dndAllowSemiautomatic', document.getElementById('dndAllowSemiautomatic').checked ? 'true' : 'false');
            formData.append('dndAllowManual', document.getElementById('dndAllowManual').checked ? 'true' : 'false');
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

            document.getElementById("error-message").textContent = "";
            document.getElementById("success-message").textContent = "";
            fetch("/settings/update", { method: "POST", body: formData })
                .then(response => {
                    if (!response.ok) {
                        throw new Error("Napaka pri shranjevanju: " + response.statusText);
                    }
                    return response.text();
                })
                .then(data => {
                    if (data !== "Nastavitve shranjene!") {
                        document.getElementById("error-message").textContent = data;
                        return;
                    }
                    document.getElementById("success-message").textContent = data;
                })
                .catch(error => {
                    document.getElementById("error-message").textContent = error.message;
                    document.getElementById("success-message").textContent = "";
                });
        }

        function resetSettings() {
            updateSettings();
        }

        updateInterval = setInterval(updateSettings, 10000);
        updateSettings();

        // Ustavljanje updateInterval ob fokusiranju na inpute
        const inputs = document.querySelectorAll('#settingsForm input');
        inputs.forEach(input => {
            input.addEventListener('focus', () => clearInterval(updateInterval));
            input.addEventListener('blur', () => updateInterval = setInterval(updateSettings, 10000));
        });
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_H