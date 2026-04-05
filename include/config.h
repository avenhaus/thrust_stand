#pragma once

#include <Arduino.h>

/********************************************\
|*  Pin Definitions
\********************************************/


/*
https://drive.google.com/file/d/1gbKM7DA7PI7s1-ne_VomcjOrb0bE2TPZ/view
---+------+----+-----+-----+-----------+---------------------------
No.| GPIO | IO | RTC | ADC | Default   | Function
---+------+----+-----+-----+-----------+---------------------------
25 |   0* | IO | R11 | 2_1 | Boot      | Button TARE
35 |   1  | IO |     |     | UART0_TXD | USB Programming/Debug
24 |   2* | IO | R12 | 2_2 |           | LCD_DC
34 |   3  | IO |     |     | UART0_RXD | USB Programming/Debug
26 |   4* | IO | R10 | 2_0 |           | 
29 |   5* | IO |     |     | SPI0_SS   | SD_CS (LED)  
14 |  12* | IO | R15 | 2_5 |           | LCD_LED
16 |  13  | IO | R14 | 2_4 |           | RPM Input
13 |  14  | IO | R16 | 2_6 |           | TOUCH_CS
23 |  15* | IO | R13 | 2_3 |           | LCD_CS
27 |  16+ | IO |     |     | UART2_RXD | MLX90640_SDA (I2C1)
28 |  17+ | IO |     |     | UART2_TXD | MLX90640_SCL (I2C1)
30 |  18  | IO |     |     | SPI0_SCK  | SCK MAX31855, LCD, Touch, D
31 |  19  | IO |     |     | SPI0_MISO | MISO MAX31855, Touch, SD
33 |  21  | IO |     |     | I2C0_SDA  | INA226_SDA
36 |  22  | IO |     |     | I2C0_SCL  | INA226_SCL
37 |  23  | IO |     |     | SPI0_MOSI | MOSI LCD, SD
10 |  25  | IO | R06 | 2_8 |DAC1/I2S-DT| BUTTON START / STOP
11 |  26  | IO | R07 | 2_9 |DAC2/I2S-WS| Motor ESC PWM
12 |  27  | IO | R17 | 2_7 | I2S-BCK   | MAX31855_CS
8  |  32  | IO | R09 | 1_4 |           | HC711-CLK2
9  |  33  | IO | R08 | 1_5 |           | HC711-CLK1
6  |  34  | I  | R04 | 1_6 |           | HC711-DOUT1
7  |  35  | I  | R05 | 1_7 |           | HC711-DOUT2
4  |  36  | I  | R00 | 1_0 | SENSOR_VP | Potentiometer
5  |  39  | I  | R03 | 1_3 | SENSOR_VN | 
3  |  EN  | I  |     |     | RESET     | Reset LCD       
---+------+----+-----+-----+-----------+---------------------------
(IO6 to IO11 are used by the internal FLASH and are not useable)
22 x I/O  + 4 x input only = 26 usable pins 
GPIO_34 - GPIO_39 have no internal pullup / pulldown.
- ADC2 can not be used with WIFI/BT (easily)
+ GPIO 16 and 17 are not available on WROVER (PSRAM)
* Strapping pins: IO0, IO2, IO4, IO5 (HIGH), IO12 (LOW), IO15 (HIGH)
*/


// #define USE_IPS_DISPLAY 1 // ILI9481 instead of ILI9488
// The IPS display looks better, but is slower as ILI9481 can only be run at 15MHz vs 70MHz SPI for ILI9488.
// IPS is also more expensive.
// 10.66 FPS vs 3.59 FPS
// Fix potential stripes by modifying Panel_9481 driver from:
//   CMD_PWSET  , 3, 0x07, 0x42, 0x18,
// to:
//   CMD_PWSET  , 3, 0x07, 0x42, 0x17,

#define LCD_SCK_PIN 18
#define LCD_MOSI_PIN 23
#define LCD_MISO_PIN 19
#define LCD_RST_PIN -1
#define LCD_DC_PIN 2
#define LCD_CS_PIN 15
#define LCD_LED_PIN 12
#define LCD_LED_PWM_CHANNEL 0

#define TOUCH_CS_PIN 14
#define TOUCH_CS_IRQ -1

#define SD_CS_PIN 5
#define LED_PIN 5

#define BUZZER_PIN 21

#define ADC_VREF 1100

#define VOLTAGE_PIN -1 // 34

#define HX711_SCK_1_PIN 33 // mcu > HX711 no 1 sck pin 
#define HX711_DOUT_1_PIN 34 // mcu > HX711 no 1 dout pin
#define HX711_SCK_2_PIN 32 // mcu > HX711 no 2 sck pin
#define HX711_DOUT_2_PIN 35 // mcu > HX711 no 2 dout pin

#define RPM_SENSOR_PIN 13     // GPIO pin connected to the optical sensor

#define POTI_PIN 36 // GPIO pin connected to the potentiometer

#define MOTOR_ESC_PIN 26

#define BUTTON_TARE_PIN 0
#define BUTTON_START_PIN 25


/*
ESP32 S3 Pins
https://www.wemos.cc/en/latest/s3/s3.html
---+------+----+-----+-----+-----------+---------------------------
No.| GPIO | IO | RTC | ADC | Default   | Function
---+------+----+-----+-----+-----------+---------------------------
   |   0* | IO |   0 |     | Boot      | Button
   |   1  | IO |   1 | 1_0 |           | 
   |   2  | IO |   2 | 1_1 |           | 
   |   3* | IO |   3 | 1_2 |           | 
   |   4  | IO |   4 | 1_3 |           | HX711-CLK1
   |   5  | IO |   5 | 1_4 |           | HX711-DOUT1
   |   6  | IO |   6 | 1_5 |           | HX711-CLK2
   |   7  | IO |   7 | 1_6 |           | HX711-DOUT2
   |   8  | IO |   8 | 1_7 |           | 
   |   9  | IO |   9 | 1_8 |           | 
   |  10  | IO |  10 | 1_9 | SPI-SS    | 
   |  11  | IO |  11 | 2_0 | SPI-MOSI  | 
   |  12  | IO |  12 | 2_1 | SPI-SCK   | 
   |  13  | IO |  13 | 2_2 | SPI-MISO  | 
   |  14  | IO |  14 | 2_3 |           | 
   |  15  | IO |  15 | 2_4 |           |
   |  16  | IO |  16 | 2_5 |           |
   |  17  | IO |  17 | 2_6 |           |
   |  18  | IO |  18 | 2_7 |           |
   |  19  | IO |  19 | 2_8 | USB/JTAG  |
   |  20  | IO |  20 | 2_9 | USB/JTAG  |
   |  21  | IO |  21 |     |           |
   |  38  | IO |     |     |           | RGB LED
   |  39  | IO |     |     |           | 
   |  40  | IO |     |     |           | 
   |  41  | IO |     |     | I2C_SCL   | 
   |  42  | IO |     |     | I2C_SDA   | 
   |  43  | IO |     |     | UART_TX0  |
   |  44  | IO |     |     | UART_RX0  |
   |  45* | IO |     |     |           |
   |  46* | IO |     |     |           |
   |  47  | IO |     |     |           |
   |  48  | IO |     |     |           |
---+------+----+-----+-----+-----------+---------------------------
* Strapping pins: IO0, IO3, IO45, IO46
*/


// #define BUTTON_PIN 0
// #define RGB_LED_PIN 38

// #define I2C_SCL_PIN 41
// #define I2C_SDA1_PIN 42
// #define I2C_SDA2_PIN 40
// #define I2C_SDA3_PIN 39

// #define SPI_SS_PIN 10
// #define SPI_MOSI_PIN 11
// #define SPI_SCK_PIN 12
// #define SPI_MISO_PIN 13

// #define HX711_SCK_1_PIN 4 // mcu > HX711 no 1 sck pin 
// #define HX711_DOUT_1_PIN 5 // mcu > HX711 no 1 dout pin
// #define HX711_SCK_2_PIN 6 // mcu > HX711 no 2 sck pin
// #define HX711_DOUT_2_PIN 7 // mcu > HX711 no 2 dout pin

//number of samples in moving average dataset, value must be 1, 2, 4, 8, 16, 32, 64 or 128.
#define SAMPLES 					16		//default value: 16

//adds extra sample(s) to the dataset and ignore peak high/low sample, value must be 0 or 1.
#define IGN_HIGH_SAMPLE 			1		//default value: 1
#define IGN_LOW_SAMPLE 				1		//default value: 1

//microsecond delay after writing sck pin high or low. This delay could be required for faster mcu's.
//So far the only mcu reported to need this delay is the ESP32 (issue #35), both the Arduino Due and ESP8266 seems to run fine without it.
//Change the value to '1' to enable the delay.
#define SCK_DELAY					0		//default value: 0

//if you have some other time consuming (>60μs) interrupt routines that trigger while the sck pin is high, this could unintentionally set the HX711 into "power down" mode
//if required you can change the value to '1' to disable interrupts when writing to the sck pin.
#define SCK_DISABLE_INTERRUPTS		0		//default value: 0


#define LOOP_DELAY 10

/********************************************\
|*  MLX90640 Thermal Sensor
\********************************************/
#define MLX90640_I2C_ADDR     0x33
#define MLX90640_I2C_SDA_PIN  16     // Dedicated I2C bus (Wire1) for MLX90640
#define MLX90640_I2C_SCL_PIN  17
#define MLX90640_I2C_FREQ     1000000 // 1 MHz FM+ — MLX90640 supports up to 1 MHz
#define MLX90640_REFRESH_RATE 0x05   // 8 Hz (register value)
#define THERMAL_COLS          32
#define THERMAL_ROWS          24
#define THERMAL_PIXELS        (THERMAL_COLS * THERMAL_ROWS)  // 768

// Default ROI (region of interest) — centered 12x10 block
#define THERMAL_ROI_X_DEFAULT   10
#define THERMAL_ROI_Y_DEFAULT   7
#define THERMAL_ROI_W_DEFAULT   12
#define THERMAL_ROI_H_DEFAULT   10

// Thermal frame considered stale after this many ms
#define THERMAL_STALE_MS        2000

/********************************************\
|*  Wi-Fi Configuration
\********************************************/
#include "secrets.h"   // WiFi AP credentials (edit secrets.h, not this file)

#define WIFI_STA_CONNECT_TIMEOUT_MS   10000   // 10 s initial connect window
#define WIFI_STA_RECONNECT_TIMEOUT_MS 15000   // 15 s runtime reconnect window

/********************************************\
|*  Web Control
\********************************************/
#define WEB_THROTTLE_TIMEOUT_MS  5000    // 5 s heartbeat timeout for manual web throttle
#define WEB_TELEMETRY_INTERVAL_MS 100    // ~10 Hz telemetry push
#define WEB_THERMAL_INTERVAL_MS   125    // ~8 Hz thermal frame push

// Heap safety margin — throttle thermal rate if free heap drops below this
#define HEAP_SAFETY_MARGIN_BYTES  30000

// Programming and debug: connect to UART USB, not OTG
#define LOGGER Serial

/* ============================================== *\
 * Constants
\* ============================================== */

#define __PROJECT_NAME__ "Thrust Stand";
#define __PROJECT_COMPILE__ "0.2";
#define SERIAL_DEBUG 1
#define SERIAL_SPEED 115200


extern const char EMPTY_STRING[];
extern const char NEW_LINE[];

extern const char PROJECT_NAME[] PROGMEM;
extern const char PROJECT_VERSION[] PROGMEM;
extern const char COMPILE_DATE[] PROGMEM;
extern const char COMPILE_TIME[] PROGMEM;

#define FST (const char *)F
#define PM (const __FlashStringHelper *)

/* ============================================== *\
 * EEPROM
\* ============================================== */

# define EEPROM_MAGIC 0x4242

#define EEPROM_SIZE sizeof(eeprom_data_t)

/* ============================================== *\
 * Debug
\* ============================================== */

#if SERIAL_DEBUG < 1
#define DEBUG_println(...) 
#define DEBUG_print(...) 
#define DEBUG_printf(...) 
#else
#define DEBUG_println(...) if (debugStream) {debugStream->println(__VA_ARGS__);}
#define DEBUG_print(...) if (debugStream) {debugStream->print(__VA_ARGS__);}
#define DEBUG_printf(...) if (debugStream) {debugStream->printf(__VA_ARGS__);}
#endif

extern Print* debugStream;
