# MLX90640 Thermal Imaging And Web Control Spec

## Purpose

This document is the execution spec for an AI coding agent tasked with extending the existing ESP32 Thrust Stand firmware with:

- an MLX90640ESF-BAB thermal imaging sensor
- motor max-temperature monitoring based on the thermal image
- an ESP32-hosted web page for live thermal aiming and live test control

The repository already contains a working baseline for motor control, stepped testing, and wired sensor acquisition. The agent must build on that baseline instead of replacing it.

## Project Goal

Add a thermal imaging path and browser-based operator interface to the current thrust stand so that an operator can:

- aim the thermal sensor at the motor using a scaled-up live thermal image
- monitor the motor's hottest area during a test
- view live thrust-stand telemetry in a browser
- start, stop, abort, and monitor motor tests from the web page
- keep the existing firmware safety behavior intact

## Relationship To Existing Firmware

This spec extends the baseline firmware behavior defined in `spec/01_thrust_stand.md`.

The existing code already provides:

- ESC control and throttle ramping
- stepped motor test execution
- serial command handling
- live electrical and mechanical telemetry
- CSV result printing

The new implementation must reuse the existing control and safety paths wherever possible. Web commands must call the same underlying actions as serial commands rather than duplicating separate motor-control logic.

## Target Platform

- MCU: ESP32 compatible with PlatformIO board `lolin32`
- Framework: Arduino
- Build system: PlatformIO
- Existing serial monitor speed: `115200`
- Existing I2C bus (Wire): GPIO `21` SDA and GPIO `22` SCL — used by INA226
- Dedicated I2C bus (Wire1): GPIO `16` SDA and GPIO `17` SCL — used by MLX90640

## New Hardware Scope

The new thermal feature is based on:

- MLX90640ESF-BAB thermal array sensor
- `32 x 24` pixel thermal frame
- dedicated I2C bus on GPIO `16` SDA and GPIO `17` SCL (ESP32 Wire1)

The MLX90640 uses a separate I2C bus from the INA226 to avoid bus contention. At 8 Hz refresh rate, the MLX90640 transfers ~26 KB/s which would consume over half the bandwidth of a shared 400 kHz bus. Running on its own bus allows independent 1 MHz (FM+) operation and eliminates blocking interference with electrical telemetry sampling.

The MLX90640 replaces the existing MAX31855 thermocouple and MLX90614 IR temperature sensor. Those sensors and their dependencies should be removed from the firmware.

The design assumes the MLX90640 will coexist with:

- INA226 current and voltage sensor (on the primary I2C bus, Wire)
- the rest of the existing thrust stand hardware

## Primary Use Cases

### 1. Thermal Aiming

The operator opens the ESP32-hosted web page, sees a scaled-up thermal view, and physically adjusts sensor position until the motor body is centered in the intended target region.

### 2. Live Temperature Monitoring

During motor operation, the web page shows the current max motor temperature derived from the thermal sensor and updates that value live enough to catch heating trends.

### 3. Browser-Based Test Control

The operator can use the web page to:

- start a stepped test
- stop or abort a running test
- tare sensors
- set manual throttle when allowed
- view current test step and live telemetry

### 4. Post-Test Review

After a test, the operator can still inspect the latest thermal frame, the final max temperature, and the latest test results available from the firmware.

## Core Design Principles

The implementation must follow these principles:

- Safety first: thermal and web features must never weaken stop behavior or safe motor startup behavior
- Reuse existing logic: browser controls must feed the same internal actions used by serial commands
- Degrade gracefully: if Wi-Fi or thermal streaming is slow, motor control must still remain reliable
- Keep the loop responsive: thermal imaging and networking must not block motor updates
- Make aiming practical: the web UI should prioritize a clear scaled image over high frame rate

## Functional Requirements

### 1. MLX90640 Initialization

The firmware must:

- initialize the MLX90640 on boot
- report success or failure clearly over serial and through web status
- continue operating if the thermal sensor is absent, while marking thermal features unavailable
- keep existing non-thermal sensors functional even if the MLX90640 fails to initialize

### 2. Thermal Frame Acquisition

The firmware must:

- read thermal frames from the MLX90640 at a practical rate for ESP32 operation
- store the latest complete frame in memory
- track frame timing and validity
- avoid blocking the main control loop for long periods during frame acquisition
- expose the latest frame to the web subsystem

The implementation may reduce frame rate to protect stability and motor responsiveness.

### 3. Motor Max-Temperature Monitoring

The thermal feature must report motor temperature and ambient temperature metrics that are useful and stable.

The firmware must:

- compute the maximum temperature across the entire thermal frame
- expose current frame maximum temperature
- expose ambient temperature from the MLX90640 sensor
- preserve the latest maximum motor temperature during a test step for display and logging
- preserve the latest ambient temperature during a test step for display and logging
- provide a clear and stable indication of motor hotspot temperature and ambient reference temperature

### 4. Thermal Aiming View

The web page must show a scaled-up live thermal image that:

- is visually large enough to aim the sensor by eye
- updates often enough to be useful for positioning
- includes a visible color map or contrast range
- identifies the hottest region clearly
- uses smooth bilinear interpolation when scaling the 32×24 pixel thermal array to reduce blockiness

The aiming view does not need to behave like smooth video. A lower but stable update rate is acceptable. Smooth interpolation is handled entirely by the browser on the client side using canvas image smoothing, imposing no additional load on the ESP32.

### 5. Browser-Based Motor And Test Control

The web page must support:

- starting a stepped test
- aborting a running test
- stopping the motor immediately or through the existing safe path as appropriate
- taring load cells
- showing whether the motor is idle, accelerating, running, or decelerating
- showing current throttle and allowing manual throttle changes when no automated test is active

The implementation must prevent conflicting commands from web UI and serial UI from producing undefined behavior.

The implementation must also define browser-control ownership and disconnect behavior:

- manual web throttle must have an explicit timeout or heartbeat policy
- if the browser disconnects while manual web throttle is active, the firmware must fall back to a defined safe behavior
- the recommended default is to disable manual web throttle and ramp the motor to zero after a short timeout
- a running automated stepped test may continue after browser disconnect, but control state and test state must remain coherent
- reconnecting the browser must not silently reapply stale manual throttle commands

### 6. Live Telemetry In Browser

The web page must display current live values for at least:

- throttle
- motor state
- thrust
- torque
- voltage
- current
- power
- RPM
- thermal maximum temperature
- thermal ambient temperature
- current test step and total steps

The page should also surface whether thermal streaming is healthy or stale.

### 7. Test Status And Results Access

The browser must be able to view test progress while the test is running.

The firmware must expose:

- whether a test is active
- current test step
- latest step statistics when available
- final result data after completion or abort
- CSV-compatible test result access, either directly or through an endpoint that mirrors serial output

Thermal metrics must be part of the result model, not only the live view. At minimum the implementation must expose:

- current maximum temperature in live telemetry
- current ambient temperature in live telemetry
- per-step maximum temperature for completed test steps
- per-step ambient temperature for completed test steps
- final run maximum temperature
- final run ambient temperature
- clear indication of whether a thermal value came from a valid frame or stale data

## Web Architecture Requirements

### Wi-Fi Mode

The firmware must support station mode as the primary networking path and fall back to a local configuration AP using a deterministic, latched policy.

The boot-time network sequence must be:

1. Load Wi-Fi credentials from NVM (ESP32 Preferences/NVS).
2. If no credentials are stored, start a configuration AP immediately.
3. If credentials exist, attempt one bounded station-connect window (recommended `10 s`, configurable in `config.h`).
4. If the station attempt fails to obtain an IP within the window, start the configuration AP and latch into AP mode.
5. Once latched into AP mode, remain there until the operator saves new credentials or explicitly requests a reconnect through the web UI or serial command.

After a successful station session, if the link drops at runtime:

1. Allow one bounded reconnect window (recommended `15 s`, configurable in `config.h`).
2. If the reconnect window expires without regaining an IP, transition to the configuration AP and latch.
3. Do not oscillate between station retries and AP mode.

The configuration AP must:

- use a deterministic SSID derived from a project prefix and the chip ID, e.g. `ThrustStand_XXXX`
- use a default password defined in `config.h` (minimum 8 characters)
- serve the same web UI used in station mode, plus a Wi-Fi settings panel for credential entry
- accept new SSID and password from the operator, validate non-empty SSID, persist them in NVM, and allow the operator to trigger a station reconnect attempt

The firmware must expose the current network mode (station, AP fallback, connecting, disconnected), the last failure reason, and the AP SSID/IP through shared status accessible to both serial output and the web API.

### Wi-Fi Credential Management

Wi-Fi credentials must be stored in NVM using ESP32 Preferences/NVS under a dedicated namespace.

The implementation must:

- store SSID and password as separate entries
- load credentials on boot before attempting station connect
- allow credentials to be updated from the web UI in both station mode and AP fallback mode
- allow credentials to be cleared from the web UI or the serial command `w` (mnemonic: Wi-Fi), causing the next boot to enter AP mode
- accept empty passwords for open networks; require minimum 8 characters for non-empty passwords
- never expose the stored password in API responses or serial output

### Web Protocol Split

The implementation should use:

- REST-style endpoints for commands and snapshot reads
- WebSocket for live telemetry and thermal updates

This keeps control flows simple while allowing the browser to receive push updates efficiently.

### Asset Delivery

The web UI may be served by:

- embedded strings in firmware, or
- on-device filesystem assets such as SPIFFS or LittleFS

If asset storage requires a larger partition layout than the current minimal SPIFFS configuration, the partition change is in scope for this feature.

## Thermal Data Transport Requirements

The implementation must choose a practical transport format for the browser thermal image.

Acceptable approaches include:

- sending a compact array of temperatures and letting the browser render the heatmap with smooth interpolation
- sending preprocessed low-rate image frames suitable for browser display

The chosen approach must prioritize ESP32 stability and predictable memory use.

The implementation must not assume the ESP32 can serve high-frame-rate video. A modest live rate optimized for aiming is sufficient. The browser will apply smooth bilinear scaling to the small thermal array, improving visual quality without burdening the firmware.

The recommended baseline target is:

- thermal frame acquisition target of roughly `8 Hz` during active motor testing
- optional higher idle-rate aiming updates when the motor is not running, if headroom permits
- browser display updates that may drop frames rather than block control or sensor processing

## UI Requirements

The web page must include these areas:

### 1. Thermal Panel

- scaled thermal image
- current palette or range information
- current maximum temperature
- current ambient temperature
- thermal stream status

### 2. Live Telemetry Panel

- current throttle
- motor state
- thrust and torque
- voltage, current, and power
- RPM
- thermal maximum temperature
- compact network status indicator (mode and IP)

### 3. Test Control Panel

- start test
- abort test
- stop motor
- tare sensors
- optional manual throttle controls

### 4. Test Status Panel

- test running or idle state
- current step
- total steps
- latest step results
- final results or CSV access after completion

### 5. Network Settings Panel

- current network mode (station connected, AP fallback, connecting)
- station SSID and IP when connected
- AP SSID and IP when in fallback mode
- last connection failure reason
- form to enter new SSID and password
- button to save credentials and trigger station reconnect
- button to clear stored credentials

This panel must be functional in AP fallback mode so the operator can provision Wi-Fi without serial access.

The UI should be designed for operator clarity rather than visual complexity.

## API Requirements

The exact endpoint names may differ, but the browser-accessible interface must cover these behaviors:

- fetch current status snapshot
- fetch or subscribe to live telemetry
- fetch or subscribe to latest thermal frame
- start test
- abort test
- stop motor
- tare sensors
- set manual throttle when safe
- fetch test results or CSV output
- fetch current Wi-Fi status, mode, and connection details
- submit new Wi-Fi credentials and trigger station reconnect
- clear stored Wi-Fi credentials

The API responses must be structured and stable enough that the browser UI can be implemented without scraping serial-like text.

## Architecture Constraints

### Main Loop

`src/main.cpp` should remain the central coordinator for:

- calling `motor.run()`
- calling sensor updates
- maintaining test state
- processing serial commands
- invoking web-service processing or coordinating with a web task

The implementing agent should avoid turning the whole codebase inside out just to add web support.

### Sensor Layer

The thermal sensor should be integrated using the same general pattern already used by the current sensor stack:

- initialize during the sensor initialization phase
- update during the sensor update phase
- publish state through clear shared data structures or getters

### Motor Control Layer

The existing motor control module should remain the authority for:

- throttle commands
- ESC state
- acceleration and deceleration
- safe stop behavior

Web code must not bypass this layer.

## Performance Requirements

The implementation must respect the practical limits of a non-PSRAM ESP32-class board.

It must:

- keep the motor control path responsive while Wi-Fi is active
- avoid unbounded dynamic memory growth
- use fixed-size buffers or otherwise predictable memory use where practical
- keep thermal update rate conservative enough to avoid bus or CPU saturation
- handle temporary browser disconnects without destabilizing the running test

The implementation should treat the following as concrete operating targets unless hardware testing proves a different limit is required:

- thermal acquisition target: `8 Hz` during active tests
- motor-control and main-loop work should not be blocked by thermal or web work for more than about `10 ms` at a time
- maintain a measurable free-heap safety margin during active Wi-Fi and thermal streaming, with the exact threshold documented if tuned during implementation
- prefer dropping or coalescing browser updates over delaying motor or sensor control logic

The implementation may reduce thermal streaming frequency during active tests if needed to preserve control stability.

## Safety Requirements

This feature set must preserve and strengthen existing safety behavior.

Mandatory safety properties:

- zero-throttle boot behavior remains unchanged
- the motor does not start automatically when Wi-Fi or the browser connects
- stop and abort commands remain authoritative
- browser commands cannot bypass throttle bounds or test-state protections
- loss of the web client does not cause unsafe motor behavior
- thermal feature failure must not trigger motor behavior changes on its own

## Error Handling Requirements

The system must detect and report at least these conditions:

- MLX90640 initialization failure
- thermal frame read failure or timeout
- stale thermal frame data
- Wi-Fi credentials missing, triggering AP mode
- station connect failed within bounded window, triggering AP fallback
- station link lost at runtime, reconnect window active or exhausted
- current network mode transitions (station, AP fallback, connecting)
- browser not connected
- test command rejected due to invalid current state
- manual web throttle timeout or disconnect fallback activation

The firmware should report failures in a way that is usable both from serial and from the web UI.

## Deliverables

The implementing agent is expected to produce:

- buildable firmware with MLX90640 integration
- browser-accessible thermal aiming page
- browser-based motor and test control page or combined single-page UI
- stable API surface for live telemetry and control
- network and web UX should run on separate core to not affect motor control etc.
- updated documentation for setup and usage

## Acceptance Criteria

The task is complete only when all of the following are true:

1. `pio run` succeeds for the target PlatformIO environment after dependency and partition updates.
2. The firmware boots safely and the existing motor-control behavior still works without requiring the browser UI.
3. The MLX90640 initializes successfully when connected and fails gracefully when absent.
4. The browser can display a scaled live thermal image that is good enough to help aim the sensor at the motor.
5. The firmware reports a maximum motor temperature derived from the entire thermal frame.
6. The browser can display live thermal frames with the current maximum temperature clearly visible.
7. The browser can start, abort, stop, and monitor a motor test without bypassing existing safety logic.
8. Live browser telemetry includes motor state, throttle, key test values, and thermal maximum temperature.
9. Loss of browser connection or temporary Wi-Fi instability does not break a running test or leave the motor in an unsafe state, and manual web throttle falls back according to the documented timeout policy.
10. The firmware boots into AP provisioning mode when no Wi-Fi credentials are stored, attempts station mode when credentials exist, and latches into AP fallback after a bounded failed connect window without oscillating between modes.
11. Wi-Fi credentials can be entered, saved to NVM, and used on reboot to reconnect in station mode without operator re-entry.
12. Thermal metrics are available both live and in test-result data for completed steps and final run summary.
13. The added thermal and web features do not introduce unacceptable loop blocking or obvious memory instability.
14. Documentation matches the implemented browser workflow, network provisioning, thermal behavior, disconnect behavior, and limitations.

## Suggested File-Level Approach

The exact filenames may vary, but the recommended implementation shape is:

- keep `src/main.cpp` as the system coordinator
- extend `src/sensors.cpp` and `include/sensors.h` with thermal support
- add a dedicated thermal module if the MLX90640 logic becomes too large for `sensors.cpp`
- add a dedicated web module for Wi-Fi, HTTP, and WebSocket behavior
- extend `include/config.h` with thermal, Wi-Fi (SSID prefix, default AP password, connect timeouts), and ROI defaults
- update `platformio.ini` with required dependencies and any filesystem or partition changes

## Implementation Priorities

The implementing agent should work in this order:

### Phase 1. Thermal Sensor Integration

- add MLX90640 dependency and initialization
- verify stable frame acquisition
- expose latest frame and derived max-temperature metrics
- define default ROI and ROI state model

### Phase 2. Shared Data And Control Hooks

- expose motor, test, and thermal state to shared interfaces
- unify serial and web command paths where needed
- add serial command `w` to clear stored Wi-Fi credentials
- add manual web-control timeout and disconnect fallback behavior

### Phase 3. Web Transport Layer

- add Wi-Fi setup with station-first connect and deterministic AP fallback
- add NVM-backed credential storage and AP-hosted provisioning flow
- add HTTP and WebSocket endpoints including Wi-Fi status and credential management
- verify browser-visible telemetry, commands, and network provisioning

### Phase 4. Browser UI

- implement scaled thermal aiming view
- implement ROI editing or adjustment workflow
- implement telemetry and control panels
- show test status and result access

### Phase 5. Hardening

- measure runtime stability
- tune frame rate and update intervals
- verify disconnect fallback and stale-frame handling
- improve failure handling and documentation

## Known Risks

The implementing agent should watch closely for:

- thermal frame reads that block other time-sensitive work (mitigated by dedicated I2C bus)
- Wi-Fi or web serving load affecting motor responsiveness
- heap pressure from web assets, frame buffers, and live updates
- mismatched command behavior between serial and browser control paths
- browser disconnect leaving manual control state ambiguous if timeout behavior is not implemented cleanly
- mode flapping between station retries and AP fallback under marginal Wi-Fi if the latched fallback policy is not enforced
- NVM wear from frequent credential writes if the provisioning UI does not debounce or validate before persisting
- no security mechanisms for web control

## Out Of Scope

Unless explicitly requested later, the following are out of scope:

- multi-user browser sessions
- cloud upload or remote internet access
- mobile apps
- full thermal video streaming at camera-like frame rates
- redesigning the whole firmware architecture
- replacing the existing serial interface

## Definition Of Done

The feature is considered complete when the repository contains a stable extension of the thrust-stand firmware that uses an MLX90640ESF-BAB to provide a useful thermal aiming and motor max-temperature workflow, plus an ESP32-hosted web interface for live viewing, live telemetry, safe motor-test control, and self-service Wi-Fi provisioning with deterministic station/AP fallback behavior.
