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
unsigned long step_start_ts = 0;

// Add these variables for deceleration
bool decelerating = false;
unsigned long decel_start_ts = 0;
unsigned long decel_duration_ms = 2000; // 2 second deceleration
float decel_start_throttle = 0;

MotorESC motor;

void start_test();
void run_test();
void handle_deceleration(); // New function to handle deceleration

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

  boolean new_data = run_sensors();
 
  if ((new_data)) {
    if (millis() > t + 100) {
      Serial.print(current_step);
      Serial.print(" | Throttle: ");
      Serial.print(motor.getCurrentThrottle() ,1);
      Serial.print("% | Thrust: ");
      Serial.print(lc_value_1);
      Serial.print(" | Torque: ");
      Serial.print(lc_value_2);
      Serial.print(" | Voltage: ");
      Serial.print(bus_voltage);
      //Serial.print(" | Shunt: ");
      //Serial.print(shunt_voltage);
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

  // Run test or handle deceleration, if active
  if (decelerating) {
    handle_deceleration();
  } else {
    run_test();
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') { tare_sensors(); }
    if (inByte == 'c') { motor.calibrate(); }
    if (inByte == 's') { start_test(); }
  }
}

void start_test() {
  current_step = 0;
  step_start_ts = millis();
  decelerating = false; // Make sure deceleration is off
}

void run_test() {
  if (current_step < 0) return; // No test started

  unsigned long now = millis();
  if (now - step_start_ts >= step_time_ms) {
    current_step++;
    if (current_step > total_steps) {
      DEBUG_println(FST("\n# Test completed. Starting deceleration..."));
      
      // Start deceleration instead of immediate stop
      decelerating = true;
      decel_start_ts = now;
      decel_start_throttle = motor.getCurrentThrottle();
      
      return;
    }
    step_start_ts = now;
    float throttle = (float)current_step / total_steps * 100.0f; // Calculate throttle percentage
    motor.setThrottle(throttle); // Set motor throttle
    DEBUG_printf(FST("\n# Step %d/%d: Throttle set to %.2f%%\n"), current_step, total_steps, throttle);
  }
}

void handle_deceleration() {
  unsigned long now = millis();
  unsigned long elapsed = now - decel_start_ts;
  
  if (elapsed >= decel_duration_ms) {
    // Deceleration complete
    motor.stop();
    decelerating = false;
    current_step = -1; // Reset test
    DEBUG_println(FST("\n# Deceleration complete, motor stopped."));
    return;
  }
  
  // Calculate smooth deceleration using either linear or exponential approach
  
  // Linear deceleration (smoother at high speeds, abrupt at the end)
  float progress = (float)elapsed / decel_duration_ms;
  float throttle = decel_start_throttle * (1.0 - progress);
  
  // Alternative: Exponential deceleration (more natural feeling, gentler at the end)
  // float throttle = decel_start_throttle * exp(-5.0 * progress);
  
  motor.setThrottle(throttle);
  
  // Only print debug info every 250ms to avoid flooding the serial console
  static unsigned long last_decel_print = 0;
  if (now - last_decel_print > 250) {
    DEBUG_printf(FST("# Decelerating: %.2f%% throttle\n"), throttle);
    last_decel_print = now;
  }
}