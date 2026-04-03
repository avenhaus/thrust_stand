# ESP32 Thrust Stand

ESP32-based test stand firmware for measuring brushless motor and propeller performance. The project drives a standard RC ESC, samples thrust and torque through two HX711 load-cell amplifiers, reads electrical and temperature data, and prints live telemetry plus CSV-friendly test results over the serial port.

The current firmware targets the `lolin32` PlatformIO environment and is built with the Arduino framework.

## What It Does

- Arms and controls a brushless ESC from an ESP32 PWM output
- Measures thrust and torque using two HX711 load cells
- Reads bus voltage, current, and electrical power with an INA226
- Measures RPM using an optical sensor input
- Reads motor temperature with a MAX31855 thermocouple interface
- Optionally reads ambient and object temperature from an MLX90614
- Supports manual throttle control, analog throttle control, and automated stepped test runs
- Prints live telemetry and CSV output for later analysis

## Hardware

The firmware is currently written around this sensor stack:

- ESP32 development board compatible with the `lolin32` PlatformIO board definition
- Brushless motor + ESC
- Two HX711 boards with load cells
- INA226 current and voltage sensor
- Optical RPM sensor
- MAX31855 thermocouple interface
- MLX90614 IR temperature sensor
- Potentiometer for analog throttle control
- Optional tare and start/stop buttons

## Default Pin Mapping

The active pin configuration is defined in `include/config.h`.

| Function | GPIO |
| --- | ---: |
| ESC PWM output | 26 |
| RPM sensor input | 13 |
| Potentiometer input | 36 |
| HX711 #1 DOUT / SCK | 34 / 33 |
| HX711 #2 DOUT / SCK | 35 / 32 |
| INA226 I2C SDA / SCL | 21 / 22 |
| MAX31855 CS | 27 |
| Tare button | 0 |
| Start/stop button | 25 |
| Status LED | 5 |

If your wiring differs, update the values in `include/config.h` before flashing.

## Firmware Behavior

### Startup

On boot the firmware:

1. Initializes the status LED
2. Starts the serial logger at `115200`
3. Initializes and arms the ESC
4. Initializes the sensor stack
5. Prints the serial help menu

If a sensor is missing, the firmware usually logs an error and continues where possible instead of halting permanently.

### Manual Control

You can control throttle directly from the serial monitor:

- `1` through `9` set throttle to `10%` through `90%`
- `0` sets throttle to `100%`
- `SPACE` stops the motor
- `a` enables potentiometer-based throttle control, but only when the knob is near zero first

Throttle changes are ramped smoothly through the `MotorESC` class instead of jumping immediately.

### Automated Test Sweep

Press `s` in the serial monitor to start an automated sweep. During a test run, the firmware:

- Ramps between throttle steps smoothly
- Collects averaged sensor data for each step
- Prints a per-step summary to serial
- Prints CSV data when the run finishes or is aborted

Current firmware details:

- `total_steps = 20`
- `step_time_ms = 2000`
- `step_accel_time_ms = 1000`
- `decel_time_ms = 3000`
- The sweep currently begins recording at step `6`, which corresponds to roughly `30%` throttle, after the initial idle entry

These values are set in `src/main.cpp` and can be tuned for your stand, motor, and sensor response time.

## Serial Commands

Open a serial monitor at `115200` baud and send one of the following commands:

| Command | Action |
| --- | --- |
| `t` | Tare both load cells |
| `c` | Run ESC calibration sequence |
| `s` | Start test, or abort the running test |
| `p` | Print stored CSV results |
| `a` | Enable analog throttle control from the potentiometer |
| `SPACE` | Stop the motor |
| `h` | Print the help text again |
| `0`-`9` | Set throttle from `10%` to `100%` |

## Serial Output

The firmware prints two kinds of data:

### Live Telemetry

While running, the serial port continuously updates a single status line with fields such as:

- Current test step
- Throttle and motor state
- Thrust and torque
- Voltage, current, and power
- RPM
- Thermocouple temperature
- MLX ambient and object temperature
- Potentiometer value

### CSV Results

After a completed or aborted test, the firmware prints a CSV block headed by:

```text
*** TEST RESULTS CSV DATA ***
```

The current row format is:

```text
Step,Throttle(%),Thrust(g),Voltage(V),Current(A),Power(W),RPM,Efficiency(g/W),Samples
```

This output is intended to be copied into a spreadsheet or analysis script.

## Build And Flash

This project uses PlatformIO.

### Prerequisites

- Visual Studio Code with the PlatformIO extension, or PlatformIO Core installed locally
- USB connection to the ESP32 board
- A correctly powered sensor stack and ESC signal connection

### Build

```bash
pio run
```

### Upload

```bash
pio run -t upload
```

### Monitor Serial Output

```bash
pio device monitor -b 115200
```

The configured PlatformIO environment is:

```ini
[env:lolin32]
platform = espressif32
board = lolin32
framework = arduino
```

## Libraries

The project currently depends on these PlatformIO libraries:

- `HX711_ADC`
- `INA226`
- `Adafruit MAX31855 library`
- `Adafruit MLX90614 Library`
- `SimpleKalmanFilter`

PlatformIO installs them automatically from `platformio.ini`.

## Calibration Notes

- Load cell calibration factors are hard-coded in `src/sensors.cpp`
- INA226 shunt and offset calibration values are also hard-coded in `src/sensors.cpp`
- ESC calibration is available from the serial command `c`
- Always verify zero-load readings before running a propeller test

If you change the mechanical structure, shunt resistor, load cells, or sensor mounting, recalibrate before trusting the data.

## Safety

This project drives a real motor and propeller. Treat it like a rotating machine, not like a bench-top software demo.

- Remove propellers before ESC calibration and first-power testing
- Secure the stand before applying throttle
- Keep hands, tools, and cables clear of the propeller arc
- Use an adequate power supply and wiring gauge
- Do not trust early calibration values until they have been verified with known loads and external instruments
- Be ready to cut power immediately if the ESC or motor behaves unexpectedly

## Project Structure

```text
include/   Headers, pin definitions, and interfaces
src/       Firmware implementation
lib/       Private libraries if needed later
test/      PlatformIO test area
```

## Current Gaps

The codebase already includes the main control loop and sensor pipeline, but a few areas are still rough:

- The README does not yet document the mechanical stand design or calibration workflow in detail
- Button inputs are configured in firmware, but the main control path is currently serial-driven
- Some commented notes in the source indicate planned cleanup and sensor refinements

## Next Improvements

Useful follow-up work for the project:

1. Add a wiring diagram and photos of the stand
2. Document the exact calibration procedure for both load cells and the INA226
3. Save test results to SD card or SPIFFS instead of serial only
4. Add a structured host-side script to capture and plot CSV output automatically

