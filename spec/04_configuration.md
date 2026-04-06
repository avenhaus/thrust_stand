# Configuration UX And Profile Management Spec

## Purpose

This document is the execution spec for an AI coding agent tasked with implementing and hardening the configuration user experience for the ESP32 Thrust Stand firmware and web UI.

The goal is to make test configuration safe, clear, and persistent, while preserving the existing motor-control and safety behavior.

## Relationship To Existing Specs

This spec extends:

- `spec/01_thrust_stand.md` (core firmware behavior)
- `spec/02_thermal_image_and_web.md` (thermal + browser control architecture)

This document focuses specifically on operator configuration workflows and the data model behind them.

## Project Goal

Provide a robust configuration workflow so an operator can:

- inspect current active test settings quickly
- update settings with clear units and validation feedback
- reset settings to known defaults
- persist settings across reboot using NVS
- run tests with predictable behavior, including an explicit idle baseline step

## Scope

This spec covers:

- test configuration model and constraints
- web UX for editing and reviewing configuration
- API contract for loading, validating, saving, and resetting configuration
- persistence behavior and defaults
- interaction rules while a test is running
- CSV and test-result expectations related to configured behavior

This spec does not cover:

- cloud sync or remote profile sharing
- authentication and user roles
- mobile-specific UX redesign

## Configuration Model

The active test configuration must include at minimum:

- `totalSteps` (integer)
- `stepTimeMs` (integer)
- `stepAccelTimeMs` (integer)
- `decelTimeMs` (integer)
- `minThrottlePercent` (float)
- `maxThrottlePercent` (float)
- `maxTempLimitCelsius` (float)

The model should remain centralized in the firmware config structure and exposed to web handlers through shared state.

## Required Runtime Behavior

### 1. Idle Baseline Step

Test step `0` must represent idle baseline measurement:

- motor commanded to `0%` throttle
- capture baseline thrust, torque, electrical values, and thermal values
- included in test results and CSV output when valid samples exist

Test step `1` is the first powered step and must start at the configured first throttle value.

### 2. Powered Steps Mapping

Powered steps `1..totalSteps` must map linearly from `minThrottlePercent` to `maxThrottlePercent`.

For `totalSteps > 1`, endpoint mapping must be exact:

- step `1` => `minThrottlePercent`
- step `totalSteps` => `maxThrottlePercent`

For `totalSteps == 1`, step `1` should use `minThrottlePercent`.

### 3. Save And Apply Semantics

When configuration is saved successfully:

- the active in-memory configuration updates immediately
- the configuration is persisted to NVS
- the UI receives explicit success/failure feedback

### 4. Running-Test Behavior

While a test is active:

- configuration edits may be accepted for next run, but must not silently alter current step timing/throttle progression in-flight
- or be rejected with clear message if runtime mutability is disabled

Either behavior is acceptable, but must be explicit and consistent.

## UX Requirements

### 1. Test Control Integration

Configuration controls should be directly available in the test control workflow, not hidden behind unrelated panels.

At minimum include:

- editable input fields for all configuration values
- `Save`, `Reset Defaults`, and `Refresh` actions
- concise status message area (`ok`/`error`)

### 2. Units And Labels

Each field label must include units where applicable:

- `(ms)` for timing
- `(%)` for throttle
- `(°C)` for thermal limit

No ambiguous unlabeled numeric fields.

### 3. Layout And Readability

Configuration rows should be scan-friendly and compact.

Recommended pattern:

- one row per parameter
- label and input on one line
- consistent spacing and alignment

### 4. Operator Feedback

UI must provide immediate textual feedback for:

- successful load/save/reset
- validation failures
- communication errors

Error text should mention the first actionable reason when possible.

## Validation Requirements

Firmware-side validation is mandatory; UI-side validation is additive.

Minimum constraints:

- `totalSteps`: `1..100`
- `stepTimeMs`: `>= 100`
- `stepAccelTimeMs`: `> 0`
- `decelTimeMs`: `> 0`
- `minThrottlePercent`: `>= 0`
- `maxThrottlePercent`: `<= 100`
- `minThrottlePercent < maxThrottlePercent`
- `maxTempLimitCelsius > 0`

Rejected values must never be persisted.

## Persistence Requirements

Configuration must be stored in Preferences/NVS under the existing thrust stand namespace using stable keys.

Required behavior:

- load on boot before web UI starts
- save only after validation passes
- reset action writes firmware defaults back to NVS
- missing keys fall back to defaults safely

## API Requirements

The configuration API must expose:

- `GET /api/test/config`: returns full active config
- `POST /api/test/config`: validate and save provided config
- `POST /api/test/config/reset`: restore defaults and persist

Response expectations:

- stable JSON field names matching UI contract
- boolean `ok` plus human-readable `message` or `error`
- HTTP status codes aligned with outcome (`200` success, `400` invalid input, `409` invalid state when applicable)

## Results And CSV Requirements

Result model and CSV export must reflect configuration-driven execution:

- include step `0` idle baseline when sampled data exists
- include all executed powered steps through completion or abort
- include thermal validity and thermal-abort flags
- keep CSV column order stable

CSV download filename should be deterministic and descriptive (for example `thrust_data.csv`).

## Safety Requirements

Configuration UX must not weaken motor safety:

- no automatic motor start on page load or config refresh
- save/reset actions must not command throttle changes on their own
- stop/abort authority must remain unchanged
- invalid configs must fail closed

## Agent Implementation Priorities

Implement in this order:

### Phase 1: Data Model And Validation

- confirm single source of truth for config structure
- enforce validation at API boundary
- ensure defaults are complete and consistent

### Phase 2: Persistence And API Stability

- finalize NVS load/save/reset behavior
- lock response schema and status codes
- verify cold-boot and reboot persistence

### Phase 3: UX Integration

- place controls in test control workflow
- implement one-line label/input layout
- improve status/error messaging

### Phase 4: Runtime Semantics And Results

- verify idle step 0 capture behavior
- verify step mapping 1..N min->max endpoints
- verify test results and CSV include baseline row

## Acceptance Criteria

This spec is satisfied only when all are true:

1. Operator can load, edit, save, reset, and refresh configuration from the web UI.
2. UI labels include units and provide clear success/error feedback.
3. Invalid values are rejected and not persisted.
4. Configuration persists across reboot via NVS.
5. Test execution uses step `0` as idle baseline and step `1` as first powered step.
6. Powered step mapping honors exact min/max endpoints.
7. Test results and CSV include idle baseline row when sampled.
8. Safety behavior (stop/abort/manual control bounds) remains intact.
9. `pio run` succeeds for target environment after changes.

## Definition Of Done

The configuration UX is considered complete when an operator can confidently configure and run repeatable tests from the web interface, with persistent validated settings, explicit baseline measurement, and predictable results export, without introducing unsafe motor behavior.

