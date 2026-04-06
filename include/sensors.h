#pragma once

#include "thermal.h"
#include "config.h"

typedef struct {
    float throttle;          // Throttle percentage (0.0 to 100.0)
    float thrust;            // Thrust value from load cell 1
    float torque;            // Torque value from load cell 2
    float voltage;           // Bus voltage from INA226
    float current;           // Current from INA226
    float power;             // Power from INA226
    float rpm;               // Current RPM value    
    float thermal_max;       // Max temperature from thermal sensor (°C)
    bool  thermal_valid;     // True if thermal data was from a fresh frame
    unsigned int lc_samples; // Number of load cell samples
    unsigned int sensor_samples; // Number of other sensor samples
} test_data_t;

// INA226 scan result structure
typedef struct {
    float avg_bus_voltage;           // Average bus voltage reading (Volts)
    float avg_current_mA;            // Average current reading (mA)
    float recommended_zero_offset;   // Recommended zero offset correction
} ina226_scan_result_t;

// Sensor control functions
bool init_sensors(bool tare);
bool run_sensors(bool update_stats = false);
void tare_sensors();
void calibrate_ina226();
ina226_scan_result_t calibrate_ina226_scan();
void reset_stats();
void get_stats(test_data_t* data);

// Calibration management functions
void get_calibration(calibration_t* cal);
void set_calibration(const calibration_t* cal);
void apply_calibration(const calibration_t* cal);
calibration_t* get_calibration_ptr();

// Sensor value accessors
extern float lc_value_1; // thrust value from load cell 1
extern float lc_value_2; // torque value from load cell 2

extern float bus_voltage;       //  Volt
extern float shunt_voltage;     //  Volt
extern float current;           //  Ampere
extern float power;             //  Watt
