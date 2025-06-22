#pragma once

bool init_sensors(boolean tare);
bool run_sensors();
void tare_sensors();
void calibrate_ina226();

extern float lc_calibration_value_1;
extern float lc_value_1; // load cell 1 value

extern float lc_calibration_value_2;
extern float lc_value_2; // load cell 2 value

extern float bus_voltage;       //  Volt
extern float shunt_voltage;     //  Volt
extern float current;           //  Ampere
extern float power;             //  Watt

extern float thermocouple_temp;
extern float thermocouple_max_temp; // maximum temperature recorded by the thermocouple

