#pragma once

typedef struct {
    float throttle;          // Throttle percentage (0.0 to 100.0)
    float thrust;            // Thrust value from load cell 1
    float torque;            // Torque value from load cell 2
    float voltage;           // Bus voltage from INA226
    float current;           // Current from INA226
    float power;             // Power from INA226
    float rpm;               // Current RPM value    
    float temperature;       // Temperature from thermocouple
    float temperature_max;
    unsigned int lc_samples; // Number of load cell samples
    unsigned int sensor_samples; // Number of other sensor samples
} test_data_t;

bool init_sensors(bool tare);
bool run_sensors(bool update_stats = false);
void tare_sensors();
void calibrate_ina226();
void reset_stats();
void get_stats(test_data_t* data);

extern float lc_calibration_value_1;
extern float lc_value_1; // load cell 1 value

extern float lc_calibration_value_2;
extern float lc_value_2; // load cell 2 value

extern float bus_voltage;       //  Volt
extern float shunt_voltage;     //  Volt
extern float current;           //  Ampere
extern float power;             //  Watt

extern float thermocouple_temp;
extern float mlx_ambient_temp; // MLX90614 ambient temperature
extern float mlx_object_temp;  // MLX90614 object temperature
