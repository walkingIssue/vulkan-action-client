# SP2-003 Result: Golden Governance and Update Workflow v1

Status: ready for merge
Branch: `sprint02/sp2-003-golden-governance-update-workflow`
Start commit: `f512327`
Implementation commits: `e833bc3`, `0afeb4a`

## What Changed

- Added `docs/golden-trace-workflow.md` as the durable scenario golden update workflow.
- Documented the normal compare-only path, the guarded update command flow, and the requirement to clear `VAC_UPDATE_GOLDENS` before broad verification.
- Added a review checklist for intentional golden drift, including scenario id, golden path, semantic reason, event count/order/effect/hash changes, update command, and verification.
- Added a result-doc contract requiring every ticket to list intentional golden drift or explicitly say none occurred.
- Added focused `combat_scenario_tests` coverage proving checked-in goldens are not rewritten by normal runs.
- Added focused update-gate tests proving environment permission without `--update-golden` cannot rewrite a mismatched golden, and `--update-golden` without `VAC_UPDATE_GOLDENS=1` cannot rewrite a mismatched golden.
- Added a positive test proving fully guarded updates can rewrite only a temporary golden and that the written file matches the actual trace.
- Strengthened golden mismatch diagnostics coverage so damage/reaction fields remain visible after `SP2-001` trace growth.

## Manifest Decision

No separate generated manifest was added in this ticket. The current readable trace JSON already carries scenario id, seed, tick count, final state hash, ordered events, and expanded effect fields. A small workflow doc plus explicit result-doc review checklist is easier to review right now than another metadata artifact that could drift independently. Revisit a manifest once more scenario families or trace schema versions exist.

## Behavior Now Covered

- Normal CTest-style scenario runs compare goldens without rewriting checked-in files.
- Missing `--update-golden` keeps the runner in compare mode even if the environment permission equivalent is present.
- Missing `VAC_UPDATE_GOLDENS=1` rejects update mode and preserves the existing golden file.
- Fully guarded update mode writes the actual trace to the requested golden path.
- Mismatch diagnostics continue to expose `damage`, `targetRemainingHealth`, `reactionMove`, `hitstopTicks`, and `stunTicks`.
- Future result docs have an explicit place to distinguish intentional golden drift from accidental drift.

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_tests` passed.
- `. .\tools\dev-shell.ps1; .\build\msvc-debug\combat_scenario_tests.exe` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R combat_scenario_tests --output-on-failure` passed: 1/1.
- Initial `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug-combat --output-on-failure` failed because process/viewer/move executables were stale after only rebuilding the focused test target.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` refreshed stale binaries and passed.
- `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

## Intentional Golden Drift

- None. Existing checked-in scenario goldens still match.

## Residual Risks

- The workflow is doc-and-test governance, not a CI-only permission system. Local developers can still set the explicit update gate, so review discipline remains required.
- No trace schema version manifest exists yet. That is acceptable while there are only a few scenario families, but a later ticket should revisit it when traces diversify.
- The process-level update guard still checks the missing environment side through CTest. The missing command-option and positive update cases are covered in the focused C++ test harness using temporary golden files.

## Next Recommended Ticket

Proceed to `SP2-004 Scenario Playback Controls in Visual Lab v1`. The visual lab should consume the now-documented scenario/golden evidence without owning golden acceptance rules.
