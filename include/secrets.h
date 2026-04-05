#pragma once

/********************************************\
|*  WiFi AP Default Credentials
|*  Edit these values for your network setup
\********************************************/

// AP SSID prefix (chip ID will be appended, e.g. ThrustStand_XXXX)
#define WIFI_AP_PREFIX "ThrustStand"

// AP password when no credentials are stored (minimum 8 characters)
#define WIFI_AP_DEFAULT_PASSWORD "thruststand"

// Example station credentials (optional, for testing)
// Note: These are for reference only. Credentials are normally configured via the web UI
// and persisted to NVS (ESP32 Preferences). Uncomment and set to automatically provision on first boot.
// #define WIFI_STA_EXAMPLE_SSID     "YourNetworkSSID"
// #define WIFI_STA_EXAMPLE_PASSWORD "YourNetworkPassword"
