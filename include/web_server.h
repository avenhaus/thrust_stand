#pragma once

#include <Arduino.h>

// Wi-Fi mode reported to UI / serial
enum NetMode {
    NET_MODE_DISCONNECTED,
    NET_MODE_CONNECTING,
    NET_MODE_STATION,
    NET_MODE_AP_FALLBACK
};

struct WiFiStatus {
    NetMode    mode;
    String     ssid;
    String     ip;
    String     lastFailReason;
};

// Call from setup() — starts Wi-Fi + web server on Core 0
void web_init();

// Call from loop() — pushes telemetry/thermal via WebSocket (non-blocking)
void web_loop();

// Status accessors
WiFiStatus web_get_wifi_status();

// Manual web throttle — checked by main loop for timeout fallback
bool   web_throttle_is_active();
float  web_throttle_get_value();
void   web_throttle_clear();

// Credential management (also callable from serial command 'w')
void web_wifi_clear_credentials();

// Test configuration persistence
bool web_load_test_config(test_config_t* cfg);
bool web_save_test_config(const test_config_t* cfg);
void web_reset_test_config();
