# SP2-001 Result: Combat Effects and Damage/Reactions v1

Status: ready for merge
Branch: `sprint02/sp2-001-combat-effects-damage-reactions`
Start commit: `44c0422`
Implementation commits: `3bc2783`, `83c0ed3`

## What Changed

- Added authored hit effect data to `content/moves/light_attack.move.json`, including damage, hitstop, stun, and a deterministic reaction move.
- Extended move asset parsing/compilation with optional hitbox effect fields and validation that authored reaction moves exist in the same move library.
- Added provisional scenario combat bridge health defaults and runtime actor health state without introducing final character definition ownership.
- Added runtime hit effect application for clamped damage, hitstop/stun timing, and reaction move startup.
- Extended scenario trace events with `damage`, `targetRemainingHealth`, `reactionMove`, `hitstopTicks`, and `stunTicks`.
- Included combat-affecting health state in final scenario hashes.
- Updated hit, whiff, cancel-on-hit, and dodge/invulnerability fixtures and goldens for the expanded trace schema.
- Updated visual lab scenario smoke expectations because the scenario trace count and final hash intentionally changed.

## Behavior Now Covered

- Successful light-attack hits apply 12 damage exactly once and record the target at 88 remaining health.
- Successful hits record authored hitstop/stun values and start `move.hit_reaction` deterministically.
- Whiffs do not apply damage or reaction fields.
- Dodge/invulnerability blocked hits report no damage and preserve the target's health.
- Scenario final state hashes now move when combat health/timing state moves.
- Visual lab scenario smoke diagnostics stay aligned with the updated scenario/golden oracle.

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_runtime_tests combat_scenario_tests move_asset_tests combat_scenario_runner` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "(move_asset_tests|combat_runtime_tests)" --output-on-failure` passed: 2/2.
- Guarded golden updates were produced with `VAC_UPDATE_GOLDENS=1` for `sword_light_hits_idle_target`, `sword_light_whiffs`, `sword_light_cancel_on_hit`, and `dodge_invulnerability_boundary`.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "combat_scenario" --output-on-failure` passed: 6/6 after clearing `VAC_UPDATE_GOLDENS`.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "visual_lab_tests" --output-on-failure` passed: 1/1.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 27/27.

Representative updated artifact:

- `build/msvc-debug/test-artifacts/sword_light_hits_idle_target.sp2-001-update.json` reports a `hit` event with `damage=12`, `targetRemainingHealth=88`, `reactionMove=move.hit_reaction`, `hitstopTicks=1`, and `stunTicks=3`.
- `build/msvc-debug/test-artifacts/visual_lab_scenario_smoke_result.json` reports `scenarioTraceEventCount=13` and `scenarioFinalStateHash=0xf73237aa3baea830`.

## Intentional Golden Drift

- `sword_light_hits_idle_target` now records a hit effect and the victim reaction sequence.
- `sword_light_cancel_on_hit` records the same hit effect data while preserving the cancel-on-hit behavior.
- `sword_light_whiffs` retains no damage effect fields and only changes through final-state hash coverage.
- `dodge_invulnerability_boundary` records a blocked hit without damage, remaining-health, or reaction fields.
- Visual lab scenario smoke expected values changed from 8 trace events / `0x899156866903d60e` to 13 trace events / `0xf73237aa3baea830`.

## Residual Risks

- Health defaults are intentionally scenario bridge data for now; SP2-002 should move permanent defaults into character/content ownership.
- `applyHitEffect` applies deterministic runtime truth, while scenario traces remain the reporting layer. Future tickets should keep that separation.
- The reaction move is authored in the existing move library, not a general effects scripting system.
- Network authority, prediction, rollback, and compatibility remain deferred to later network-focused work.

## Next Recommended Ticket

Proceed to SP2-002 character definition ownership once SP2-001 is merged, so the provisional scenario combat bridge fields can be replaced with content-owned actor defaults.
