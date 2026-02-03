#include "web.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "globals.h"
#include "logging.h"
#include "html.h"

AsyncWebServer server(80);

bool settingsUpdatePending = false;
unsigned long settingsUpdateStartTime = 0;
bool settingsUpdateSuccess = false;
String settingsUpdateMessage = "";

void handleRoot(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /");
  request->send_P(200, "text/html", index_html);
}

void handleDataRequest(AsyncWebServerRequest *request) {
  LOG_DEBUG("Web", "Zahtevek: GET /data");

  // Breme direktno iz NVS
  Preferences prefs;
  prefs.begin("vent", true);

  String json = "{" +
                String("\"HUM_THRESHOLD\":\"") + String((int)prefs.getFloat("humThreshold", 60.0f)) + "\"," +
                String("\"FAN_DURATION\":\"") + String(prefs.getUInt("fanDuration", 180)) + "\"," +
                String("\"FAN_OFF_DURATION\":\"") + String(prefs.getUInt("fanOffDuration", 1200)) + "\"," +
                String("\"FAN_OFF_DURATION_KOP\":\"") + String(prefs.getUInt("fanOffDuration", 1200)) + "\"," +
                String("\"TEMP_LOW_THRESHOLD\":\"") + String((int)prefs.getFloat("tempLowThreshold", 5.0f)) + "\"," +
                String("\"TEMP_MIN_THRESHOLD\":\"") + String((int)prefs.getFloat("tempMinThreshold", -10.0f)) + "\"," +
                String("\"DND_ALLOW_AUTOMATIC\":") + String(prefs.getBool("dndAllowableAutomatic", true) ? "true" : "false") + "," +
                String("\"DND_ALLOW_SEMIAUTOMATIC\":") + String(prefs.getBool("dndAllowableSemiautomatic", true) ? "true" : "false") + "," +
                String("\"DND_ALLOW_MANUAL\":") + String(prefs.getBool("dndAllowableManual", true) ? "true" : "false") + "," +
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
  newSettings.tempLowThreshold = request->getParam("tempLowThreshold", true)->value().toFloat();
  newSettings.tempMinThreshold = request->getParam("tempMinThreshold", true)->value().toFloat();
  newSettings.dndAllowableAutomatic = request->hasParam("dndAllowAutomatic", true);
  newSettings.dndAllowableSemiautomatic = request->hasParam("dndAllowSemiautomatic", true);
  newSettings.dndAllowableManual = request->hasParam("dndAllowManual", true);
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

  // Ponovno naloži iz NVS v RAM za zagotovitev sinhronizacije
  loadSettings();

  LOG_INFO("Web", "Nastavitve shranjene: humThreshold=%.1f, fanDuration=%d, tempLowThreshold=%.1f",
           newSettings.humThreshold, newSettings.fanDuration, newSettings.tempLowThreshold);

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
  LOG_INFO("Web", "Inicializacija strežnika");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleDataRequest);
  server.on("/settings/update", HTTP_POST, handlePostSettings);
  server.on("/settings/reset", HTTP_POST, handleResetSettings);
  server.on("/settings/status", HTTP_GET, handleSettingsStatus);

  server.begin();
  LOG_INFO("Web", "Strežnik zagnan na portu 80");
}