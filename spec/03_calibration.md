# Calibration UX And Sensor Calibration Spec

## Purpose

This document is the execution spec for an AI coding agent tasked with implementing and hardening the calibration user experience for the ESP32 Thrust Stand firmware and web UI.

The project already includes calibration persistence and calibration API endpoints. The agent must improve and stabilize the workflow, not replace working control paths.

## Relationship To Existing Specs

This spec extends:

- spec/01_thrust_stand.md (core motor and sensor behavior)
- spec/02_thermal_image_and_web.md (web architecture and live telemetry)
- spec/03_configuration.md (test configuration UX and persistence behavior)

This document focuses specifically on sensor calibration UX, validation, persistence semantics, and safe operator workflows.

## Project Goal

Provide a robust calibration workflow so an operator can:

- inspect active calibration values quickly
- run guided calibration steps safely
- update calibration values with validation and clear units
- persist calibration across reboot via NVS
- revert to known defaults without ambiguity

## Scope

This spec covers:

- calibration data model and constraints
- web UX for calibration read/update/reset
- INA226 scan workflow and operator guidance
- API contracts for calibration and scan operations
- calibration persistence and boot-load behavior
- safety behavior during calibration actions

This spec does not cover:

- cloud storage or remote profile sync
- authentication or role-based permissions
- closed-loop auto-calibration algorithms

## Current Baseline (Must Be Preserved)

The current codebase already has:

- calibration storage in NVS as a packed calibration struct
- calibration apply path through sensors module
- web endpoints for get/set/reset calibration
- INA226 scan endpoint returning recommended zero offset
- a calibration panel in data/index.html

The agent should preserve API route names and existing field names unless a backward-compatible migration is explicitly implemented.

## Calibration Model

The active calibration model must include these fields:

- lc_calibration_value_1 (thrust load-cell calibration factor)
- lc_calibration_value_2 (torque load-cell calibration factor)
- shunt (ohms)
- current_LSB_mA (milliamps)
- current_zero_offset_mA (milliamps)
- INA266_max_current (amps)
- bus_V_scaling_e4 (unitless integer scale in 1e-4)

The single source of truth remains calibration_t in include/config.h with runtime ownership in sensors.cpp.

## Required Runtime Behavior

### 1. Boot Load And Apply

On boot, firmware must:

- load calibration from NVS when present and structurally valid
- fall back to CALIBRATION_DEFAULTS on missing or invalid stored blob
- apply the effective calibration to HX711 and INA226 before normal operation
- report the source (NVS or defaults) through debug output

### 2. Set Calibration Semantics

When POST /api/calibration succeeds:

- validate all required fields first
- persist full calibration atomically as one struct payload
- apply the just-saved calibration immediately
- return explicit success status and message

Rejected payloads must not partially update hardware state or NVS.

### 3. Reset Calibration Semantics

When POST /api/calibration/reset succeeds:

- restore CALIBRATION_DEFAULTS
- persist defaults to NVS
- apply defaults immediately
- return explicit success status and message

### 4. INA226 Scan Semantics

When POST /api/calibration/ina226-scan is called:

- execute a bounded measurement sequence while motor is expected off
- return averaged bus voltage and averaged current measurement
- return recommended_zero_offset for operator use
- never alter saved calibration values automatically

The scan is advisory output only; applying values requires explicit save.

## UX Requirements

### 1. Calibration Panel Structure

The calibration UI should remain in a dedicated panel and include:

- current active values section
- editable input section for all calibration fields
- action row for Save and Reset Defaults
- INA226 scan section with last scan results
- status message area for validation and transport feedback

### 2. Units And Labels

Each field label must include clear units where applicable:

- shunt (Ohm)
- current_LSB_mA (mA)
- current_zero_offset_mA (mA)
- INA266_max_current (A)
- bus_V_scaling_e4 (1e-4)

Load-cell factors should be clearly labeled as calibration factors (unitless).

### 3. Operator Guidance

UI guidance text must include, at minimum:

- tare before known-load checks
- motor off requirement for INA226 zero-offset scan
- save-after-edit requirement
- reset-defaults behavior explanation

### 4. Feedback Behavior

UI must present clear and immediate textual feedback for:

- load success/failure
- validation errors
- save success/failure
- reset success/failure
- scan success/failure

Messages should be actionable and avoid generic unknown error text when a specific reason is available.

## Validation Requirements

Firmware-side validation is mandatory; UI-side checks are additive.

Minimum required constraints:

- lc_calibration_value_1 > 0
- lc_calibration_value_2 > 0
- shunt > 0
- current_LSB_mA > 0
- INA266_max_current > 0
- bus_V_scaling_e4 > 0

current_zero_offset_mA may be positive or negative.

Additional recommended guardrails (firmware or UI):

- reject NaN or infinity for all float fields
- reject unreasonably extreme magnitudes that indicate parse errors
- return first-failure reason in error payload

## API Contract Requirements

### 1. GET /api/calibration

Returns full active calibration object with stable field names matching the model.

Success response:

- HTTP 200
- JSON object containing all calibration fields

### 2. POST /api/calibration

Accepts full calibration object and performs validate -> persist -> apply.

Success response:

- HTTP 200
- JSON: { ok: true, message: string }

Validation failure response:

- HTTP 400
- JSON: { ok: false, error: string }

### 3. POST /api/calibration/reset

Restores defaults, persists, and applies.

Success response:

- HTTP 200
- JSON: { ok: true, message: string }

### 4. POST /api/calibration/ina226-scan

Runs scan and returns advisory values.

Success response:

- HTTP 200
- JSON with: ok, avg_bus_voltage, avg_current_mA, recommended_zero_offset, message

Operational failure response should use HTTP 409 or 500 with explicit reason when scan cannot run safely.

## Persistence Requirements

Calibration persistence must use the existing namespace/key strategy:

- namespace: thrust_stand
- key: calibration

Required behavior:

- write full struct blob only after validation passes
- ignore malformed blob length on read and use defaults
- keep struct compatibility stable when possible

If struct layout changes in future, migration strategy must be explicit (versioning or compatibility path).

## Safety Requirements

Calibration UX must not weaken motor safety:

- save/reset/scan actions must not command motor throttle changes
- scan workflow must clearly require motor off state
- if a scan precondition fails (motor running/test active), return explicit error
- calibration changes during active automated test must be either blocked or clearly deferred, never silently applied mid-step

## Agent Implementation Priorities

Implement in this order:

### Phase 1: Contract And Validation Hardening

- audit calibration API input handling
- enforce complete required-field and numeric validation
- ensure deterministic error responses

### Phase 2: Safe Runtime Semantics

- enforce motor/test-state preconditions for scan and high-risk operations
- define and implement behavior for calibration writes during active test

### Phase 3: UX Clarity

- improve labels, units, and guidance copy
- surface clear save/reset/scan statuses
- keep panel scan-friendly and compact

### Phase 4: Persistence Robustness

- verify reboot persistence and malformed-read fallback
- verify defaults reset path always round-trips through NVS and apply path

## Acceptance Criteria

This spec is satisfied only when all are true:

1. Operator can load, edit, save, and reset calibration through web UI.
2. Operator can run INA226 scan and receive recommended offset without auto-applying it.
3. Invalid calibration payloads are rejected and not persisted.
4. Calibration persists across reboot and is applied on boot.
5. Missing/invalid NVS calibration data safely falls back to defaults.
6. Calibration actions do not trigger motor start or unsafe throttle behavior.
7. Calibration field names and endpoint routes remain stable.
8. Build succeeds with pio run for target environment.

## Definition Of Done

Calibration UX is complete when an operator can reliably calibrate thrust, torque, and INA226-related measurements through the web interface with clear guidance, predictable persistence, explicit validation feedback, and no regression in motor safety behavior.
