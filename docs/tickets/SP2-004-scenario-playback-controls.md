# SP2-004: Scenario Playback Controls in Visual Lab v1

Status: in progress
Branch: sprint02/sp2-004-scenario-playback-controls
Start commit: pending docs-only start commit from base `b08f4ff`
Source plan: `docs/sprint-02-implementation-plan.md`
Source dispatch: `C:\Users\Bartek\Documents\Playground\vulkan-agent-comms\claims\SP2-004.md`
Required base: `origin/main` `b08f4ffa0c6d3bcfe35093d25e693a2fb4418a00`
Source result docs:
- `docs/tickets/SP1-011-visual-combat-lab_result.md`
- `docs/tickets/RM1-004-scenario-visual-lab_result.md`
- `docs/tickets/SP2-001-combat-effects-damage-reactions.md`
- `docs/tickets/SP2-002-character-definitions-spawn-ownership.md`
- `docs/tickets/SP2-003-golden-governance-update-workflow_result.md`

## Goal

Add basic scenario playback controls to the visual lab so scenario/golden evidence can be stepped, reset, and inspected after damage/reaction traces and actor ownership boundaries are stable.

## Non-Goals

- Do not build the full editor UI, timeline editor, undo/redo, hierarchy, inspector, or asset browser.
- Do not make visual output the oracle for combat correctness.
- Do not invent character ownership, damage semantics, or golden rules in viewer code.
- Do not add screenshot goldens unless capture tooling is separately stabilized.
- Do not depend on ignored local `assets/`.

## Current Baseline

- `visual_lab_core` can build direct-asset and scenario-driven lab data through `buildVisualLabScene` and `buildVisualLabSceneFromScenario`.
- `vulkan_scene_viewer --visual-lab --scenario <path> --offline` reports scenario/golden diagnostics through `VisualLabScene::resultDiagnostics`.
- Visual lab mode has smoke coverage, but no timeline controls or frame-by-frame scenario playback UI.
- Scenario result diagnostics already expose source, id, status, golden comparison flags, trace event count, final hash, ticks run, and golden path.
- Combat correctness belongs to scenario traces/goldens; visual lab must consume that evidence without recomputing combat rules.

## Data Flow

```text
scenario fixture + golden result
  -> combat scenario runner trace
  -> scenario-driven visual lab data
  -> playback state for current tick, pause/run, step, reset, and bounded seek
  -> evidence summary over trace fields
  -> viewer result diagnostics for smoke/process coverage
```

Ownership boundaries:

- Scenario runner owns the trace and golden truth.
- Visual lab owns playback presentation and summaries over that trace.
- Viewer owns input/CLI/plumbing and result-file diagnostics.

## Implementation Plan

1. Add a small `visual_lab_core` playback model for duration/current tick, paused/running state, step, reset, and bounded tick seek.
2. Thread scenario trace evidence into `VisualLabScene` so playback state and summaries are derived from runner output.
3. Emit scenario evidence summaries for event count plus damage/reaction/effect fields without recomputing combat rules.
4. Add focused `visual_lab_tests` coverage for playback step/reset/seek bounds and scenario evidence summaries.
5. Preserve existing direct-asset and scenario-driven smoke behavior and offline mode.
6. Add structured result diagnostics that prove the playback path loaded scenario/golden evidence and selected deterministic playback state.
7. Keep viewer/UI changes narrow and extract reusable visual lab helpers instead of growing `vulkan_scene_viewer.cpp`.

## Test Plan

- Unit:
  - Visual lab playback model step/reset/seek bounds.
  - Scenario evidence summaries include trace event counts and effect fields.
- Focused:
  - Build and run `visual_lab_tests`.
  - `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure`.
- Process/CTest:
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
- Keep scenario trace inspection read-only; adding combat semantics here would violate the visual-lab consumer boundary.
- Preserve existing smoke result fields so viewer presets keep passing.
- `assets/` is local/untracked and must remain untouched.

## Progress Log

- 2026-06-20: Planned by `SP2-PLAN`.
- 2026-06-20: Aetoun dispatched by Mia from `origin/main` `b08f4ff`; posted ACK/start evidence with tracked-clean status and untracked `assets/` only.

## Verification Results

Fill this before merging.

## Final Commits

Fill this after implementation.
