#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "thermal.h"
#include "rpm_sensor.h"
#include <HX711_ADC.h>
#include <INA226.h>


//HX711 constructor (dout pin, sck pin)
HX711_ADC LoadCell_1(HX711_DOUT_1_PIN, HX711_SCK_1_PIN); //HX711 1
float lc_calibration_value_1 = 1941.5; // calibration value load cell 1
float lc_value_1 = 0.0; // load cell 1 value

HX711_ADC LoadCell_2(HX711_DOUT_2_PIN, HX711_SCK_2_PIN); //HX711 2
float lc_calibration_value_2 = 1904; // calibration value load cell 2
float lc_value_2 = 0.0; // load cell 2 value

INA226 INA(0x40);
float cal =  (481.15 / 290.0);
float shunt = 0.002*(481.15 / 290.0);                      /* shunt (Shunt Resistance in Ohms). Lower shunt gives higher accuracy but lower current measurement range. Recommended value 0.020 Ohm. Min 0.001 Ohm */
float INA266_max_current = 0.081 / shunt; /* INA226 max current depends on shunt restistor: 0.081 * shunt  */
float current_LSB_mA = 0.5;              /* current_LSB_mA (Current Least Significant Bit in milli Amperes). Recommended values: 0.050, 0.100, 0.250, 0.500, 1, 2, 2.5 (in milli Ampere units) */
float current_zero_offset_mA = -23.850;// -39.650*cal;    /* current_zero_offset_mA (Current Zero Offset in milli Amperes, default = 0) | Needs to be updated along with shunt!! */
uint16_t bus_V_scaling_e4 = 10000;    

float bus_voltage = 0.0;       //  Volt
float shunt_voltage = 0.0;     //  Volt
float current = 0.0;           //  Ampere
float power = 0.0;             //  Watt


unsigned int lc_value_count = 0; // Counter for number of values read from sensors
float thrust_sum = 0.0; // Sum of thrust readings
float torque_sum = 0.0; // Sum of torque readings

unsigned int sensor_value_count = 0; // Counter for number of values read from sensors
float bus_voltage_sum = 0.0; // Sum of bus voltage readings
float shunt_voltage_sum = 0.0; // Sum of shunt voltage readings
float current_sum = 0.0; // Sum of current readings
float power_sum = 0.0; // Sum of power readings

// Thermal tracking for test steps
float thermal_max_step = 0.0f;   // Max temperature seen during this step
bool  thermal_had_valid_frame = false; // Did we get at least one valid frame this step?
float sensor_temp_step = 25.0f; // Sensor temperature during this step
bool  sensor_temp_had_valid_frame = false; // Did we get at least one valid sensor temp reading this step?

RpmSensor rpm_sensor; // RPM sensor instance
float rpm = 0.0; // Current RPM value

bool init_sensors(boolean tare) {
  // Initialize the thermocouple.
  
  // Initialize the load cells.
  DEBUG_println(FST("# Initialize HX711 Load Cells ..."));
  LoadCell_1.begin();
  LoadCell_2.begin();

  // Set gain for HX711 (default is 128).
  LoadCell_1.setGain(128);
  LoadCell_2.setGain(128);

  unsigned long stabilizingtime = 3000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  while ((loadcell_1_rdy + loadcell_2_rdy) < 2) { //run startup, stabilization and tare, both modules simultaniously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilizingtime, tare);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilizingtime, tare);
  }
  if (LoadCell_1.getTareTimeoutFlag()) {
    DEBUG_println(FST("# Timeout, check MCU => HX711 No.1 wiring and pin designations"));
    return false;
  }
  if (LoadCell_2.getTareTimeoutFlag()) {
    DEBUG_println(FST("# Timeout, check MCU => HX711 No.2 wiring and pin designations"));
    return false;
  }
  LoadCell_1.setCalFactor(lc_calibration_value_1); // user set calibration value (float)
  LoadCell_2.setCalFactor(lc_calibration_value_2); // user set calibration value (float)

  Wire.begin();
  if (!INA.begin() )
  {
    DEBUG_println(FST("# Could not connect to INA226 Current Sensor."));
    return false;
  }
  INA.setMaxCurrentShunt(INA266_max_current, shunt);
  INA.configure(shunt, current_LSB_mA, current_zero_offset_mA, bus_V_scaling_e4);
  //calibrate_ina226();
  DEBUG_println(FST("# INA226 Current Sensor initialized."));

  // Initialize MLX90640 thermal sensor (non-critical — continues if absent)
  thermal_init();

  DEBUG_println(FST("# Initialize RPM Sensor ..."));
  rpm_sensor.begin(RPM_SENSOR_PIN);

  DEBUG_println(FST("# Sensors initialized."));
  return true;
}
    
bool run_sensors(bool update_stats) { 
  static boolean newDataReady = 0;

  // check for new data/start next conversion:
  if (LoadCell_1.update()) newDataReady = true;
  LoadCell_2.update();

  //get smoothed value from data set
  if ((newDataReady)) {
      lc_value_1 = LoadCell_1.getData();
      lc_value_2 = LoadCell_2.getData();

    if (update_stats) {
      lc_value_count++; // Increment the counter for number of values read from sensors
      thrust_sum += lc_value_1; // Add the current thrust value to the sum
      torque_sum += lc_value_2; // Add the current torque value to the sum
    }
  }
    
  if (LoadCell_1.getTareStatus() == true) { DEBUG_println(FST("# Tare load cell 1 complete")) };
  if (LoadCell_2.getTareStatus() == true) { DEBUG_println(FST("# Tare load cell 2 complete")) };

  bus_voltage = INA.getBusVoltage();
  shunt_voltage = INA.getShuntVoltage_mV();
  current = INA.getCurrent_mA() / 1000.0; 
  power = INA.getPower_mW() / 1000.0;

  rpm = rpm_sensor.update(); // Update RPM sensor

  // Update thermal sensor (non-blocking)
  thermal_update();

  if (update_stats) {
    sensor_value_count++; // Increment the counter for number of values read from sensors
    bus_voltage_sum += bus_voltage; // Add the current bus voltage value to the sum
    shunt_voltage_sum += shunt_voltage; // Add the current shunt voltage value to the sum
    current_sum += current; // Add the current value to the sum
    power_sum += power; // Add the current power value to the sum

    // Track thermal max temperature and ambient per step
    if (thermal_is_available() && thermal_get_frame_age_ms() < THERMAL_STALE_MS) {
      thermal_had_valid_frame = true;
      sensor_temp_had_valid_frame = true;
      float frm = thermal_get_frame_max();
      if (frm > thermal_max_step) thermal_max_step = frm;
      sensor_temp_step = thermal_get_frame_ambient();
    }
  }


  return newDataReady; // return true if new data is ready
}

void calibrate_ina226() {
  DEBUG_println(FST("# Calibrate INA226 Current Sensor"));

  float bv = 0, cu = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    cu += INA.getCurrent_mA() / 1000.0;
    delay(150);
  }
  bv /= 10;
  cu /= 10;
  DEBUG_print(FST("\nAverage Bus and Current values for use in Shunt Resistance, Bus Voltage and Current Zero Offset calibration:"));
  bv = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    delay(100);
  }
  bv /= 10;
  Serial.print("\nAverage of 10 Bus Voltage values = ");
  Serial.print(bv, 3);
  Serial.println("V");
  cu = 0;
  for (int i = 0; i < 10; i++) {
    cu += INA.getCurrent_mA() / 1000.0;
    delay(100);
  }
  cu /= 10;
  Serial.print("Average of 10 Current values = ");
  Serial.print(cu, 3);
  Serial.println("mA");

  Serial.println("\nCALIBRATION VALUES TO USE:\t(DMM = Digital MultiMeter)");
  Serial.println("Step 5. Attach a power supply with voltage 5-10V to INA226 on VBUS/IN+ and GND pins, without any load.");
  Serial.print("\tcurrent_zero_offset_mA = ");
  Serial.print(current_zero_offset_mA + cu, 3);
  Serial.println("mA");
  if(cu > 5)
    Serial.println("********** NOTE: No resistive load needs to be present during current_zero_offset_mA calibration. **********");
  Serial.print("\tbus_V_scaling_e4 = ");
  Serial.print(bus_V_scaling_e4);
  Serial.print(" / ");
  Serial.print(bv, 3);
  Serial.println(" * (DMM Measured Bus Voltage)");
  Serial.println("Step 8. Set DMM in current measurement mode. Use a resistor that will generate around 50-100mA IOUT measurement between IN- and GND pins with DMM in series with load. Note current measured on DMM.");
  Serial.print("\tshunt = ");
  Serial.print(shunt,4);
  Serial.print(" * ");
  Serial.print(cu, 3);
  Serial.println(" / (DMM Measured IOUT)");
  if(cu < 40)
    Serial.println("********** NOTE: IOUT needs to be more than 50mA for better shunt resistance calibration. **********");
}

void tare_sensors() {
  DEBUG_println(FST("# Taring sensors ..."));
  LoadCell_1.tareNoDelay();
  LoadCell_2.tareNoDelay();
  DEBUG_println(FST("# Sensors tared."));
}

void reset_stats() {
    lc_value_count = 0;
    thrust_sum = 0.0;
    torque_sum = 0.0;

    sensor_value_count = 0;
    bus_voltage_sum = 0.0;
    shunt_voltage_sum = 0.0;
    current_sum = 0.0;
    power_sum = 0.0;

    thermal_max_step = 0.0f;
    thermal_had_valid_frame = false;
    sensor_temp_step = 25.0f;
    sensor_temp_had_valid_frame = false;
}

// Fill in the test_data_t structure with the current average sensor values
void get_stats(test_data_t* data){
    // Make sure we don't divide by zero
    if (!data) {
        DEBUG_println(FST("# ERROR: Null pointer passed to get_stats"));
        return;
    }
    
    // Store the sample counts
    data->lc_samples = lc_value_count;
    data->sensor_samples = sensor_value_count;
    
    // Calculate averages for load cell values (if any samples were collected)
    if (lc_value_count > 0) {
        data->thrust = thrust_sum / lc_value_count;
        data->torque = torque_sum / lc_value_count;
    } else {
        data->thrust = lc_value_1; // Use current value if no samples
        data->torque = lc_value_2;
    }
    
    // Calculate averages for other sensor values (if any samples were collected)
    if (sensor_value_count > 0) {
        data->voltage = bus_voltage_sum / sensor_value_count;
        data->current = current_sum / sensor_value_count;
        data->power = power_sum / sensor_value_count;
    } else {
        data->voltage = bus_voltage; // Use current values if no samples
        data->current = current;
        data->power = power;
    }
    
    // Thermal — store per-step max and ambient values
    data->thermal_max   = thermal_max_step;
    data->thermal_valid = thermal_had_valid_frame;

    
    // Add RPM values from the global variables
    data->rpm = rpm_sensor.getRPM();
    
    // Note that we don't set throttle here as it should be set by the caller
    // who knows what the throttle setting is
   
}
