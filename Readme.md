# ESP32 Thrust Stand

ESP32-based test stand firmware for measuring brushless motor and propeller performance. The project drives a standard RC ESC, samples thrust and torque through two HX711 load-cell amplifiers, reads electrical data, and prints live telemetry plus CSV-friendly test results over the serial port.

The firmware also includes an MLX90640 thermal imaging sensor for motor temperature monitoring and a browser-based web UI for thermal aiming, live telemetry, and test control.

The current firmware targets the `lolin32` PlatformIO environment and is built with the Arduino framework.

## What It Does

- Arms and controls a brushless ESC from an ESP32 PWM output
- Measures thrust and torque using two HX711 load cells
- Reads bus voltage, current, and electrical power with an INA226
- Measures RPM using an optical sensor input
- Reads a 32×24 thermal image from an MLX90640 sensor for motor temperature monitoring
- Computes max temperatu from the full thermal frame
- Hosts a browser-accessible web UI for thermal display, live telemetry, and test control
- Supports Wi-Fi station mode with automatic AP fallback for provisioning
- Supports manual throttle control, analog throttle control, and automated stepped test runs
- Prints live telemetry and CSV output for later analysis

## Hardware

The firmware is currently written around this sensor stack:

- ESP32 development board compatible with the `lolin32` PlatformIO board definition
- Brushless motor + ESC
- Two HX711 boards with load cells
- INA226 current and voltage sensor
- Optical RPM sensor
- MLX90640ESF-BAB thermal array sensor (32×24 pixels, I2C)
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
| MLX90640 I2C SDA / SCL | 16 / 17 (dedicated bus) |
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
4. Initializes the sensor stack (including the MLX90640 thermal sensor)
5. Mounts LittleFS and starts the Wi-Fi and web server
6. Prints the serial help menu

If a sensor is missing, the firmware logs an error and continues where possible. The thermal sensor is optional — if the MLX90640 is absent, the firmware marks thermal features unavailable but operates normally otherwise.

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
| `w` | Clear stored Wi-Fi credentials (next boot enters AP mode) |
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
- Thermal maximum temperature
- Potentiometer value

### CSV Results

After a completed or aborted test, the firmware prints a CSV block headed by:

```text
*** TEST RESULTS CSV DATA ***
```

The current row format is:

```text
Step,Throttle(%),Thrust(g),Torque(g·cm),Voltage(V),Current(A),Power(W),RPM,Thermal_Max(C),Thermal_Valid,Efficiency(g/W),LC_Samples,Sensor_Samples
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

### Upload Firmware

```bash
pio run -t upload
```

### Upload Web UI (LittleFS)

The browser UI is stored in the `data/` folder and served from LittleFS. After modifying `data/index.html`, upload it with:

```bash
pio run -t uploadfs
```

This must be done at least once before the web UI will load in a browser.

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

The project depends on these PlatformIO libraries:

- `HX711_ADC` — load cell amplifier driver
- `INA226` — voltage/current/power sensor
- `Adafruit MLX90640` — thermal array sensor driver
- `ESPAsyncWebServer` (esp32async fork) — async HTTP + WebSocket server
- `ArduinoJson` — JSON serialization for API and WebSocket
- `LittleFS` — filesystem for serving web UI assets

PlatformIO installs them automatically from `platformio.ini`.

## Wi-Fi and Web UI

### Wi-Fi Provisioning

On first boot (or after credentials are cleared), the firmware starts a Wi-Fi access point:

- **SSID**: `ThrustStand_XXXX` (where XXXX is derived from the chip ID)
- **Password**: `thruststand` (configurable in `config.h`)

Connect to this AP and open `http://192.168.4.1` in a browser to enter your Wi-Fi SSID and password. The credentials are saved to NVS and used on subsequent boots.

If station connection fails within 10 seconds, the firmware latches into AP fallback mode. To clear stored credentials, use the `w` serial command or the web UI.

### Web UI

The browser UI is a single-page app served from LittleFS at the root URL. It provides:

- **Thermal Panel** — Scaled-up live heatmap from the MLX90640 (32×24 pixels rendered via HTML5 Canvas with an ironbow palette), displaying the frame maximum temperature.
- **Telemetry Panel** — Live values for throttle, motor state, thrust, torque, voltage, current, power, RPM, and thermal maximum temperature, updated at ~10 Hz via WebSocket.
- **Test Control Panel** — Start, abort, and stop buttons, plus a manual throttle slider with heartbeat-backed safety timeout.
- **Test Results Panel** — Table of completed test step data with CSV download.
- **Network Settings Panel** — View current Wi-Fi mode and IP, enter new credentials, or clear stored credentials.

### Web Throttle Safety

Manual throttle control from the browser uses a heartbeat mechanism. If the browser disconnects or stops sending heartbeats for 5 seconds, the firmware automatically ramps the motor to zero.

### API Endpoints

| Method | Endpoint | Description |
| --- | --- | --- |
| GET | `/api/status` | Full status snapshot |
| POST | `/api/test/start` | Start a test |
| POST | `/api/test/abort` | Abort running test |
| POST | `/api/motor/stop` | Stop motor |
| POST | `/api/motor/throttle` | Set manual throttle (param: `value`) |
| POST | `/api/sensors/tare` | Tare load cells |
| GET | `/api/test/results` | JSON test results |
| GET | `/api/test/csv` | CSV download |
| GET | `/api/wifi/status` | Wi-Fi status |
| POST | `/api/wifi/credentials` | Save Wi-Fi credentials (params: `ssid`, `pass`) |
| POST | `/api/wifi/clear` | Clear stored credentials |

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

