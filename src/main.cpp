/*======================================================================*\
 * ESP32-S3 DC Servo Motor Controller
\*======================================================================*/

/*
 */


#include <Arduino.h>
#include "config.h"
#include <SPI.h>
#include <EEPROM.h>
#include "sensors.h"
#include "motor.h"


PROGMEM const char EMPTY_STRING[] =  "";
PROGMEM const char NEW_LINE[] =  "\n";

PROGMEM const char PROJECT_NAME[] = __PROJECT_NAME__;
PROGMEM const char PROJECT_VERSION[] = __PROJECT_COMPILE__;
PROGMEM const char COMPILE_DATE[] = __DATE__;
PROGMEM const char COMPILE_TIME[] = __TIME__;


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

void start_test();
void run_test();

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
    while (1) { delay(1000); } // Halt the program if sensors initialization fails
  }
}

void loop() {
  // Run the motor control loop to handle acceleration/deceleration
  motor.run();

  boolean new_data = run_sensors();
 
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
      Serial.print(current);
      Serial.print(" | Power: ");
      Serial.print(power);
      Serial.print(" | Temp: ");
      Serial.print(thermocouple_temp);
      Serial.print(" | ");
      Serial.print(thermocouple_max_temp);
      Serial.print("   \r");
      t = millis();
    }
  }

  // Test logic is now simpler with all acceleration/deceleration moved to the motor class
  run_test();

  // receive command from serial terminal
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') { tare_sensors(); }
    if (inByte == 'c') { motor.calibrate(); }
    if (inByte == 's') { start_test(); }
  }
}

void start_test() {
  current_step = 0;
  step_start_ts = millis() + step_accel_time_ms + 100; // Set end time for next step
  
  // Begin first step with smooth acceleration
  // float throttle = (float)current_step / total_steps * 100.0f;
  // motor.setThrottle(throttle, true, 1000); // Smoothly accelerate to first step
  
  DEBUG_printf(FST("\n# Test started.\n"));
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
    DEBUG_println(FST(""));
    current_step++;
    
    if (current_step > total_steps) {
      DEBUG_println(FST("\n# Test completed."));
      
      // Start smooth deceleration to stop
      motor.stop(true, decel_time_ms);
      current_step = -1; // Reset test
      
      return;
    }
    
   step_start_ts = millis() + step_accel_time_ms + 100; // Set start time for next step
   float throttle = (float)current_step / total_steps * 100.0f; // Calculate throttle percentage
    
    // Start smooth acceleration to new throttle
    motor.setThrottle(throttle, true, step_accel_time_ms);
  }
}