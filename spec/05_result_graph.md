# Result Graph Panel Specification

## Goal
Add a web panel that visualizes recorded test-step results as a graph.

- X axis: throttle value in percent.
- Y axis series:
	- thrust (g)
	- current (A)
	- efficiency (g/W)
	- temperature (thermal max, C)

This spec is implementation-ready for an AI coding agent and aligned with existing firmware/web behavior.

## Scope
In scope:

- Add a dedicated Result Graph panel in the web UI.
- Render all required series using test results from `/api/test/results`.
- Keep existing Test Results table and CSV behavior unchanged.
- Update graph whenever result data refreshes.

Out of scope:

- Backend endpoint schema changes.
- CSV format changes.
- Historical persistence across reboot.

## Data Source Contract
Use existing GET endpoint:

- `/api/test/results`

Expected payload shape:

```json
{
	"steps": [
		{
			"step": 0,
			"throttle": 0.0,
			"thrust": 2.3,
			"current": 0.015,
			"efficiency": 0.0,
			"thermalMax": 28.2,
			"thermalAbort": false
		}
	]
}
```

Field mapping:

- X: `throttle`
- Y Thrust: `thrust`
- Y Current: `current`
- Y Efficiency: `efficiency`
- Y Temperature: `thermalMax`
- Annotation source: `thermalAbort`

## UI and Layout Requirements
File: `data/index.html`

Add a new panel titled `Result Graph` near the Test Results panel with:

- `<canvas id="results-graph">`
- Compact legend with 4 color-coded series labels.
- Meta/status line for data count and runtime context.

Responsive behavior:

- Desktop: full-width panel allowed.
- Mobile: canvas height reduced; no horizontal overflow.

## Rendering Requirements
Rendering technology:

- Vanilla Canvas 2D API (no external chart libraries).

Graph behavior:

- Draw axes and grid.
- Plot four line series:
	- Thrust
	- Current
	- Efficiency
	- Temperature (thermal max)
- Draw point markers on each valid data point.
- Skip invalid/missing values gracefully.
- Empty-state text when no data exists.
- On point hover, show a tooltip with step, throttle, thrust, current, efficiency, and temperature.

Axis model:

- X axis domain from throttle values with small padding.
- X axis domain is fixed from `0` to the maximum measured throttle of the current result set (no padding).
- Left Y axis for thrust, current, temperature combined domain.
- Right Y axis for efficiency domain.
- Temperature series uses its own domain where the minimum temperature of the current test maps to the bottom of the plot area.

Annotations:

- During active test, show vertical marker at current step if point exists.
- For `thermalAbort == true`, draw a red X marker at the corresponding temperature point.

## Update and Integration Requirements
Integration points in `data/index.html`:

- `fetchResults()` remains source-of-truth for table and graph updates.
- On each successful fetch:
	- rebuild table rows
	- normalize graph data
	- redraw graph
- On telemetry updates:
	- keep latest test-running and current-step state
	- redraw graph marker without requiring full refetch

Performance guardrails:

- Schedule redraw via `requestAnimationFrame`.
- Avoid redundant redraw storms.

## Non-Functional Requirements
- Must not break existing thermal canvas rendering.
- Must not change existing API endpoints or firmware behavior.
- Must remain functional in Chromium-class browsers used for local device UI.

## Acceptance Criteria
Functional:

1. Result Graph panel is visible and styled consistently with existing UI.
2. X axis uses throttle values from results payload.
3. Four Y series are rendered: thrust, current, efficiency, temperature.
4. Graph updates after `fetchResults()` is called.
5. Existing results table still renders correctly.

Runtime behavior:

1. While test is running, current-step marker moves when telemetry step changes.
2. Thermal abort points show red X annotations.
3. Empty dataset shows readable placeholder text.
4. Hovering near any plotted point shows detailed values for that point.

Safety/regression:

1. No console errors in normal operation.
2. No regressions in start/abort/config/calibration workflows.

## Manual Test Plan
1. Open UI with no previous results: verify empty graph message.
2. Run short stepped test (for example 5 to 10 steps).
3. Confirm graph grows as steps complete.
4. Compare at least 3 points against table values for throttle/thrust/current/efficiency/thermal max.
5. Abort a test and confirm graph remains stable and marker handling is valid.
6. Verify mobile viewport rendering (no clipped axes/legend).

## Implementation Notes For AI Agent
- Keep edits focused in `data/index.html` for first delivery.
- Reuse existing fetch cadence and websocket state fields.
- Do not introduce dependencies unless explicitly requested later.
- If axis readability is poor for mixed units, optional follow-up is per-series toggle visibility.
