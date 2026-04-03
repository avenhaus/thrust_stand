/*======================================================================*\
 * ESP32 Thrust Stand
\*======================================================================*/
// This code is part of the ESP32 Thrust Stand project.
// It is designed to control a brushless motor ESC, read sensor data,
// and perform tests to measure thrust, torque, power, RPM, temperature and other parameters.

#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <EEPROM.h>
#include "sensors.h"
#include "motor.h"
#include "analog.h"


PROGMEM const char EMPTY_STRING[] =  "";
PROGMEM const char NEW_LINE[] =  "\n";

PROGMEM const char PROJECT_NAME[] = __PROJECT_NAME__;
PROGMEM const char PROJECT_VERSION[] = __PROJECT_COMPILE__;
PROGMEM const char COMPILE_DATE[] = __DATE__;
PROGMEM const char COMPILE_TIME[] = __TIME__;

//Fix poti filter
//Fix Poti end  
//Kalman filter
//Max obj temp
//add temps to stats
//calibrate sensor


Print* debugStream = &LOGGER;

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define HAS_RGB_LED 1
#endif

unsigned long t = 0;

int current_step = -1;
unsigned int total_steps = 20;
unsigned long step_time_ms = 2000;
unsigned long step_accel_time_ms = 1000;
unsigned long step_start_ts = 0;
unsigned long decel_time_ms = 3000; // Time to decelerate to stop

MotorESC motor;
Potentiometer poti(POTI_PIN, 0, 4095, 0.1f); // Potentiometer for throttle control
float poti_value = 0.0f; // Current potentiometer value
bool analogThrottle = false; 

test_data_t test_data[101] = {0};

extern float rpm; // Current RPM value

void check_serial();
void setThrottle(float throttle);
void tare_sensors();
void start_test();
void run_test();
void abort_test(); // Add this line
void print_stats(const test_data_t& data);
void print_csv_results(); // Add this line
void print_help(); // Add this function declaration

/***********************************************************\
 * Initialization Code
\***********************************************************/

void setup() {
#ifdef HAS_RGB_LED
  // There is currently a bug in the ESP32-S3 neopixelWrite() code. This will produce the 
  // following errors and the RGB LED will not light up.:
  //  E (19) rmt: rmt_set_gpio(526): RMT GPIO ERROR
  //  E (19) rmt: rmt_config(686): set gpio for RMT driver failed
  //  ==> Remove the #ifdef RGB_BUILTIN logic in the neopixelWrite() code.
  neopixelWrite(RGB_BUILTIN, 0, 10, 10);
#else
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  
#endif

  pinMode(BUTTON_TARE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_START_PIN, INPUT_PULLUP);

  LOGGER.begin(SERIAL_SPEED);
  delay(100);
  DEBUG_printf(FST("\n\n# %s %s | %s | %s\n"), PROJECT_NAME, PROJECT_VERSION, COMPILE_DATE, COMPILE_TIME);

  DEBUG_println(FST("# Motor ESC ..."));
  motor.begin(MOTOR_ESC_PIN, 1000, 2000, 50, 16, 1);
  motor.arm(); // Arm the ESC with a 2 second min throttle
  DEBUG_println(FST("# Motor ESC initialized."));

  if (!init_sensors(true)) {
    DEBUG_println(FST("# Sensors initialization failed!"));
    //while (1) { delay(1000); } // Halt the program if sensors initialization fails
  }
  
  // Print keyboard command help
  print_help();
}

void loop() {
  // Run the motor control loop to handle acceleration/deceleration
  motor.run();

  boolean new_data = run_sensors(step_start_ts >= millis());
 
  if ((new_data)) {
    if (millis() > t + 100) {
      Serial.print(current_step);
      Serial.print(" | Throttle: ");
      Serial.print(motor.getCurrentThrottle(), 1);
      Serial.print("% | State: ");
      
      // Print motor state
      switch (motor.getState()) {
        case MotorESC::STATE_IDLE: Serial.print("IDLE"); break;
        case MotorESC::STATE_RUNNING: Serial.print("RUNNING"); break;
        case MotorESC::STATE_ACCELERATING: Serial.print("ACCEL"); break;
        case MotorESC::STATE_DECELERATING: Serial.print("DECEL"); break;
      }
      
      Serial.print(" | Thrust: ");
      Serial.print(lc_value_1);
      Serial.print(" | Torque: ");
      Serial.print(lc_value_2);
      Serial.print(" | Voltage: ");
      Serial.print(bus_voltage);
      Serial.print(" | Current: ");
      Serial.print(current, 3);
      Serial.print(" | Power: ");
      Serial.print(power);
      Serial.print(" | RPM: ");
      Serial.print(rpm);
      Serial.print(" | Temp: ");
      Serial.print(thermocouple_temp);
      Serial.print(" | Amb: ");
      Serial.print(mlx_ambient_temp);
      Serial.print(" | Obj: ");
      Serial.print(mlx_object_temp);
      Serial.print(" | Pot: ");
      Serial.print(poti_value);
      Serial.print("   \r");
      t = millis();
    }
  }

  // Test logic is now simpler with all acceleration/deceleration moved to the motor class
  run_test();

  check_serial(); // Check for serial input commands
  
  poti_value = poti.read(); // Read potentiometer value
  if (analogThrottle) {
    // Use potentiometer value for throttle control
    motor.setThrottle(poti_value * 100.0); 
  }
}

void setThrottle(float throttle) {
  analogThrottle = false; // Disable analog throttle control
  if (current_step >= 0) {abort_test(); return; }
  motor.setThrottle(throttle, true, 3000 * abs((motor.getCurrentThrottle() - throttle)) / 100.0f); // Smooth acceleration based on throttle change
}

void check_serial() {
// receive command from serial terminal
  if (Serial.available() <= 0) { return; }

  char inByte = Serial.read();
  if (inByte == 't') { tare_sensors(); }
  else if (inByte == 'c') { motor.calibrate(); }
  else if (inByte == 's') { 
    if (current_step < 0) {
      // No test running, start a new one
      start_test(); 
    } else {
      // Test is already running, abort it
      abort_test();
    }
  }
  else if (inByte == 'p') { print_csv_results(); } // Print CSV data on demand
  else if (inByte == ' ') { // Stop the motor
        if (current_step >= 0) { abort_test(); }
        else {  setThrottle(0.0); }
  }
  else if (inByte == '1') { setThrottle(10.0); } 
  else if (inByte == '2') { setThrottle(20.0); }
  else if (inByte == '3') { setThrottle(30.0); }
  else if (inByte == '4') { setThrottle(40.0); }
  else if (inByte == '5') { setThrottle(50.0); }
  else if (inByte == '6') { setThrottle(60.0); }
  else if (inByte == '7') { setThrottle(70.0); }
  else if (inByte == '8') { setThrottle(80.0); }
  else if (inByte == '9') { setThrottle(90.0); }
  else if (inByte == '0') { setThrottle(100.0); }
  else if (inByte == 'a') {
    if (current_step >= 0) { abort_test(); }
    else if (poti_value > 0.1f) {
      DEBUG_printf(FST("\n\n# Knob is at %d%%. Turn to 0 to start analog throttle control.\n"), (int)poti_value);
    } else {  analogThrottle = true; }
  }
  else if (inByte == 'h') { print_help(); } // Print help message
}

void start_test() {
  reset_stats(); // Reset statistics before starting the test
  current_step = 0;
  step_start_ts = millis() + step_accel_time_ms + 100; // Set end time for next step
  
  // Begin first step with smooth acceleration
  // float throttle = (float)current_step / total_steps * 100.0f;
  // motor.setThrottle(throttle, true, 1000); // Smoothly accelerate to first step
  
  DEBUG_printf(FST("\n# Test started.\n"));
}

void print_stats(const test_data_t& data) { 
  DEBUG_printf(FST("%d | "), current_step);
  DEBUG_printf(FST("Throttle: %.2f%% | "), data.throttle);
  DEBUG_printf(FST("Thrust: %.2f | "), data.thrust);
  DEBUG_printf(FST("Torque: %.2f | "), data.torque);
  DEBUG_printf(FST("Voltage: %.2f | "), data.voltage);
  DEBUG_printf(FST("Current: %.3f | "), data.current);
  DEBUG_printf(FST("Power: %.2f | "), data.power);
  float efficiency = 0;
    if (data.power > 0) {
      efficiency = data.thrust / (data.power / 1000.0); // g/W
    }
  DEBUG_printf(FST("Power: %.2f | "), data.power);
  DEBUG_printf(FST("RPM: %d | "), (uint32_t)data.rpm);
  DEBUG_printf(FST("Temperature: %.2f | Max: %.2f | "), data.temperature, data.temperature_max);
  DEBUG_printf(FST(" %u |"), data.lc_samples);
  DEBUG_printf(FST("%u\n"), data.sensor_samples);
}

void print_csv_results() {
  // Print CSV header
  Serial.println(F("\n\n*** TEST RESULTS CSV DATA ***\n"));
  Serial.println(F("Step,Throttle(%),Thrust(g),Torque(g·cm),Voltage(V),Current(A),Power(W),Temp(°C),MaxTemp(°C),RPM,Efficiency(g/W),Samples"));
  Serial.println(F("Step,Throttle(%),Thrust(g),Voltage(V),Current(A),Power(W),RPM,Efficiency(g/W),Samples"));
  
  // Print data rows
  for (int i = 0; i <= total_steps; i++) {
    // Skip rows with no data (where throttle is 0 and samples are 0)
    if (test_data[i].throttle == 0 && 
        test_data[i].lc_samples == 0 && 
        test_data[i].sensor_samples == 0) {
      continue;
    }
    
    // Calculate efficiency if we have both thrust and power data
    float efficiency = 0;
    if (test_data[i].power > 0) {
      efficiency = test_data[i].thrust / (test_data[i].power / 1000.0); // g/W
    }
    
    // Print row data
    Serial.print(i);
    Serial.print(F(","));
    Serial.print(test_data[i].throttle, 2);
    Serial.print(F(","));
    Serial.print(test_data[i].thrust, 2);
    Serial.print(F(","));
    //Serial.print(test_data[i].torque, 2);
    //Serial.print(F(","));
    Serial.print(test_data[i].voltage, 2);
    Serial.print(F(","));
    Serial.print(test_data[i].current, 3);
    Serial.print(F(","));
    Serial.print(test_data[i].power, 2);
    Serial.print(F(","));
    //Serial.print(test_data[i].temperature, 2);
    //Serial.print(F(","));
    //Serial.print(test_data[i].temperature_max, 2);
    //tSerial.print(F(","));
    Serial.print(test_data[i].rpm, 0);
    Serial.print(F(","));
    Serial.print(efficiency, 2);
    Serial.print(F(","));
    Serial.print(test_data[i].lc_samples);
    Serial.println(F("               "));

  }
  
  Serial.println(F("\n\n*** END OF TEST RESULTS ***"));
}

void run_test() {
  if (current_step < 0) return; // No test started
  
  // Don't proceed to the next step if we're still accelerating/decelerating
  if (motor.getState() == MotorESC::STATE_ACCELERATING || 
      motor.getState() == MotorESC::STATE_DECELERATING) {
    return;
  }

  unsigned long now = millis();
  if (now >= step_start_ts + step_time_ms) {
    test_data[current_step].throttle = motor.getCurrentThrottle();
    get_stats(&test_data[current_step]);
    print_stats(test_data[current_step]);

    current_step++;
    if (current_step < 6) { current_step = 6; } // Ensure we start from step 6 if we are below it
    
    if (current_step > total_steps) {
      DEBUG_println(FST("\n# Test completed."));
      print_csv_results(); // Print results in CSV format
      
      // Start smooth deceleration to stop
      motor.stop(true, decel_time_ms);
      current_step = -1; // Reset test
      
      return;
    }
    
   step_start_ts = millis() + step_accel_time_ms + 100; // Set start time for next step
   float throttle = (float)current_step / total_steps * 100.0f; // Calculate throttle percentage
    
    // Start smooth acceleration to new throttle
    motor.setThrottle(throttle, true, step_accel_time_ms);

    reset_stats(); // Reset statistics for the new step
  }
}

void abort_test() {
  if (current_step < 0) {
    DEBUG_println(FST("# No test running to abort."));
    return;
  }
  
  DEBUG_println(FST("\n# Test aborted!"));
  
  // Stop motor with smooth deceleration
  motor.stop(true, decel_time_ms);
  
  // Consider printing the results we have so far
  print_csv_results();
  
  // Reset test state
  current_step = -1;
}

void print_help() {
  Serial.println(F("\n*** Command List ***"));
  Serial.println(F(" t - Tare sensors"));
  Serial.println(F(" c - Calibrate motor ESC"));
  Serial.println(F(" s - Start/Stop test"));
  Serial.println(F(" p - Print CSV results"));
  Serial.println(F(" 0-9 - Set throttle to 10%%-100%%"));
  Serial.println(F(" a - Enable analog throttle control"));
  Serial.println(F(" SPACE - Stop motor"));
  Serial.println(F(" h - Print this help message"));
  Serial.println(F("Press 's' to start the test, or 'h' to see this help message."));
}