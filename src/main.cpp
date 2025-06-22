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

  if (!init_sensors(true)) {
    DEBUG_println(FST("# Sensors initialization failed!"));
    while (1) { delay(1000); } // Halt the program if sensors initialization fails
  }
}

void loop() {

  boolean new_data = run_sensors();
 
  if ((new_data)) {
    if (millis() > t + 100) {
      Serial.print(lc_value_1);
      Serial.print(" | ");
      Serial.print(lc_value_2);
      Serial.print(" | ");
      Serial.print(bus_voltage);
      Serial.print(" | ");
      Serial.print(shunt_voltage);
      Serial.print(" | ");
      Serial.print(current);
      Serial.print(" | ");
      Serial.print(power);
      Serial.print(" | ");
      Serial.print(thermocouple_temp);
      Serial.print(" | ");
      Serial.print(thermocouple_max_temp);
      Serial.print("   \r");
      t = millis();
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') { tare_sensors(); }
  }

}

#if 0


#if 0
  EEPROM.begin(EEPROM_SIZE);
  eeprom_data_t eeprom_data;
  EEPROM.get(0, eeprom_data);
  if (eeprom_data.magic == EEPROM_MAGIC) {
    memcpy(home_angle, eeprom_data.home_angle, sizeof(home_angle));
    DEBUG_print(FST("# Home angles loaded from EEPROM: "));
    for (int i = 0; i < ENCODER_COUNT; i++) {
      DEBUG_printf(FST("%0.2f"), home_angle[i]);
      if (i < ENCODER_COUNT - 1) { DEBUG_print(FST(", ")); }
    }
    DEBUG_println();
#ifdef HAS_RGB_LED
    neopixelWrite(RGB_BUILTIN, 0, 10, 0);
#endif
  } else {
    DEBUG_println(FST("# No valid EEPROM data found"));
#ifdef HAS_RGB_LED
    neopixelWrite(RGB_BUILTIN, 10, 10, 0);
#endif
  }
  EEPROM.end();
#endif



/***********************************************************\
 * Main Loop
\***********************************************************/

void loop() {
  static bool last_button = HIGH;
  u32_t start = millis();

  // Wait for the next loop.
  delay(LOOP_DELAY - (millis() % LOOP_DELAY));
}

#if 0
bool save() {
      for (int i = 0; i < ENCODER_COUNT; i++) {
      home_angle[i] = data[i].angle;
    }
    change = true;
    // Save calibration angles to EEPROM.
    EEPROM.begin(EEPROM_SIZE);
    eeprom_data_t eeprom_data;
    eeprom_data.magic = EEPROM_MAGIC;
    memcpy(eeprom_data.home_angle, home_angle, sizeof(home_angle));
    EEPROM.put(0, eeprom_data);
    EEPROM.end();
    DEBUG_println(FST("# Saved Home Position to EEPROM"));

}
#endif
#endif