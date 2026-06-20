# RM1-003: Cross-Asset Content Validation

Status: complete
Branch: mitigation01/rm1-003-cross-asset-validation
Start commit: `1eeb5b4`
Source plan: docs/sprint-01-risk-mitigation-plan.md
Source result docs:
- docs/tickets/SP1-009-proxy-animation-keyframes_result.md
- docs/tickets/SP1-010-scenario-runner-goldens_result.md
- docs/tickets/SP1-011-visual-combat-lab_result.md

## Goal

Add a scenario-level validation pass that checks the map, move, proxy animation, and golden fixture references as one content graph before a combat scenario runs or updates a golden trace.

## Non-Goals

- Do not build a full asset registry, package ID system, dependency graph UI, or cook pipeline.
- Do not change the Sprint 1 move, proxy animation, map, or scenario schema versions.
- Do not rewrite combat runtime hit resolution or damage/reaction semantics.
- Do not add new CMake targets if the existing scenario tests can cover the behavior.

## Current Baseline

- `combat_scenario_runner` loads scenario JSON, validates the scenario's own required fields, then loads map, move, proxy animation, and golden files.
- Move hitbox tracks reference proxy sockets by string.
- Scenario animation bindings map move logical IDs to proxy animation files, but there is no explicit validation that every hitbox socket exists in the animation bound to that move.
- Missing files fail through load exceptions and become generic process errors instead of content graph diagnostics.
- Golden traces can be compared or updated after the scenario loads, but no manifest-style dependency sanity check guards drift between scenarios and their referenced assets.

## Data Flow

```text
scenario JSON
  -> path resolution
  -> map/move/proxy animation/golden load
  -> per-asset validation and compile
  -> cross-asset graph validation
  -> scenario run or early structured diagnostics
  -> combat_scenario_tests and process CTest coverage
```

Ownership boundaries:

- `combat_scenario_core` owns scenario fixture orchestration and cross-asset diagnostics.
- `content_core` remains the authority for individual map and move validation.
- `animation_core` remains the authority for individual proxy animation validation and socket tracks.
- The golden trace stays an output oracle; this ticket only validates that the graph is coherent before comparison or update.

## Implementation Plan

1. Preserve the existing scenario shape validation and load order.
2. Add path-aware diagnostics for missing map, move, proxy animation, and golden files.
3. Build lookup tables for compiled move logical IDs and animation bindings.
4. Reject animation bindings that reference unknown moves.
5. Reject moves with hitbox socket references that are not present in the proxy animation bound to that move.
6. Reject active hitbox moves that have no proxy animation binding.
7. Add regression tests using in-memory scenario mutations so no extra fixture sprawl is needed.
8. Keep successful golden scenario traces unchanged.

## Test Plan

- Unit/integration:
  - Existing four golden scenarios still pass.
  - A scenario with a move hitbox socket missing from the bound proxy animation fails with a precise diagnostic.
  - A scenario with a missing move or proxy animation path fails with a path-specific diagnostic.
  - A scenario animation binding for an unknown move fails before simulation.
- Process/CTest:
  - `ctest --preset msvc-debug-combat --output-on-failure`
  - If feasible after any remote changes, `ctest --preset msvc-debug --output-on-failure`

## Acceptance Criteria

- Scenario result diagnostics include stable codes and field paths for missing files, unknown animation bindings, missing animation bindings for hitbox moves, and missing sockets.
- The scenario runner exits before golden comparison/update when cross-asset validation fails.
- Existing scenario goldens remain byte-for-byte semantically matched.
- No CMake, renderer, visual lab, asset preflight, or unrelated untracked files are changed.

## Risks and Watchpoints

- Keep diagnostics deterministic so result JSON remains stable.
- Avoid duplicating move or animation schema validation rules that already live in `content_core` and `animation_core`.
- Coordinate with RM1-001 because that ticket owns CMake/preflight edits.
- Golden update behavior must remain guarded by `--update-golden` plus `VAC_UPDATE_GOLDENS=1`.

## Progress Log

- 2026-06-20: Started after Sprint 1 completed and `docs/sprint-01-risk-mitigation-plan.md` was pushed.
- 2026-06-20: Added scenario content graph validation, path diagnostics, move-animation binding checks, hitbox socket checks, and focused regression coverage.

## Verification Results

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_tests` passed.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_runner` passed with the pre-existing `std::getenv` deprecation warning.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "combat_scenario" --output-on-failure` passed: 6/6 tests.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target visual_lab_tests` passed after the broader combat preset found that binary missing in this checkout's build tree.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13 tests.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed with the existing `Xinput.lib` imported `DllMain` linker warning.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 24/24 tests.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

Notes:

- A raw build command outside `tools/dev-shell.ps1` failed before project compilation because Clang could not find MSVC standard library headers such as `<cstdint>` and `<string>`. Verification was rerun through `tools/dev-shell.ps1`, which sets the required MSVC/Ninja/Vulkan/vcpkg environment.
- An early focused CTest run failed process tests because `combat_scenario_runner.exe` was stale; rebuilding the runner fixed the process tests.

## Final Commits

- `6ebec8d` Add scenario content graph validation
