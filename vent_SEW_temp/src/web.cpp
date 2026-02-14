// web.cpp

#include "web.h"
#include <LittleFS.h>

// Definiraj SensorData strukturo, saj je prvotno v senzor.cpp
struct SensorData {
    float temp;
    float humidity;
    float pressure;
    float voc;
    uint16_t cct;
    uint16_t lux;
    bool valid;
    unsigned long timestamp;
    uint8_t motion;
};

// Inicializacija spletnega strežnika
AsyncWebServer server(80);

// HTML predloga z vsemi spremembami
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>Senzorji</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        h1 { color: #333; text-align: center; }
        .data { margin: 10px 0; font-size: 20px; } /* Povečano za senzorje */
        .label { font-weight: bold; color: #555; }
        .value { margin-left: 10px; }
        table { width: 50%; border-collapse: collapse; margin-top: 20px; font-size: 14px; } /* Zmanjšano za tabelo */
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        #rgbCircle { width: 50px; height: 50px; border-radius: 50%; display: inline-block; vertical-align: middle; margin-left: 10px; }
        .rgbBar { width: 20px; display: inline-block; vertical-align: middle; margin-left: 5px; }
        #rBar { background-color: rgb(255, 0, 0); }
        #gBar { background-color: rgb(0, 255, 0); }
        #bBar { background-color: rgb(0, 0, 255); }
        #vocEmoji { font-size: 24px; margin-left: 10px; }
    </style>
</head>
<body>
    <h1>Trenutni podatki senzorjev</h1>
    <div class="data">
        <span class="label">Temperatura:</span>
        <span class="value" id="temp_sht"></span>°C (SHT)
        <span class="value" id="temp_bme"></span>°C (BME)
    </div>
    <div class="data">
        <span class="label">Vlaga:</span>
        <span class="value" id="hum_sht"></span> (SHT)
        <span class="value" id="hum_bme"></span> (BME)
    </div>
    <div class="data">
        <span class="label">Tlak:</span>
        <span class="value" id="pressure"></span> hPa
    </div>
    <div class="data">
        <span class="label">VOC:</span>
        <span class="value" id="voc"></span> kOhm
        <span id="vocEmoji"></span>
    </div>
    <div class="data">
        <span class="label">Lux:</span>
        <span class="value" id="lux"></span> lx
    </div>
    <div class="data">
        <span class="label">CCT:</span>
        <span class="value" id="cct"></span> K
    </div>
    <div class="data">
        <span class="label">RGB:</span>
        <span class="value" id="rgbValues">R = 0%, G = 0%, B = 0%</span>
        <div id="rgbCircle"></div>
        <div id="rBar" class="rgbBar"></div>
        <div id="gBar" class="rgbBar"></div>
        <div id="bBar" class="rgbBar"></div>
    </div>
    <h1>Zaznana gibanja (zadnjih 24 ur)</h1>
    <table id="motionTable">
        <tr><th>Ura</th><th>Gibanja</th></tr>
        <tr><td id="hour0">00:00-01:00</td><td id="h0"></td></tr>
        <tr><td id="hour1">01:00-02:00</td><td id="h1"></td></tr>
        <tr><td id="hour2">02:00-03:00</td><td id="h2"></td></tr>
        <tr><td id="hour3">03:00-04:00</td><td id="h3"></td></tr>
        <tr><td id="hour4">04:00-05:00</td><td id="h4"></td></tr>
        <tr><td id="hour5">05:00-06:00</td><td id="h5"></td></tr>
        <tr><td id="hour6">06:00-07:00</td><td id="h6"></td></tr>
        <tr><td id="hour7">07:00-08:00</td><td id="h7"></td></tr>
        <tr><td id="hour8">08:00-09:00</td><td id="h8"></td></tr>
        <tr><td id="hour9">09:00-10:00</td><td id="h9"></td></tr>
        <tr><td id="hour10">10:00-11:00</td><td id="h10"></td></tr>
        <tr><td id="hour11">11:00-12:00</td><td id="h11"></td></tr>
        <tr><td id="hour12">12:00-13:00</td><td id="h12"></td></tr>
        <tr><td id="hour13">13:00-14:00</td><td id="h13"></td></tr>
        <tr><td id="hour14">14:00-15:00</td><td id="h14"></td></tr>
        <tr><td id="hour15">15:00-16:00</td><td id="h15"></td></tr>
        <tr><td id="hour16">16:00-17:00</td><td id="h16"></td></tr>
        <tr><td id="hour17">17:00-18:00</td><td id="h17"></td></tr>
        <tr><td id="hour18">18:00-19:00</td><td id="h18"></td></tr>
        <tr><td id="hour19">19:00-20:00</td><td id="h19"></td></tr>
        <tr><td id="hour20">20:00-21:00</td><td id="h20"></td></tr>
        <tr><td id="hour21">21:00-22:00</td><td id="h21"></td></tr>
        <tr><td id="hour22">22:00-23:00</td><td id="h22"></td></tr>
        <tr><td id="hour23">23:00-00:00</td><td id="h23"></td></tr>
    </table>
    <script>
        function updateSensorData() {
            fetch("/data")
                .then(response => response.json())
                .then(data => {
                    document.getElementById("temp_sht").textContent = data.TEMP_SHT;
                    document.getElementById("temp_bme").textContent = data.TEMP_BME;
                    document.getElementById("hum_sht").textContent = data.HUM_SHT + "%";
                    document.getElementById("hum_bme").textContent = data.HUM_BME + "%";
                    document.getElementById("pressure").textContent = data.PRESSURE;
                    document.getElementById("voc").textContent = data.VOC;
                    let vocValue = parseFloat(data.VOC);
                    document.getElementById("vocEmoji").textContent = 
                        vocValue > 50 ? "😊" : (vocValue >= 30 ? "😐" : "☹️");
                    document.getElementById("lux").textContent = data.LUX;
                    document.getElementById("cct").textContent = data.CCT;
                    let rPercent = ((data.R / 65535) * 100).toFixed(1);
                    let gPercent = ((data.G / 65535) * 100).toFixed(1);
                    let bPercent = ((data.B / 65535) * 100).toFixed(1);
                    document.getElementById("rgbValues").textContent = 
                        `R = ${rPercent}%, G = ${gPercent}%, B = ${bPercent}%`;
                    let rScaled = Math.round((data.R / 65535) * 255);
                    let gScaled = Math.round((data.G / 65535) * 255);
                    let bScaled = Math.round((data.B / 65535) * 255);
                    document.getElementById("rgbCircle").style.backgroundColor = 
                        `rgb(${rScaled}, ${gScaled}, ${bScaled})`;
                    let maxHeight = 100; // Maksimalna višina stolpcev v pikah
                    let rHeight = (Math.log(data.R + 1) / Math.log(65535 + 1)) * maxHeight;
                    let gHeight = (Math.log(data.G + 1) / Math.log(65535 + 1)) * maxHeight;
                    let bHeight = (Math.log(data.B + 1) / Math.log(65535 + 1)) * maxHeight;
                    document.getElementById("rBar").style.height = rHeight + "px";
                    document.getElementById("gBar").style.height = gHeight + "px";
                    document.getElementById("bBar").style.height = bHeight + "px";
                })
                .catch(error => console.error('Error fetching sensor data:', error));
        }

        function updateMotionData() {
            fetch("/motion")
                .then(response => response.json())
                .then(data => {
                    for (let i = 0; i < 24; i++) {
                        document.getElementById("hour" + i).textContent = data.hours[i].hour;
                        document.getElementById("h" + i).textContent = 
                            data.hours[i].motions.length > 0 ? data.hours[i].motions.join(", ") : "Brez gibanja";
                    }
                })
                .catch(error => console.error('Error fetching motion data:', error));
        }

        setInterval(updateSensorData, 5000);  // Posodobi senzorje vsake 5 sekund
        setInterval(updateMotionData, 30000); // Posodobi tabelo gibanja vsake 30 sekund
        updateSensorData();  // Prvi klic ob nalaganju strani
        updateMotionData();  // Prvi klic za tabelo
    </script>
</body>
</html>
)rawliteral";

// API za posredovanje podatkov senzorjev
void handleDataRequest(AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"TEMP_SHT\":\"" + (sht41Present ? String(lastTempSHT, 1) : "N/A") + "\",";
    json += "\"TEMP_BME\":\"" + (bmePresent ? String(lastTempBME, 1) : "N/A") + "\",";
    json += "\"HUM_SHT\":\"" + (sht41Present ? String(lastHumSHT, 1) : "N/A") + "\",";
    json += "\"HUM_BME\":\"" + (bmePresent ? String(lastHumBME, 1) : "N/A") + "\",";
    json += "\"PRESSURE\":\"" + (bmePresent ? String(lastPressure, 1) : "N/A") + "\",";
    json += "\"VOC\":\"" + (bmePresent ? String(lastVOC, 1) : "N/A") + "\",";
    json += "\"LUX\":\"" + (tcsPresent ? String(lastLux) : "N/A") + "\",";
    json += "\"CCT\":\"" + (tcsPresent ? String(lastCCT) : "N/A") + "\",";
    json += "\"R\":\"" + (tcsPresent ? String(lastR) : "0") + "\",";
    json += "\"G\":\"" + (tcsPresent ? String(lastG) : "0") + "\",";
    json += "\"B\":\"" + (tcsPresent ? String(lastB) : "0") + "\"";
    json += "}";
    request->send(200, "application/json", json);
}

// API za posredovanje podatkov o gibanju z novim filtrom (5 minut)
void handleMotionRequest(AsyncWebServerRequest *request) {
    String json = "{\"hours\":[";
    fs::File file = LittleFS.open("/history.bin", FILE_READ);
    if (!file) {
        json += "]}";
        request->send(200, "application/json", json);
        return;
    }

    int totalEntries = file.size() / sizeof(SensorData);
    std::vector<SensorData> motionData;
    SensorData tempData;
    unsigned long lastMotionTime = 0;

    // Preberi in filtriraj gibanja z 5-minutnim oknom (300 sekund)
    while (file.available()) {
        file.read((uint8_t*)&tempData, sizeof(SensorData));
        if (tempData.valid && tempData.motion == 1) {
            if (lastMotionTime == 0 || (tempData.timestamp - lastMotionTime >= 300)) { // Spremenjeno na 5 minut
                motionData.push_back(tempData);
                lastMotionTime = tempData.timestamp;
            }
        }
    }
    file.close();

    // Razvrsti gibanja po urah z uporabo NTP časa
    unsigned long currentTime = getCurrentTime();
    unsigned long startTime = currentTime - 86400; // Zadnjih 24 ur
    String motionTimes[24];
    for (int i = 0; i < 24; i++) {
        motionTimes[i] = "";
    }

    for (const auto& data : motionData) {
        if (data.timestamp >= startTime && data.timestamp <= currentTime) {
            time_t ts = data.timestamp;
            struct tm* timeinfo = localtime(&ts);
            int hour = timeinfo->tm_hour;
            char timeStr[6];
            strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
            if (motionTimes[hour].length() > 0) {
                motionTimes[hour] += ", ";
            }
            motionTimes[hour] += String(timeStr);
        }
    }

    // Sestavi JSON z oznakami "danes", "včeraj" in "delno"
    time_t now = currentTime;
    struct tm* currentTimeInfo = localtime(&now);
    int currentHour = currentTimeInfo->tm_hour;

    for (int i = 0; i < 24; i++) {
        String hourRange = String(i < 10 ? "0" : "") + String(i) + ":00-" + 
                           String((i + 1) % 24 < 10 ? "0" : "") + String((i + 1) % 24) + ":00";
        String label;
        if (i == currentHour) {
            label = " (delno)"; // Trenutna ura
        } else if (i < currentHour || (currentHour == 0 && i == 23)) {
            label = " (danes)"; // Ure danes pred trenutno uro ali 23:00, če je polnoč
        } else {
            label = " (včeraj)"; // Ure od včeraj
        }
        hourRange += label;

        json += "{\"hour\":\"" + hourRange + "\",\"motions\":[";

        if (motionTimes[i].length() > 0) {
            String times = motionTimes[i];
            while (times.length() > 0) {
                int commaIndex = times.indexOf(", ");
                if (commaIndex == -1) {
                    json += "\"" + times + "\"";
                    break;
                } else {
                    json += "\"" + times.substring(0, commaIndex) + "\",";
                    times = times.substring(commaIndex + 2);
                }
            }
        }

        json += "]}";
        if (i < 23) json += ",";
    }
    json += "]}";

    request->send(200, "application/json", json);
}

// Inicializacija spletnega strežnika
void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });
    server.on("/data", HTTP_GET, handleDataRequest);
    server.on("/motion", HTTP_GET, handleMotionRequest);
    server.begin();
}