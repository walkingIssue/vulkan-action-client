# RM1-004: Scenario-Driven Visual Lab Diagnostics

Status: ready for merge
Branch: mitigation01/rm1-004-scenario-visual-lab
Start commit: `b937b33`
Source plan: docs/sprint-01-risk-mitigation-plan.md
Source result docs:
- docs/tickets/RM1-003-cross-asset-validation_result.md
- docs/tickets/SP1-011-visual-combat-lab_result.md

## Goal

Let `vulkan_scene_viewer --visual-lab` accept a checked-in combat scenario fixture as the lab data source and report scenario/golden diagnostics in the viewer result file, bridging headless scenario validation with visual inspection.

## Non-Goals

- Do not add timeline controls, editor panels, undo/redo, live asset editing, or playback UI.
- Do not turn rendering into the combat correctness oracle.
- Do not replace the existing direct-asset visual lab path unless scenario mode clearly supersedes one smoke entry.
- Do not broaden scenario validation beyond the narrow helpers needed by the visual lab.
- Do not touch network E2E files, local `assets/`, or unrelated result docs.

## Current Baseline

- `visual_lab_core` builds a lab scene from direct map, move, and proxy animation paths.
- `vulkan_scene_viewer --visual-lab --offline` uses `--scene` for the map and optional `--move`/`--proxy-animation`.
- Viewer result diagnostics include visual lab overlay counts but not scenario id, golden match state, trace event count, or final state hash.
- `combat_scenario_core` loads scenarios, resolves content paths privately, validates content graphs, runs golden comparisons, and emits structured scenario results.
- RM1-003 added missing-path and content graph diagnostics for scenario-owned map, move, animation, socket, and golden references.

## Data Flow

```text
scenario JSON fixture
  -> combat_scenario_core load/validate/run/golden compare
  -> resolved map/move/proxy animation paths + scenario actors
  -> visual_lab_core scene/debug overlay build
  -> vulkan_scene_viewer --visual-lab --scenario --offline
  -> host result diagnostics with overlay counts + scenario/golden summary
  -> unit and viewer CTest assertions
```

Ownership boundaries:

- `combat_scenario_core` owns scenario parsing, path resolution, content graph validation, trace generation, golden comparison, and deterministic scenario diagnostics.
- `visual_lab_core` owns renderer-friendly primitive scene/debug overlay summaries.
- `vulkan_scene_viewer` owns CLI selection and result-file emission only.
- CTest owns the process smoke oracle through structured result files, not screenshots.

## Implementation Plan

1. Add a narrow scenario visual-lab API that can build lab scene data from a scenario fixture while preserving the existing direct-asset API.
2. Reuse scenario validation/run output for scenario id, golden compared/matched state, trace event count, final state hash, and diagnostics.
3. Add `vulkan_scene_viewer --visual-lab --scenario <path> --offline`, rejecting `--scenario` outside visual-lab mode.
4. Add unit coverage for building scenario-driven lab diagnostics from `tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json`.
5. Add or update a viewer smoke so CTest exercises scenario-driven visual lab mode and writes a structured result file.
6. Update ticket/result docs with verification, residual risks, and final commits.

## Test Plan

- Build:
  - `cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer`
- Focused:
  - `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure`
  - `ctest --preset msvc-debug-viewer --output-on-failure`
  - `ctest --preset msvc-debug-combat --output-on-failure`
- Broader:
  - `cmake --build --preset msvc-debug`
  - `ctest --preset msvc-debug --output-on-failure`
- Failure inspection:
  - Check for visible MSVC runtime/assertion dialogs if a native test hangs or aborts.

## Acceptance Criteria

- `vulkan_scene_viewer --visual-lab --scenario <fixture> --offline` succeeds for a checked-in scenario fixture.
- The viewer result file includes `visualLab=true`, `visualLabSource=scenario`, scenario id, scenario status, golden compared/matched state, trace event count, final state hash, and existing overlay counts.
- Existing direct-asset visual lab behavior remains covered.
- Invalid scenario/golden/content diagnostics surface in structured result output or a clear viewer failure path.
- Focused visual lab, viewer, combat, and full debug CTest verification pass.
- No `assets/` files or unrelated changes are staged.

## Risks and Watchpoints

- Avoid duplicating scenario path resolution or content graph validation in visual lab code.
- Keep any public scenario helper narrow and deterministic.
- `CMakeLists.txt`, `src/vulkan_scene_viewer.cpp`, and public visual-lab/scenario headers are high-conflict files; coordinate before editing.
- Viewer diagnostics are inspection/handoff aids, not gameplay truth.
- Keep render artifact or screenshot goldens out of this ticket.

## Progress Log

- 2026-06-20: Started after RM1-002 landed and Mia released RM1-004 implementation.
- 2026-06-20: Added scenario-driven visual lab build path, viewer `--scenario` routing, focused unit coverage, and scenario visual-lab smoke registration.

## Verification Results

- `cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer` passed.
- `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed: 3/3.
- `ctest --preset msvc-debug-viewer --output-on-failure` passed: 5/5.
- `ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- `build/msvc-debug/test-artifacts/visual_lab_scenario_smoke_result.json` reported `status: ok`, `visualLabSource=scenario`, `scenarioId=scenario.sword_light_hits_idle_target`, `scenarioGoldenMatched=true`, `scenarioTraceEventCount=8`, and `scenarioFinalStateHash=0x899156866903d60e`.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

## Final Commits

- `de92a82` Add scenario visual lab diagnostics
