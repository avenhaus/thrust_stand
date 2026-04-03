# ESP32 Thrust Stand Implementation Spec

## Purpose

This document is the execution spec for an AI coding agent tasked with implementing and hardening the ESP32 Thrust Stand firmware in this repository.

The repository already contains a partial implementation. The agent must treat the current codebase as the baseline, improve it incrementally, and avoid rewriting working subsystems without a concrete reason.

## Project Goal

Build a reliable ESP32-based thrust stand firmware that can:

- Control a brushless ESC safely
- Measure thrust, torque, voltage, current, power, RPM, and temperature
- Run repeatable stepped throttle tests
- Stream live telemetry over serial
- Export structured CSV results suitable for offline analysis
- Remain maintainable and calibratable for bench use

## Target Platform

- MCU: ESP32 compatible with PlatformIO board `lolin32`
- Framework: Arduino
- Build system: PlatformIO
- Serial speed: `115200`

## Repository Baseline

Current code organization:

- `src/main.cpp`: main loop, serial command handling, test sequencing
- `src/motor.cpp` and `include/motor.h`: ESC control and throttle ramping
- `src/sensors.cpp` and `include/sensors.h`: sensor initialization, live reads, and averaged statistics
- `src/analog.cpp` and `include/analog.h`: potentiometer filtering
- `src/rpm_sensor.cpp` and `include/rpm_sensor.h`: RPM sensing
- `include/config.h`: pin definitions and project-wide constants

The agent must preserve this general layout unless a change clearly improves clarity or correctness.

## Hardware Assumptions

The implementation must support the current hardware stack:

- ESP32 dev board using the `lolin32` environment
- Standard RC ESC controlled by PWM
- Two HX711 boards for thrust and torque measurement
- INA226 for voltage, current, and power
- Optical RPM sensor
- MAX31855 thermocouple interface
- Optional MLX90614 IR temperature sensor
- Potentiometer for analog throttle control
- Optional tare and start/stop buttons

Default pin mapping is defined in `include/config.h` and should remain the source of truth.

## Functional Requirements

### 1. Boot And Initialization

On startup the firmware must:

- Initialize serial logging
- Initialize status indicators
- Initialize and arm the ESC safely at minimum throttle
- Initialize all configured sensors
- Report initialization status clearly over serial
- Continue operating in a degraded mode where reasonable if optional sensors are absent

### 2. ESC Control

The ESC subsystem must:

- Support minimum and maximum pulse widths
- Use smooth throttle transitions instead of abrupt jumps by default where safety matters
- Expose immediate stop behavior and smooth stop behavior
- Support an ESC calibration sequence triggered by command
- Prevent invalid throttle values outside `0..100%`

### 3. Sensor Acquisition

The sensor subsystem must:

- Read thrust from load cell channel 1
- Read torque from load cell channel 2
- Read voltage, current, and power from INA226
- Read RPM from the optical sensor
- Read motor temperature from MAX31855
- Read ambient and object temperature from MLX90614 if present
- Tare the load cells on request
- Maintain rolling or per-step statistics for test summaries

### 4. Manual Operation

The firmware must support manual control via the serial terminal:

- Fixed throttle commands from `10%` to `100%`
- Stop command
- Tare command
- ESC calibration command
- Help command
- Analog throttle mode via potentiometer

Manual control must remain usable even when no automated test is running.

### 5. Automated Test Execution

The firmware must support an automated stepped throttle test that:

- Starts on user command
- Smoothly ramps to each target throttle step
- Waits for a configurable stabilization and sample window
- Collects averaged metrics for each step
- Stores per-step results in memory
- Stops cleanly after the last step
- Supports aborting mid-run
- Prints structured results after completion or abort

### 6. Serial Output

The firmware must emit:

- Human-readable live telemetry during operation
- A clear help menu on request
- Per-step test summaries
- CSV output with stable column ordering
- Error and fault messages that are explicit enough for bench debugging

## Non-Functional Requirements

The implementation must:

- Favor deterministic behavior over feature count
- Keep loop-time work lightweight enough for continuous sensor updates
- Avoid blocking delays except where hardware protocols explicitly require them, such as ESC arming or calibration steps
- Keep hardware constants centralized in `include/config.h` or well-defined calibration sections
- Degrade gracefully when optional sensors are unavailable
- Keep public interfaces simple and easy to test manually via serial

## Safety Requirements

Safety is a first-class requirement. The agent must not add behavior that increases the chance of an unsafe spin-up.

Mandatory safety properties:

- Boot at zero throttle
- Require explicit user action to start motor motion
- Ensure stop commands override normal operation
- Keep ESC calibration clearly marked as dangerous in serial output and documentation
- Avoid automatic test start on boot
- Avoid enabling analog throttle mode if the potentiometer is not near zero first
- Preserve smooth deceleration for test completion and abort where possible

## Architecture Requirements

### Main Loop Responsibilities

`src/main.cpp` should remain the coordinator for:

- calling `motor.run()`
- calling sensor update functions
- maintaining test state
- handling serial commands
- selecting manual versus analog versus automated control modes

### Motor Module Responsibilities

`src/motor.cpp` should remain responsible for:

- PWM timer and channel configuration
- ESC arming
- throttle to pulse-width conversion
- smooth acceleration and deceleration state management
- current motor state reporting

### Sensor Module Responsibilities

`src/sensors.cpp` should remain responsible for:

- hardware initialization for all sensors
- live sensor reads
- load-cell taring
- internal running totals and counters for test windows
- computation of averaged test statistics

### Config Responsibilities

`include/config.h` should remain responsible for:

- pin assignments
- serial speed
- project constants used across modules
- hardware-specific compile-time settings

## Implementation Priorities

The agent should work in this order unless a dependency forces a different sequence.

### Phase 1: Stabilize Core Control Path

- Verify boot sequence safety
- Verify ESC arm and stop behavior
- Verify serial command parsing
- Eliminate obvious logic errors in test state transitions

### Phase 2: Stabilize Measurement Path

- Verify all sensors initialize correctly
- Ensure missing optional sensors do not break the main loop
- Verify that per-step data collection resets and averages correctly
- Verify CSV output matches stored test data

### Phase 3: Improve Operator Workflow

- Improve serial help and status messages
- Make calibration and tare workflows clearer
- Ensure abort behavior preserves useful partial results
- Keep test parameters easy to modify

### Phase 4: Hardening And Documentation

- Remove dead or misleading code paths
- Reduce duplicated logic
- Add or update documentation where behavior changed
- Confirm build passes cleanly

## Explicit Deliverables

The agent is expected to produce:

- A buildable PlatformIO firmware project
- Clear serial command behavior for manual and automated operation
- Reliable per-step test result capture
- CSV output suitable for spreadsheet import
- Updated documentation reflecting the implemented behavior

## Acceptance Criteria

The task is complete only when all of the following are true:

1. `pio run` succeeds for the `lolin32` environment.
2. On boot, the firmware initializes serial, arms the ESC at minimum throttle, and attempts sensor setup without crashing.
3. Manual serial commands for throttle, stop, tare, help, and test start are functional.
4. The automated test can start, step through throttle levels, stop, and emit CSV output.
5. Sensor statistics are internally consistent enough that each completed step has sane values for samples, thrust, voltage, current, power, and RPM when the hardware is present.
6. Abort and stop behavior leaves the motor in a safe state.
7. Documentation matches actual implemented commands and output.

## Constraints For The Agent

The agent must:

- Prefer small, focused changes
- Preserve existing public behavior when it already works
- Fix root causes rather than masking symptoms
- Avoid introducing unnecessary new abstractions
- Avoid adding heavyweight dependencies without a clear benefit
- Avoid changing the PlatformIO environment unless required
- Avoid destructive git operations

## Validation Checklist

Before considering the work done, the agent should verify:

- Build success with PlatformIO
- Serial command list matches implementation
- Test-step indexing and bounds are correct
- CSV column order matches the printed data
- Optional MLX90614 absence does not crash the loop
- ESC stop behavior works both from idle and during an active test
- No obvious divide-by-zero or null-pointer issues remain in result generation

## Suggested Work Plan For The Agent

1. Read `platformio.ini`, `src/main.cpp`, `src/sensors.cpp`, `src/motor.cpp`, and `include/config.h`.
2. Build the firmware to identify compile or link issues.
3. Fix correctness and safety issues in the motor and test-state path first.
4. Fix data-collection and CSV consistency issues second.
5. Update documentation after code behavior is verified.

## Known Risks

The agent should pay particular attention to:

- unsafe throttle transitions
- incorrect step timing or skipped steps
- stale statistics leaking from one step into another
- mismatch between documented and printed CSV headers
- sensor initialization failures causing partial state corruption
- calibration constants being hard-coded and easy to misuse

## Out Of Scope

Unless explicitly requested later, the following are out of scope:

- designing the physical stand
- creating a desktop GUI
- adding cloud connectivity
- adding wireless control
- redesigning the entire firmware architecture
- implementing persistent storage for test results

## Definition Of Done

The project is considered implemented when the repository contains a stable, documented firmware flow for safe motor control and repeatable thrust-stand measurements, with manual control, automated stepped testing, and CSV export all functioning within the current PlatformIO project.
