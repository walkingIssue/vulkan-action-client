# RM1-001: Local Asset Preflight and Fixture Guidance

Status: complete
Branch: mitigation01/rm1-001-local-asset-preflight
Start commit: `48d2f74`
Source plan: `docs/sprint-01-risk-mitigation-plan.md`
Source result docs:
- `docs/tickets/SP1-003-characterization_result.md`
- `docs/tickets/SP1-011-visual-combat-lab_result.md`
- `docs/tickets/SP1-012-command-script-map-wireframing_result.md`

## Goal

Make the ignored local paladin asset prerequisite explicit and easy to diagnose before agents run the full viewer/characterization suite.

## Non-Goals

- Do not commit extracted binary assets or ZIP payloads.
- Do not change the legacy bootstrap scene characterization fixture.
- Do not remove the paladin-backed legacy viewer smoke.
- Do not replace the new `visual_lab_smoke`, which already avoids imported model assets.

## Current Baseline

- Full `ctest --preset msvc-debug` passes when `assets/extracted/stylized_paladin_obj/Stylized_Paladin_Clean.obj` exists locally.
- Fresh isolated worktrees can miss `assets/extracted`, causing `characterization_tests` and `viewer_smoke` to fail with model-load errors.
- The dependency is documented in result notes but not represented as a focused preflight target.

## Data Flow

```text
asset fixture path list
  -> asset_fixture_check executable
  -> filesystem existence/readability checks
  -> structured result JSON + stdout diagnostics
  -> CTest preflight label
```

Ownership boundaries:

- The preflight owns local environment diagnostics only.
- The renderer and scene loader remain responsible for actual model loading.
- No asset payload or extracted binary directory should become tracked by this ticket.

## Implementation Plan

1. Add `asset_fixture_check` using `host_core` common `--result-file` support and a custom `--root` override for tests.
2. Check the paladin OBJ, MTL, and texture directory under `assets/extracted/stylized_paladin_obj`.
3. Emit actionable diagnostics naming the legacy tests that depend on the asset and the local copy source on this machine.
4. Add a CTest target with `asset;preflight;viewer;characterization` labels.
5. Add unit/process coverage for both existing and missing root paths.
6. Update README or ticket notes with the direct preflight command.

## Test Plan

- Unit/process:
  - `asset_fixture_check --result-file <temp>` succeeds when assets exist.
  - `asset_fixture_check --root <missing-temp> --result-file <temp>` fails with missing OBJ/MTL/texture diagnostics.
- CTest:
  - `ctest --test-dir build/msvc-debug -R "asset_fixture_check" --output-on-failure`
  - `ctest --preset msvc-debug-viewer`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- Agents can run a focused preflight for local ignored assets.
- Missing asset failures produce structured diagnostics and nonzero exit.
- The preflight result file includes `status`, `host`, and missing/present asset diagnostics.
- No asset payloads, generated build outputs, or unrelated untracked files are committed.

## Risks and Watchpoints

- This only partially mitigates asset hydration; it does not solve distribution.
- Keep the preflight optional and diagnostic so CMake configure/build remains source-only.
- Do not read secrets or machine-specific password files.
- Coordinate `CMakeLists.txt` edits with other agents.

## Progress Log

- 2026-06-20: Started after Sprint 1 completed and `docs/sprint-01-risk-mitigation-plan.md` was pushed.
- 2026-06-20: Added `asset_fixture_check`, CTest preflight coverage, missing-root negative coverage, and README guidance.
- 2026-06-20: Rebasing onto `origin/main` at `cab1da0` preserved the RM1-001 implementation without conflicts.

## Verification Results

- `cmake --build --preset msvc-debug --target asset_fixture_check` passed after rebase.
- `ctest --test-dir build/msvc-debug -R "asset_fixture_check" --output-on-failure` passed: 2/2 tests.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-viewer --output-on-failure` passed: 4/4 tests.
- `ctest --preset msvc-debug --output-on-failure` passed: 26/26 tests.
- `build/msvc-debug/test-artifacts/asset_fixture_check_result.json` reported `status: ok`, `host: asset_fixture_check`, and present diagnostics for the OBJ, MTL, and texture directory.
- `build/msvc-debug/test-artifacts/asset_fixture_check_missing_result.json` reported `status: error`, `host: asset_fixture_check`, missing diagnostics for the OBJ, MTL, and texture directory, and remediation guidance.

## Final Commits

- `3f014d0` Add RM1-001 asset fixture preflight
