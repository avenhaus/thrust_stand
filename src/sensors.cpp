#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include <HX711_ADC.h>
#include <INA226.h>
#include "Adafruit_MAX31855.h"


//HX711 constructor (dout pin, sck pin)
HX711_ADC LoadCell_1(HX711_DOUT_1_PIN, HX711_SCK_1_PIN); //HX711 1
float lc_calibration_value_1 = 696.0; // calibration value load cell 1
float lc_value_1 = 0.0; // load cell 1 value

HX711_ADC LoadCell_2(HX711_DOUT_2_PIN, HX711_SCK_2_PIN); //HX711 2
float lc_calibration_value_2 = 733.0; // calibration value load cell 2
float lc_value_2 = 0.0; // load cell 2 value

INA226 INA(0x40);

float shunt = 0.100;                      /* shunt (Shunt Resistance in Ohms). Lower shunt gives higher accuracy but lower current measurement range. Recommended value 0.020 Ohm. Min 0.001 Ohm */
float INA266_max_current = 0.081 * shunt; /* INA226 max current depends on shunt restistor: 0.081 * shunt  */
float current_LSB_mA = 0.05;              /* current_LSB_mA (Current Least Significant Bit in milli Amperes). Recommended values: 0.050, 0.100, 0.250, 0.500, 1, 2, 2.5 (in milli Ampere units) */
float current_zero_offset_mA = -0.785;    /* current_zero_offset_mA (Current Zero Offset in milli Amperes, default = 0) */
uint16_t bus_V_scaling_e4 = 10000;    

float bus_voltage = 0.0;       //  Volt
float shunt_voltage = 0.0;     //  Volt
float current = 0.0;           //  Ampere
float power = 0.0;             //  Watt


Adafruit_MAX31855 thermocouple(MAX31855_CS_PIN);
float thermocouple_temp = 0.0; // Thermocouple temperature value
float thermocouple_max_temp = 0.0; // Thermocouple maximum temperature value


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
    DEBUG_println(FST("# Could not connect to INA226 Current Sensor. Fix and Reboot"));
    return false;
  }
  INA.setMaxCurrentShunt(INA266_max_current, shunt);
  INA.configure(shunt, current_LSB_mA, current_zero_offset_mA, bus_V_scaling_e4);

  calibrate_ina226();
  DEBUG_println(FST("# INA226 Current Sensor initialized."));

  DEBUG_println(FST("# Initialize MAX31855 Thermocouple ..."));
  if (!thermocouple.begin()) {
    DEBUG_println(FST("# Thermocouple MAX31855 initialization failed!"));
    return true;
  }
  DEBUG_println(FST("# Thermocouple MAX31855 initialized."));


  DEBUG_println(FST("# Sensors initialized."));
  return true;
}

bool run_sensors() { 
  static boolean newDataReady = 0;

  // check for new data/start next conversion:
  if (LoadCell_1.update()) newDataReady = true;
  LoadCell_2.update();

  //get smoothed value from data set
  if ((newDataReady)) {
      lc_value_1 = LoadCell_1.getData();
      lc_value_2 = LoadCell_2.getData();
  }
    
  if (LoadCell_1.getTareStatus() == true) { DEBUG_println(FST("# Tare load cell 1 complete")) };
  if (LoadCell_2.getTareStatus() == true) { DEBUG_println(FST("# Tare load cell 2 complete")) };

  bus_voltage = INA.getBusVoltage();
  shunt_voltage = INA.getShuntVoltage_mV();
  current = INA.getCurrent_mA();
  power = INA.getPower_mW();

  thermocouple_temp = thermocouple.readCelsius();
  if (isnan(thermocouple_temp)) {
    DEBUG_println(FST("# Thermocouple fault(s) detected!"));
    uint8_t e = thermocouple.readError();
    if (e & MAX31855_FAULT_OPEN) DEBUG_println(FST("#   FAULT: Thermocouple is open - no connections."));
    if (e & MAX31855_FAULT_SHORT_GND) DEBUG_println(FST("#   FAULT: Thermocouple is short-circuited to GND."));
    if (e & MAX31855_FAULT_SHORT_VCC) DEBUG_println(FST("#   FAULT: Thermocouple is short-circuited to VCC."));
  }
  if (thermocouple_temp > thermocouple_max_temp) {
    thermocouple_max_temp = thermocouple_temp; // update maximum temperature
  } 

  return newDataReady; // return true if new data is ready
}

void calibrate_ina226() {
  DEBUG_println(FST("# Calibrate INA226 Current Sensor"));

  float bv = 0, cu = 0;
  for (int i = 0; i < 10; i++) {
    bv += INA.getBusVoltage();
    cu += INA.getCurrent_mA();
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
    cu += INA.getCurrent_mA();
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
  Serial.print(shunt);
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
