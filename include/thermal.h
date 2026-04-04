#pragma once

#include <Arduino.h>

struct ThermalROI {
    uint8_t x;  // column offset (0..31)
    uint8_t y;  // row offset    (0..23)
    uint8_t w;  // width  in columns
    uint8_t h;  // height in rows
};

// Initialization — call once during setup after Wire.begin()
bool thermal_init();

// Non-blocking update — call every loop iteration.
// Returns true when a new complete frame was merged.
bool thermal_update();

// Latest 768-element temperature array (°C). NULL if sensor absent.
const float* thermal_get_frame();

// Is the sensor present and operational?
bool thermal_is_available();

// Milliseconds since last successful frame merge
unsigned long thermal_get_frame_age_ms();

// Max temperature inside the active ROI
float thermal_get_roi_max();

// Max temperature across the entire frame
float thermal_get_frame_max();

// ROI management
ThermalROI thermal_get_roi();
bool thermal_set_roi(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
bool thermal_roi_is_default();
void thermal_load_roi_from_nvs();
void thermal_save_roi_to_nvs();
