# RM1-003 Result: Cross-Asset Content Validation

Status: complete
Branch: `mitigation01/rm1-003-cross-asset-validation`
Start commit: `1eeb5b4`
Implementation commit: `6ebec8d`

## What Changed

- Added early path diagnostics for missing scenario map, move, proxy animation, and golden trace files.
- Added a scenario content graph validation pass after individual map/move/proxy animation validation.
- Validates duplicate move IDs, duplicate animation bindings, animation bindings that reference unknown moves, hitbox moves without animation bindings, and hitbox socket references missing from the bound proxy animation.
- Added focused regression tests by mutating loaded scenarios in memory and creating one temporary move file for the bad-socket case.

## Behavior Now Covered

- Existing four golden scenarios still match.
- Missing content paths fail before simulation with stable diagnostic codes and field paths.
- Unknown animation bindings fail before simulation.
- Hitbox moves without proxy animation bindings fail before simulation.
- Hitbox tracks referencing sockets absent from the bound proxy animation fail before simulation.
- Golden update remains guarded by `--update-golden` plus `VAC_UPDATE_GOLDENS=1`.

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_tests` passed.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_runner` passed with the pre-existing `std::getenv` deprecation warning.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "combat_scenario" --output-on-failure` passed: 6/6 tests.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13 tests.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed with the existing `Xinput.lib` imported `DllMain` linker warning.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 24/24 tests.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

## Residual Risks

- This is still a scenario-owned validation path, not a full asset registry or cooker dependency graph.
- Socket semantics remain string-based inside the Sprint 1 schemas; this ticket validates references but does not replace strings with stable cooked IDs.
- Scenario placement and hurtboxes are still scenario-authored until a later character asset definition sprint.

## Next Recommended Ticket

Continue the mitigation minisprint with RM1-001/RM1-002 coordination. RM1-004 should consume this validation path when scenario-driven visual lab diagnostics are dispatched.
