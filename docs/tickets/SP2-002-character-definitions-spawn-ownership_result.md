# SP2-002 Result: Character Definition and Spawn Ownership v1

Status: ready for merge
Branch: `sprint02/sp2-002-character-definitions-spawn-ownership`
Start commit: `dc260f0`
Implementation commits: `4f318a1`

## What Changed

- Added a minimal tracked character definition fixture at `content/characters/basic_duelists.character.json`.
- Added scenario support for top-level character definition paths and per-actor `character` references.
- Added character validation for missing files, duplicate ids, invalid default health, invalid default hurtboxes, unknown default move ids, and unknown actor character refs.
- Applied character-owned health and hurtbox defaults before runtime creation while keeping narrow scenario overrides available.
- Relaxed actor team validation when a spawn is present so map spawn points own team/side by default.
- Migrated the existing combat scenario fixtures to character refs and spawn-owned teams while preserving explicit translation/yaw test setup overrides.
- Kept all scenario goldens unchanged because the migrated defaults match the previous inline bridge data.

## Behavior Now Covered

- Scenario actors can use `character.duelist.player` and `character.duelist.enemy` instead of duplicating combat bridge health and hurtboxes.
- Character default move ids validate against the scenario's compiled move graph.
- Invalid character content exits before simulation and leaves trace events empty.
- Existing process runners and visual lab scenario consumers continue to resolve migrated scenarios through the shared scenario path resolver.

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_tests` passed.
- `. .\tools\dev-shell.ps1; .\build\msvc-debug\combat_scenario_tests.exe` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R combat_scenario_tests --output-on-failure` passed: 1/1.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-content --output-on-failure` passed: 4/4.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

Known verification note:

- The first combat preset run failed because only `combat_scenario_tests` had been rebuilt, leaving process/viewer executables linked against stale scenario code. A full debug build refreshed them, and the rerun passed 13/13.

## Intentional Golden Drift

- None. Character defaults mirror the previous inline actor defaults, and all existing scenario goldens still match.

## Bridge Fields Replaced

- Replaced scenario-local duplicated `combatBridge` defaults in the migrated fixtures.
- Replaced scenario-local duplicated actor `hurtbox` defaults in the migrated fixtures.
- Replaced scenario-local actor `team` ownership in the migrated fixtures with map spawn point ownership.

## Bridge Fields Remaining

- Explicit actor `translation` and `yawDegrees` remain as scenario regression setup overrides because current map spawn positions/facing do not match the old golden fixture placements.
- Scenario inputs, move paths, animation bindings, and goldens remain scenario-owned.

## Residual Risks

- Character definitions are intentionally loaded through the scenario content path rather than a general asset registry or cook pipeline.
- Default move ids are validated but not yet used for runtime command authorization.
- Future spawn cleanup can remove translation/yaw overrides after map spawn points are adjusted or goldens intentionally drift.
- `combat_scenario.hpp` now exposes minimal character fields; further schema growth should wait for later character/editor ownership tickets.

## Next Recommended Ticket

Proceed to SP2-003 golden governance/update workflow. SP2-002 preserved current goldens, but the next semantic changes should have stronger review metadata before more intentional drift lands.
