#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "config.h"
#include "web_server.h"
#include "thermal.h"
#include "sensors.h"
#include "motor.h"

// ---------------------------------------------------------------------------
//  Forward declarations — command functions defined in main.cpp
// ---------------------------------------------------------------------------
extern int          current_step;
extern String       abort_reason;
extern test_config_t test_config;
extern test_data_t  test_data[];
extern float        rpm;
extern MotorESC     motor;

bool cmd_start_test();
bool cmd_abort_test();
void cmd_stop_motor();
void cmd_set_throttle(float pct);
void cmd_tare();

// ---------------------------------------------------------------------------
//  Private state
// ---------------------------------------------------------------------------
static AsyncWebServer  _server(80);
static AsyncWebSocket  _ws("/ws");
static Preferences     _prefs;

static NetMode       _wifiMode      = NET_MODE_DISCONNECTED;
static String        _wifiSSID      = "";
static String        _wifiIP        = "";
static String        _wifiFailReason = "";

static bool          _webThrottleActive = false;
static float         _webThrottleValue  = 0.0f;
static unsigned long _webThrottleLastHB = 0;

static unsigned long _lastTelemetryMs = 0;
static unsigned long _lastThermalMs       = 0;
static unsigned long _lastThermalUpdateMs = 0;

// ---------------------------------------------------------------------------
//  Wi-Fi credential helpers
// ---------------------------------------------------------------------------
static String _loadSSID() {
    _prefs.begin("thrust_stand", true);
    String s = _prefs.getString("wifi_ssid", "");
    _prefs.end();
    return s;
}

static String _loadPass() {
    _prefs.begin("thrust_stand", true);
    String p = _prefs.getString("wifi_pass", "");
    _prefs.end();
    return p;
}

static void _saveCredentials(const String& ssid, const String& pass) {
    _prefs.begin("thrust_stand", false);
    _prefs.putString("wifi_ssid", ssid);
    _prefs.putString("wifi_pass", pass);
    _prefs.end();
}

void web_wifi_clear_credentials() {
    _prefs.begin("thrust_stand", false);
    _prefs.remove("wifi_ssid");
    _prefs.remove("wifi_pass");
    _prefs.end();
    DEBUG_println(FST("# Wi-Fi credentials cleared from NVS."));
}

// ---------------------------------------------------------------------------
//  AP fallback
// ---------------------------------------------------------------------------
static void _startAP() {
    // Build SSID from prefix + chip ID suffix
    uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
    char apSSID[32];
    snprintf(apSSID, sizeof(apSSID), "%s_%04X", WIFI_AP_PREFIX, chipId);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, WIFI_AP_DEFAULT_PASSWORD);
    _wifiMode = NET_MODE_AP_FALLBACK;
    _wifiSSID = apSSID;
    _wifiIP   = WiFi.softAPIP().toString();

    DEBUG_printf(FST("# Wi-Fi AP started: SSID=%s  IP=%s\n"), apSSID, _wifiIP.c_str());
}

// ---------------------------------------------------------------------------
//  Station connect (blocking, bounded)
// ---------------------------------------------------------------------------
static bool _tryStationConnect(const String& ssid, const String& pass, unsigned long timeoutMs) {
    _wifiMode = NET_MODE_CONNECTING;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    DEBUG_printf(FST("# Wi-Fi connecting to \"%s\" ...\n"), ssid.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _wifiMode = NET_MODE_STATION;
        _wifiSSID = ssid;
        _wifiIP   = WiFi.localIP().toString();
        _wifiFailReason = "";
        DEBUG_printf(FST("# Wi-Fi connected: SSID=%s  IP=%s\n"), ssid.c_str(), _wifiIP.c_str());
        return true;
    }

    _wifiFailReason = "Station connect timeout";
    WiFi.disconnect(true);
    return false;
}

// ---------------------------------------------------------------------------
//  WebSocket events
// ---------------------------------------------------------------------------
static void _onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        DEBUG_printf(FST("# WS client #%u connected\n"), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        DEBUG_printf(FST("# WS client #%u disconnected\n"), client->id());
        // If this was the throttle-controlling client, start timeout countdown
        // (main loop handles the actual timeout via web_throttle_is_active + age check)
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            // Parse incoming JSON message
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) return;

            const char* cmd = doc["cmd"];
            if (!cmd) return;

            if (strcmp(cmd, "heartbeat") == 0) {
                _webThrottleLastHB = millis();
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  REST API handlers
// ---------------------------------------------------------------------------

static void _handleStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    // Motor
    doc["throttle"]    = motor.getCurrentThrottle();
    const char* stateStr = "IDLE";
    switch (motor.getState()) {
        case MotorESC::STATE_RUNNING:       stateStr = "RUNNING";  break;
        case MotorESC::STATE_ACCELERATING:  stateStr = "ACCEL";    break;
        case MotorESC::STATE_DECELERATING:  stateStr = "DECEL";    break;
        default: break;
    }
    doc["motorState"]  = stateStr;

    // Test
    doc["testRunning"]  = (current_step >= 0);
    doc["currentStep"]  = current_step;
    doc["totalSteps"]   = test_config.total_steps;

    // Sensors
    doc["thrust"]   = lc_value_1;
    doc["torque"]   = lc_value_2;
    doc["voltage"]  = bus_voltage;
    doc["current"]  = current;
    doc["power"]    = power;
    doc["rpm"]      = rpm;

    // Thermal
    doc["thermalAvailable"] = thermal_is_available();
    doc["thermalMax"]       = thermal_get_frame_max();
    doc["thermalFrameAge"]  = thermal_get_frame_age_ms();
    doc["abortReason"]      = abort_reason;

    // Heap
    doc["freeHeap"] = ESP.getFreeHeap();

    // Wi-Fi
    switch (_wifiMode) {
        case NET_MODE_STATION:      doc["wifiMode"] = "STA";    break;
        case NET_MODE_AP_FALLBACK:  doc["wifiMode"] = "AP";     break;
        case NET_MODE_CONNECTING:   doc["wifiMode"] = "CONNECTING"; break;
        default:                    doc["wifiMode"] = "DISCONNECTED"; break;
    }
    doc["wifiSSID"] = _wifiSSID;
    doc["wifiIP"]   = _wifiIP;

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void _handleTestStart(AsyncWebServerRequest* req) {
    if (cmd_start_test()) {
        req->send(200, "application/json", "{\"ok\":true}");
    } else {
        req->send(409, "application/json", "{\"ok\":false,\"error\":\"Test already running or motor busy\"}");
    }
}

static void _handleTestAbort(AsyncWebServerRequest* req) {
    if (cmd_abort_test()) {
        req->send(200, "application/json", "{\"ok\":true}");
    } else {
        req->send(409, "application/json", "{\"ok\":false,\"error\":\"No test running\"}");
    }
}

static void _handleMotorStop(AsyncWebServerRequest* req) {
    cmd_stop_motor();
    _webThrottleActive = false;
    req->send(200, "application/json", "{\"ok\":true}");
}

static void _handleMotorThrottle(AsyncWebServerRequest* req) {
    if (!req->hasParam("value", true)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing 'value'\"}");
        return;
    }
    if (current_step >= 0) {
        req->send(409, "application/json", "{\"ok\":false,\"error\":\"Test running — cannot set manual throttle\"}");
        return;
    }
    float val = req->getParam("value", true)->value().toFloat();
    val = constrain(val, 0.0f, 100.0f);
    _webThrottleActive = true;
    _webThrottleValue  = val;
    _webThrottleLastHB = millis();
    cmd_set_throttle(val);
    req->send(200, "application/json", "{\"ok\":true}");
}

static void _handleTare(AsyncWebServerRequest* req) {
    cmd_tare();
    req->send(200, "application/json", "{\"ok\":true}");
}

static void _handleTestResults(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray steps = doc["steps"].to<JsonArray>();
    for (unsigned int i = 0; i <= test_config.total_steps; i++) {
        if (test_data[i].throttle == 0 && test_data[i].lc_samples == 0) continue;
        JsonObject s = steps.add<JsonObject>();
        s["step"]       = i;
        s["throttle"]   = test_data[i].throttle;
        s["thrust"]     = test_data[i].thrust;
        s["torque"]     = test_data[i].torque;
        s["voltage"]    = test_data[i].voltage;
        s["current"]    = test_data[i].current;
        s["power"]      = test_data[i].power;
        s["rpm"]        = test_data[i].rpm;
        s["thermalMax"] = test_data[i].thermal_max;
    s["thermalAmbient"] = test_data[i].thermal_ambient;
    s["thermalValid"] = test_data[i].thermal_valid;
    s["thermalAbort"] = test_data[i].thermal_abort;
    float eff = (test_data[i].power > 0) ? test_data[i].thrust / (test_data[i].power / 1000.0f) : 0.0f;
    s["efficiency"]  = eff;
    s["lcSamples"]   = test_data[i].lc_samples;
    s["sensorSamples"] = test_data[i].sensor_samples;
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void _handleTestCSV(AsyncWebServerRequest* req) {
    String csv = "Step,Throttle(%),Thrust(g),Torque(g·cm),Voltage(V),Current(A),Power(W),RPM,Thermal_Max(C),Efficiency(g/W),LC_Samples,Sensor_Samples\n";
    for (unsigned int i = 0; i <= test_config.total_steps; i++) {
        if (test_data[i].throttle == 0 && test_data[i].lc_samples == 0) continue;
        float eff = (test_data[i].power > 0) ? test_data[i].thrust / (test_data[i].power / 1000.0f) : 0.0f;
        char row[256];
        snprintf(row, sizeof(row),
            "%u,%.2f,%.2f,%.2f,%.2f,%.3f,%.2f,%.0f,%.2f,%.2f,%u,%u\n",
            i, test_data[i].throttle, test_data[i].thrust, test_data[i].torque,
            test_data[i].voltage, test_data[i].current, test_data[i].power,
            test_data[i].rpm, test_data[i].thermal_max,
            eff,
            test_data[i].lc_samples, test_data[i].sensor_samples);
        csv += row;
    }
    AsyncWebServerResponse* response = req->beginResponse(200, "text/csv", csv);
    response->addHeader("Content-Disposition", "attachment; filename=thrust_data.csv");
    req->send(response);
}

static void _handleWiFiStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;
    switch (_wifiMode) {
        case NET_MODE_STATION:      doc["mode"] = "STA";    break;
        case NET_MODE_AP_FALLBACK:  doc["mode"] = "AP";     break;
        case NET_MODE_CONNECTING:   doc["mode"] = "CONNECTING"; break;
        default:                    doc["mode"] = "DISCONNECTED"; break;
    }
    doc["ssid"]       = _wifiSSID;
    doc["ip"]         = _wifiIP;
    doc["failReason"] = _wifiFailReason;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void _handleWiFiCredentials(AsyncWebServerRequest* req) {
    if (!req->hasParam("ssid", true)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing 'ssid'\"}");
        return;
    }
    String ssid = req->getParam("ssid", true)->value();
    String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";

    if (ssid.length() == 0) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"SSID cannot be empty\"}");
        return;
    }
    if (pass.length() > 0 && pass.length() < 8) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Password must be empty (open) or at least 8 characters\"}");
        return;
    }

    _saveCredentials(ssid, pass);
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Credentials saved. Reconnecting...\"}");

    // Schedule reconnect attempt (non-blocking via flag checked in web_loop)
    // For simplicity, restart WiFi here after a brief delay
    delay(500);
    WiFi.disconnect(true);
    delay(100);
    if (!_tryStationConnect(ssid, pass, WIFI_STA_CONNECT_TIMEOUT_MS)) {
        _startAP();
    }
}

static void _handleWiFiClear(AsyncWebServerRequest* req) {
    web_wifi_clear_credentials();
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Credentials cleared. Reboot to enter AP mode.\"}");
}

// ---------------------------------------------------------------------------
//  Test Configuration Handlers
// ---------------------------------------------------------------------------

static const char* TEST_CFG_BLOB_KEY = "test_cfg";

static bool _isValidTestConfig(const test_config_t& cfg) {
    if (cfg.total_steps == 0 || cfg.total_steps > 100) return false;
    if (cfg.step_time_ms < 100) return false;
    if (cfg.step_accel_time_ms == 0) return false;
    if (cfg.decel_time_ms == 0) return false;
    if (cfg.min_throttle_percent < 0.0f || cfg.max_throttle_percent > 100.0f) return false;
    if (cfg.min_throttle_percent >= cfg.max_throttle_percent) return false;
    if (cfg.max_temp_limit_celsius <= 0.0f) return false;
    return true;
}

static bool _loadTestConfig(test_config_t* cfg) {
    if (!cfg) return false;

    *cfg = (test_config_t)TEST_CONFIG_DEFAULTS;
    _prefs.begin("thrust_stand", true);
    size_t len = _prefs.getBytesLength(TEST_CFG_BLOB_KEY);
    bool loaded = false;
    if (len == sizeof(test_config_t)) {
        loaded = (_prefs.getBytes(TEST_CFG_BLOB_KEY, (uint8_t*)cfg, sizeof(test_config_t)) == sizeof(test_config_t));
    }
    _prefs.end();

    if (!loaded) return true; // Defaults already applied

    if (!_isValidTestConfig(*cfg)) {
        DEBUG_println(FST("# WARNING: Stored test config invalid; reverting to defaults."));
        *cfg = (test_config_t)TEST_CONFIG_DEFAULTS;
    }
    return true;
}

static bool _saveTestConfig(const test_config_t* cfg) {
    if (!cfg) return false;
    if (!_isValidTestConfig(*cfg)) return false;

    _prefs.begin("thrust_stand", false);
    bool ok = (_prefs.putBytes(TEST_CFG_BLOB_KEY, (const uint8_t*)cfg, sizeof(test_config_t)) == sizeof(test_config_t));
    _prefs.end();

    if (!ok) {
        DEBUG_println(FST("# ERROR: Failed to persist test config blob to NVS."));
    }
    return ok;
}

static void _handleResetTestConfig(AsyncWebServerRequest* req) {
    test_config = (test_config_t)TEST_CONFIG_DEFAULTS;
    _saveTestConfig(&test_config);
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Test config reset to defaults.\"}");
}

static void _handleGetTestConfig(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["totalSteps"] = test_config.total_steps;
    doc["stepTimeMs"] = test_config.step_time_ms;
    doc["stepAccelTimeMs"] = test_config.step_accel_time_ms;
    doc["decelTimeMs"] = test_config.decel_time_ms;
    doc["minThrottlePercent"] = test_config.min_throttle_percent;
    doc["maxThrottlePercent"] = test_config.max_throttle_percent;
    doc["maxTempLimitCelsius"] = test_config.max_temp_limit_celsius;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void _handleSetTestConfig(AsyncWebServerRequest* req, JsonVariant& doc) {
    if (!doc.is<JsonObject>()) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Expected JSON object\"}");
        return;
    }
    JsonObject obj = doc.as<JsonObject>();
    if (!obj.containsKey("totalSteps") || !obj.containsKey("stepTimeMs") ||
        !obj.containsKey("stepAccelTimeMs") || !obj.containsKey("decelTimeMs") ||
        !obj.containsKey("minThrottlePercent") || !obj.containsKey("maxThrottlePercent") ||
        !obj.containsKey("maxTempLimitCelsius")) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing required test config fields\"}");
        return;
    }

    test_config_t updated = {
        obj["totalSteps"].as<unsigned int>(),
        obj["stepTimeMs"].as<unsigned long>(),
        obj["stepAccelTimeMs"].as<unsigned long>(),
        obj["decelTimeMs"].as<unsigned long>(),
        obj["minThrottlePercent"].as<float>(),
        obj["maxThrottlePercent"].as<float>(),
        obj["maxTempLimitCelsius"].as<float>()
    };

    if (updated.total_steps == 0 || updated.total_steps > 100 || updated.step_time_ms < 100 || updated.step_accel_time_ms == 0 ||
        updated.decel_time_ms == 0 || updated.min_throttle_percent < 0.0f || updated.max_throttle_percent > 100.0f ||
        updated.min_throttle_percent >= updated.max_throttle_percent || updated.max_temp_limit_celsius <= 0.0f) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid test configuration values\"}");
        return;
    }

    test_config = updated;
    if (_saveTestConfig(&test_config)) {
        req->send(200, "application/json", "{\"ok\":true,\"message\":\"Test configuration saved\"}");
    } else {
        req->send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to write configuration to NVS\"}");
    }
}

// ---------------------------------------------------------------------------
//  Calibration Handlers
// ---------------------------------------------------------------------------

static void _handleGetCalibration(AsyncWebServerRequest* req) {
    calibration_t* cal = get_calibration_ptr();
    
    JsonDocument doc;
    doc["lc_calibration_value_1"]   = cal->lc_calibration_value_1;
    doc["lc_calibration_value_2"]   = cal->lc_calibration_value_2;
    doc["shunt"]                    = cal->shunt;
    doc["current_LSB_mA"]           = cal->current_LSB_mA;
    doc["current_zero_offset_mA"]   = cal->current_zero_offset_mA;
    doc["INA266_max_current"]       = cal->INA266_max_current;
    doc["bus_V_scaling_e4"]         = cal->bus_V_scaling_e4;
    
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void _handleSetCalibrationJson(AsyncWebServerRequest* req, JsonVariant& doc) {
    if (!doc.is<JsonObject>()) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Expected JSON object\"}");
        return;
    }

    JsonObject obj = doc.as<JsonObject>();
    if (!obj.containsKey("lc_calibration_value_1") || !obj.containsKey("lc_calibration_value_2") ||
        !obj.containsKey("shunt") || !obj.containsKey("current_LSB_mA") ||
        !obj.containsKey("current_zero_offset_mA") || !obj.containsKey("INA266_max_current") ||
        !obj.containsKey("bus_V_scaling_e4")) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing required calibration fields\"}");
        return;
    }

    float lc1 = obj["lc_calibration_value_1"].as<float>();
    float lc2 = obj["lc_calibration_value_2"].as<float>();
    float shunt = obj["shunt"].as<float>();

    if (lc1 <= 0 || lc2 <= 0 || shunt <= 0) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"Calibration values must be positive\"}");
        return;
    }

    calibration_t cal;
    cal.lc_calibration_value_1 = lc1;
    cal.lc_calibration_value_2 = lc2;
    cal.shunt = shunt;
    cal.current_LSB_mA = obj["current_LSB_mA"].as<float>();
    cal.current_zero_offset_mA = obj["current_zero_offset_mA"].as<float>();
    cal.INA266_max_current = obj["INA266_max_current"].as<float>();
    cal.bus_V_scaling_e4 = obj["bus_V_scaling_e4"].as<uint16_t>();

    set_calibration(&cal);
    apply_calibration(&cal);

    DEBUG_println(FST("# Calibration updated from web UI."));
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Calibration updated and applied\"}");
}

static void _handleResetCalibration(AsyncWebServerRequest* req) {
    calibration_t cal = CALIBRATION_DEFAULTS;
    set_calibration(&cal);
    apply_calibration(&cal);

    DEBUG_println(FST("# Calibration reset to firmware defaults."));
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Calibration reset to defaults\"}");
}

static void _handleINA226Scan(AsyncWebServerRequest* req) {
    DEBUG_println(FST("# Starting INA226 calibration scan from web UI ..."));
    
    ina226_scan_result_t result = calibrate_ina226_scan();
    
    JsonDocument doc;
    doc["ok"] = true;
    doc["avg_bus_voltage"] = result.avg_bus_voltage;
    doc["avg_current_mA"] = result.avg_current_mA;
    doc["recommended_zero_offset"] = result.recommended_zero_offset;
    doc["message"] = "Scan complete. Use recommended_zero_offset as new current_zero_offset_mA value.";
    
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

// ---------------------------------------------------------------------------
//  Thermal frame endpoint (binary push over WebSocket)
// ---------------------------------------------------------------------------
static void _pushThermalFrame() {
    if (!thermal_is_available()) return;
    if (_ws.count() == 0) return;

    const float* frame = thermal_get_frame();
    if (!frame) return;

    // Send as binary: 4-byte header "THRM" + 768 × 2 bytes (temp × 100 as int16_t)
    const size_t headerLen = 4;
    const size_t payloadLen = THERMAL_PIXELS * 2;
    const size_t totalLen = headerLen + payloadLen;

    uint8_t* buf = (uint8_t*)malloc(totalLen);
    if (!buf) return;

    buf[0] = 'T'; buf[1] = 'H'; buf[2] = 'R'; buf[3] = 'M';
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        int16_t val = (int16_t)(frame[i] * 100.0f);
        buf[headerLen + i * 2]     = val & 0xFF;
        buf[headerLen + i * 2 + 1] = (val >> 8) & 0xFF;
    }

    _ws.binaryAll(buf, totalLen);
    free(buf);
}

// ---------------------------------------------------------------------------
//  Telemetry push (JSON over WebSocket)
// ---------------------------------------------------------------------------
static void _pushTelemetry() {
    if (_ws.count() == 0) return;

    JsonDocument doc;
    doc["type"]        = "telemetry";
    doc["throttle"]    = motor.getCurrentThrottle();

    const char* stateStr = "IDLE";
    switch (motor.getState()) {
        case MotorESC::STATE_RUNNING:       stateStr = "RUNNING";  break;
        case MotorESC::STATE_ACCELERATING:  stateStr = "ACCEL";    break;
        case MotorESC::STATE_DECELERATING:  stateStr = "DECEL";    break;
        default: break;
    }
    doc["motorState"]  = stateStr;
    doc["thrust"]      = lc_value_1;
    doc["torque"]      = lc_value_2;
    doc["voltage"]     = bus_voltage;
    doc["current"]     = current;
    doc["power"]       = power;
    doc["rpm"]         = rpm;
    doc["thermalMax"]     = thermal_get_frame_max();
    doc["thermalAmbient"] = thermal_get_frame_ambient();
    doc["thermalOk"]      = thermal_is_available() && thermal_get_frame_age_ms() < THERMAL_STALE_MS;

    doc["testRunning"] = (current_step >= 0);
    doc["currentStep"] = current_step;
    doc["totalSteps"]  = test_config.total_steps;
    doc["freeHeap"]    = ESP.getFreeHeap();
    doc["abortReason"] = abort_reason;

    String out;
    serializeJson(doc, out);
    _ws.textAll(out);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void web_init() {
    // Mount LittleFS for static assets
    if (!LittleFS.begin(true)) {
        DEBUG_println(FST("# LittleFS mount failed — web UI may not load."));
    } else {
        DEBUG_println(FST("# LittleFS mounted."));
    }

    // --- Wi-Fi ---
    String ssid = _loadSSID();
    if (ssid.length() == 0) {
        DEBUG_println(FST("# No Wi-Fi credentials stored — starting AP."));
        _startAP();
    } else {
        String pass = _loadPass();
        if (!_tryStationConnect(ssid, pass, WIFI_STA_CONNECT_TIMEOUT_MS)) {
            DEBUG_println(FST("# Station connect failed — starting AP fallback."));
            _startAP();
        }
    }

    // --- WebSocket ---
    _ws.onEvent(_onWsEvent);
    _server.addHandler(&_ws);

    // --- REST API ---
    _server.on("/api/status",          HTTP_GET,  _handleStatus);
    _server.on("/api/test/start",      HTTP_POST, _handleTestStart);
    _server.on("/api/test/abort",      HTTP_POST, _handleTestAbort);
    _server.on("/api/motor/stop",      HTTP_POST, _handleMotorStop);
    _server.on("/api/motor/throttle",  HTTP_POST, _handleMotorThrottle);
    _server.on("/api/sensors/tare",    HTTP_POST, _handleTare);
    _server.on("/api/test/results",    HTTP_GET,  _handleTestResults);
    _server.on("/api/test/csv",        HTTP_GET,  _handleTestCSV);
    _server.on("/api/test/config",     HTTP_GET,  _handleGetTestConfig);
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/test/config", _handleSetTestConfig));
    _server.on("/api/test/config/reset", HTTP_POST, _handleResetTestConfig);
    _server.on("/api/wifi/status",     HTTP_GET,  _handleWiFiStatus);
    _server.on("/api/wifi/credentials",HTTP_POST, _handleWiFiCredentials);
    _server.on("/api/wifi/clear",      HTTP_POST, _handleWiFiClear);
    
    // --- Calibration API ---
    _server.on("/api/calibration",           HTTP_GET,  _handleGetCalibration);
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/calibration", _handleSetCalibrationJson));
    _server.on("/api/calibration/reset",     HTTP_POST, _handleResetCalibration);
    _server.on("/api/calibration/ina226-scan", HTTP_POST, _handleINA226Scan);

    // --- Static files from LittleFS ---
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    _server.begin();
    DEBUG_println(FST("# Web server started."));
}

void web_loop() {
    _ws.cleanupClients();

    unsigned long now = millis();

    // Push telemetry at configured rate
    if (now - _lastTelemetryMs >= WEB_TELEMETRY_INTERVAL_MS) {
        _lastTelemetryMs = now;
        _pushTelemetry();
    }

    // Update thermal readings at a lower frequency to avoid blocking the sensor loop.
    unsigned long thermalUpdateInterval = THERMAL_UPDATE_INTERVAL_MS;
    if (ESP.getFreeHeap() < HEAP_SAFETY_MARGIN_BYTES) {
        thermalUpdateInterval *= 4;  // reduce thermal acquisition under memory pressure
    }
    if (now - _lastThermalUpdateMs >= thermalUpdateInterval) {
        _lastThermalUpdateMs = now;
        thermal_update();
    }

    // Push thermal frame at configured rate (throttle if heap is low)
    unsigned long thermalInterval = WEB_THERMAL_INTERVAL_MS;
    if (ESP.getFreeHeap() < HEAP_SAFETY_MARGIN_BYTES) {
        thermalInterval *= 4;  // reduce to ~0.5 Hz under memory pressure
    }
    if (now - _lastThermalMs >= thermalInterval) {
        _lastThermalMs = now;
        _pushThermalFrame();
    }

    // Runtime station link monitoring
    if (_wifiMode == NET_MODE_STATION && WiFi.status() != WL_CONNECTED) {
        DEBUG_println(FST("# Wi-Fi station link lost — attempting reconnect..."));
        _wifiMode = NET_MODE_CONNECTING;
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_STA_RECONNECT_TIMEOUT_MS) {
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED) {
            _wifiMode = NET_MODE_STATION;
            _wifiIP = WiFi.localIP().toString();
            DEBUG_println(FST("# Wi-Fi reconnected."));
        } else {
            _wifiFailReason = "Station link lost, reconnect timeout";
            DEBUG_println(FST("# Reconnect failed — latching to AP fallback."));
            WiFi.disconnect(true);
            _startAP();
        }
    }
}

WiFiStatus web_get_wifi_status() {
    return { _wifiMode, _wifiSSID, _wifiIP, _wifiFailReason };
}

bool web_throttle_is_active() {
    if (!_webThrottleActive) return false;
    // Check heartbeat timeout
    if (millis() - _webThrottleLastHB > WEB_THROTTLE_TIMEOUT_MS) {
        return false;  // timed out — main loop should ramp down
    }
    return true;
}

float web_throttle_get_value() {
    return _webThrottleValue;
}

void web_throttle_clear() {
    _webThrottleActive = false;
    _webThrottleValue  = 0.0f;
}

bool web_load_test_config(test_config_t* cfg) {
    return _loadTestConfig(cfg);
}

bool web_save_test_config(const test_config_t* cfg) {
    if (!cfg) return false;
    test_config = *cfg;
    return _saveTestConfig(cfg);
}

void web_reset_test_config() {
    test_config = (test_config_t)TEST_CONFIG_DEFAULTS;
    _saveTestConfig(&test_config);
}
