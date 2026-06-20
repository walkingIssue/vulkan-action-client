# SP1-010: Combat Scenario Runner and Golden Traces

Status: planned
Branch: sprint01/sp1-010-scenario-runner-goldens
Start commit: `8309d55`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 13.13: Combat regression tests
- `docs/action_combat_engine_editor_design.md` section 17.3: Deterministic state hashes
- `docs/action_combat_engine_editor_design.md` section 17.4: Recording and replay
- `docs/action_combat_engine_editor_design.md` section 18.5: Required executable test switches
- `docs/action_combat_engine_editor_design.md` section 19.7: Golden file governance

## Goal

Add a deterministic command-line combat scenario runner that loads checked-in scenario fixtures, advances fixed combat ticks headlessly, emits ordered event traces and final state hashes, and compares them against reviewed golden expectations.

## Non-Goals

- Do not build the visual combat lab; SP1-011 owns rendered inspection.
- Do not build the map command-script authoring flow; SP1-012 owns map wireframing scripts.
- Do not add networking, prediction, reconciliation, or server/client orchestration.
- Do not use render-frame interpolation as gameplay truth.
- Do not create a broad scripting language beyond the minimum scenario fixture format.

## Current Baseline

- `content_core` loads and validates primitive maps and move assets.
- `simulation_core` owns deterministic fixed ticks and state hashes for runtime worlds.
- `combat_runtime_core` can execute authored moves, command buffering, cancels, hit reactions, and primitive collision.
- `animation_core` can sample proxy root/socket keyframes headlessly.
- Existing tests cover unit-level map, move, simulation, combat runtime, combat collision, and proxy animation behavior.
- There is no process-level scenario runner tying maps, moves, input streams, proxy animation, combat events, and golden trace comparison together.

## Data Flow

```text
scenario JSON
  -> scenario loader and validator
  -> map fixture + actor setup + move asset references + proxy animation references + input frames
  -> RuntimeWorld + CombatRuntime fixed-tick loop
  -> ordered semantic event trace + final state hash + diagnostics
  -> expected golden events/hash comparison
  -> result JSON and CTest scenario pass/fail
```

Ownership boundaries:

- Scenario fixtures are regression assets, not editor documents.
- `content_core`, `simulation_core`, `combat_runtime_core`, and `animation_core` remain owners of their data and rules.
- The scenario runner owns orchestration, result-file output, and golden comparison diagnostics.
- Golden updates must require an explicit guarded mode; normal test runs never rewrite goldens.

## Implementation Plan

1. Add scenario fixture schema and loader for scenario ID, tick count, map path, actors, moves, proxy animations, input frames, expected events, and expected final state hash.
2. Add a `combat_scenario_runner` executable using `host_core` common options, especially `--scene`, `--ticks`, `--input-script`, `--result-file`, `--headless`, and a guarded golden-update option.
3. Build a small headless scenario loop that imports map/runtime data, applies semantic input frames by tick, advances combat, records stable semantic events, and computes the final state hash.
4. Add checked-in scenarios for `sword_light_hits_idle_target`, `sword_light_whiffs`, `sword_light_cancel_on_hit`, and `dodge_invulnerability_boundary`.
5. Compare actual traces to expected events and hashes, printing a compact tick window and expected/actual differences on mismatch.
6. Register unit and CTest scenario coverage with a `scenario` label while preserving existing Sprint 1 tests.

## Test Plan

- Unit:
  - Scenario loader rejects malformed references, invalid ticks, duplicate actor IDs, and unsorted or out-of-range input frames.
  - Event comparison reports the first differing tick and nearby event window.
  - Golden update mode is disabled unless explicitly requested.
- Integration/process:
  - `combat_scenario_runner --input-script tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json --result-file <temp> --headless` writes ok status, ordered events, and final state hash.
  - `sword_light_whiffs`, `sword_light_cancel_on_hit`, and `dodge_invulnerability_boundary` pass through CTest.
  - `ctest --preset msvc-debug-combat`
  - `ctest --preset msvc-debug-simulation`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- A `combat_scenario_runner` executable exists and rejects unsupported or unknown options clearly.
- At least four checked-in scenario fixtures run deterministically headlessly.
- Result files include status, scenario ID, ticks, ordered events, final state hash, and diagnostics.
- Golden mismatch output identifies the relevant tick window and expected/actual events.
- Golden updates are impossible during normal CTest runs.
- Existing Sprint 1 unit, characterization, network, and viewer smoke tests remain green.
- No generated build artifacts or unrelated untracked assets are committed.

## Risks and Watchpoints

- Avoid duplicating combat rules in the runner; it should orchestrate existing runtime systems.
- Keep semantic events compact and stable enough for review, but do not freeze incidental debug text.
- Coordinate `CMakeLists.txt`, shared headers, and fixture paths with SP1-012 because that work is active in another checkout.
- Do not let scenario goldens become opaque blobs; store readable JSON with precise mismatch diagnostics.
- Hashes are useful for desync detection, but field/event diffs must remain available for diagnosis.

## Progress Log

- 2026-06-20: Started after SP1-009 was merged and pushed; SP1-012 is active in a parallel lane.

## Verification Results

Pending.

## Final Commits

Pending.
