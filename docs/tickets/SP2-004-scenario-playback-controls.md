# SP2-004: Scenario Playback Controls in Visual Lab v1

Status: planned
Branch: sprint02/sp2-004-scenario-playback-controls
Start commit: TBD by dispatched owner
Source plan: `docs/sprint-02-implementation-plan.md`
Source result docs:
- `docs/tickets/SP1-011-visual-combat-lab_result.md`
- `docs/tickets/RM1-004-scenario-visual-lab_result.md`
- `docs/tickets/SP2-001-combat-effects-damage-reactions.md`
- `docs/tickets/SP2-002-character-definitions-spawn-ownership.md`

## Goal

Add basic scenario playback controls to the visual lab so scenario/golden evidence can be stepped, reset, and inspected after damage/reaction traces and actor ownership boundaries are stable.

## Non-Goals

- Do not build the full editor UI, timeline editor, undo/redo, hierarchy, inspector, or asset browser.
- Do not make visual output the oracle for combat correctness.
- Do not invent character ownership, damage semantics, or golden rules in viewer code.
- Do not add screenshot goldens unless capture tooling is separately stabilized.
- Do not depend on ignored local `assets/`.

## Current Baseline

- `visual_lab_core` can build direct-asset and scenario-driven lab data.
- `vulkan_scene_viewer --visual-lab --scenario <path> --offline` reports scenario/golden diagnostics.
- Visual lab mode has smoke coverage, but no timeline controls or frame-by-frame scenario playback UI.
- Combat correctness belongs to scenario traces/goldens.

## Data Flow

```text
scenario fixture + golden result
  -> scenario-driven visual lab data
  -> playback state for tick, pause, step, reset, and summary overlays
  -> viewer result diagnostics for smoke coverage
```

Ownership boundaries:

- Scenario runner owns the trace and golden truth.
- Visual lab owns playback presentation over that trace.
- Viewer owns input/CLI/plumbing and result-file diagnostics.

## Implementation Plan

1. Wait until `SP2-001` is merged, and preferably until minimal `SP2-002` actor ownership is stable.
2. Add a small playback model for current tick, paused/running state, step, reset, and bounded tick seek if practical.
3. Display damage/reaction trace summaries without recomputing combat rules in viewer code.
4. Preserve existing fixed-frame smoke behavior and offline mode.
5. Add structured result diagnostics that prove the playback path loaded scenario/golden evidence.
6. Keep any viewer/UI changes narrow and extract reusable visual lab helpers instead of growing `vulkan_scene_viewer.cpp`.

## Test Plan

- Unit:
  - Visual lab playback model step/reset/seek bounds.
  - Scenario evidence summaries include trace event counts and effect fields.
- Process/CTest:
  - `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure`.
  - `ctest --preset msvc-debug-viewer --output-on-failure`.
  - `ctest --preset msvc-debug-combat --output-on-failure`.
  - `cmake --build --preset msvc-debug`.
  - `ctest --preset msvc-debug --output-on-failure`.
  - MSVC runtime/assertion dialog check.

## Acceptance Criteria

- Scenario visual lab mode can pause/step/reset or otherwise expose deterministic playback state for a checked-in scenario.
- Viewer result diagnostics prove scenario/golden evidence loaded and playback state advanced as expected.
- The implementation does not duplicate combat rules in viewer or visual lab code.
- Existing direct-asset and scenario-driven visual lab smoke paths remain covered.
- No untracked assets or screenshot goldens are required.

## Risks and Watchpoints

- `src/vulkan_scene_viewer.cpp`, `src/visual_lab/*`, `src/combat/combat_scenario.*`, and `CMakeLists.txt` are high-conflict files.
- Starting this before `SP2-001` risks baking placeholder trace semantics into UI assumptions.
- Starting this before enough `SP2-002` actor ownership exists may force viewer code to invent ownership rules.

## Progress Log

- 2026-06-20: Planned by `SP2-PLAN`. Implementation not yet dispatched.

## Verification Results

Fill this before merging.

## Final Commits

Fill this after implementation.
