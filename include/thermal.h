#pragma once

#include <Arduino.h>

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

// Max temperature across the entire frame
float thermal_get_frame_max();

// Ambient temperature from the sensor (°C)
float thermal_get_frame_ambient();
