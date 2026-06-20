# SP2-001: Combat Effects and Damage/Reactions v1

Status: ready for merge
Branch: sprint02/sp2-001-combat-effects-damage-reactions
Start commit: `44c0422`
Source plan: `docs/sprint-02-implementation-plan.md`
Source dispatch: `C:\Users\Bartek\Documents\Playground\vulkan-agent-comms\claims\SP2-001.md`
Required base: `origin/main` `8f816f3 Merge Sprint 02 planning docs`
Source result docs:
- `docs/tickets/SP1-007-combat-runtime-v0_result.md`
- `docs/tickets/SP1-008-primitive-combat-collision_result.md`
- `docs/tickets/SP1-010-scenario-runner-goldens_result.md`
- `docs/tickets/RM1-003-cross-asset-validation_result.md`
- `docs/tickets/RM1-004-scenario-visual-lab_result.md`

## Goal

Add the first deterministic damage and reaction semantics to the combat runtime and scenario runner so checked-in scenarios can prove when a hit applies damage, how much health remains, what reaction/effect starts, and which combat-affecting timing state changes.

## Non-Goals

- Do not introduce final character definition ownership; `SP2-002` owns that.
- Do not add authoritative combat networking, rollback, prediction, command replication, or matchmaking.
- Do not add visual lab playback controls unless existing smoke diagnostics break.
- Do not depend on ignored local `assets/`.
- Do not replace the whole move schema or build an effects scripting language.

## Current Baseline

- `combat_runtime_core` tracks active moves, move ticks, state tags, command buffers, hitstop/stun counters, and hit registry placeholders.
- `combat_collision` produces deterministic hit candidates and once-per-target filtering.
- `combat_scenario_core` emits move, phase, authored event, hit, whiff, blocked hit, cancel, completion, final hash, and golden comparison data.
- Current scenario actors, placements, and hurtboxes are scenario-authored.
- Scenario traces do not yet include damage amount, remaining health, reaction/effect id, or health/resource/hitstop/stun state in the final hash.

## Data Flow

```text
move authored hit/effect data + scenario bridge actor state
  -> combat runtime hit/effect application
  -> scenario trace event damage/reaction/resource fields
  -> final state hash over combat-affecting state
  -> readable golden comparison and result JSON
```

Ownership boundaries:

- Health/resource/effect defaults added here are provisional scenario bridge fields.
- Runtime owns applying deterministic effects.
- Scenario runner owns trace/result/golden representation.
- Character content owns permanent defaults only after `SP2-002`.

## Implementation Plan

1. Add the smallest authored damage/effect fields needed by the current light-attack fixtures.
2. Add provisional scenario actor combat defaults such as health or resource state, clearly named as bridge data.
3. Apply damage exactly once per valid victim per move instance, using the existing hit registry and collision ordering.
4. Emit structured trace fields for damage amount, victim remaining health, reaction/effect id, and applied hitstop/stun ticks if used.
5. Include new combat-affecting state in the final scenario state hash.
6. Extend scenario fixtures and goldens for hit, whiff, cancel-on-hit, dodge/invulnerability, and duplicate-hit suppression behavior.
7. Preserve readable golden mismatch diagnostics after the trace schema grows.
8. Update the ticket result with any intentional golden drift and residual bridge fields for `SP2-002`.

## Test Plan

- Unit:
  - `combat_runtime_tests` for damage application, reaction start, hitstop/stun changes, and no damage during invulnerability.
  - `combat_collision_tests` or focused runtime coverage proving duplicate collision candidates do not duplicate damage.
  - `move_asset_tests` if authored damage/effect fields are added to move JSON.
- Integration:
  - `combat_scenario_tests` for hit applies one damage instance, whiff applies no damage, cancel-on-hit still works with damage, and reaction starts deterministically.
  - Golden mismatch snippets include the new effect fields.
- Process/CTest:
  - `ctest --preset msvc-debug-combat --output-on-failure`.
  - `cmake --build --preset msvc-debug`.
  - `ctest --preset msvc-debug --output-on-failure`.
  - MSVC runtime/assertion dialog check after native verification.

## Acceptance Criteria

- Scenario traces include structured damage/effect fields for the affected scenarios.
- Final state hashes change when health/reaction/timing state changes.
- Hit damage is applied exactly once per valid target per move instance.
- Whiff, dodge/invulnerability, and blocked/prevented hit paths do not apply damage.
- At least one reaction/effect path starts deterministically from a successful hit.
- Updated goldens are readable, guarded, reviewed in the result doc, and covered by CTest.
- `assets/` remains untracked and untouched.

## Risks and Watchpoints

- Keep provisional health/resource/effect fields obviously temporary so `SP2-002` can replace them.
- Avoid spreading combat rules into `combat_scenario_core`; it should orchestrate and report runtime truth.
- Avoid viewer, visual lab, and network files unless strictly required by changed result artifacts.
- Serialize edits to scenario fixtures and goldens through Mia.

## Progress Log

- 2026-06-20: Planned by `SP2-PLAN`. Implementation not yet dispatched.
- 2026-06-20: Vera dispatched by Mia for SP2-001 from `origin/main` `8f816f3`; start-doc refresh committed before code edits on branch `sprint02/sp2-001-combat-effects-damage-reactions`.
- 2026-06-20: Added authored light-attack damage/effect data, provisional scenario combat bridge health, runtime hit effect application, trace effect fields, and final hash coverage for health/timing state.
- 2026-06-20: Updated checked-in scenario goldens for hit, whiff, cancel-on-hit, and dodge/invulnerability paths. Visual lab scenario smoke expectations were updated because the scenario trace count and final hash intentionally changed.

## Verification Results

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_runtime_tests combat_scenario_tests move_asset_tests combat_scenario_runner` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "(move_asset_tests|combat_runtime_tests)" --output-on-failure` passed: 2/2.
- Guarded golden updates were produced with `VAC_UPDATE_GOLDENS=1` for `sword_light_hits_idle_target`, `sword_light_whiffs`, `sword_light_cancel_on_hit`, and `dodge_invulnerability_boundary`.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "combat_scenario" --output-on-failure` passed: 6/6 after clearing `VAC_UPDATE_GOLDENS`.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "visual_lab_tests" --output-on-failure` passed: 1/1 after updating the intentional trace/hash expectations.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- `build/msvc-debug/test-artifacts/visual_lab_scenario_smoke_result.json` reported `scenarioTraceEventCount=13` and `scenarioFinalStateHash=0xf73237aa3baea830`.

## Final Commits

- `6aed506` Record SP2-001 start commit
- `3bc2783` Add combat damage reaction effects
- `83c0ed3` Update visual lab scenario diagnostics for damage traces
