# SP1-001: CMake and CTest Spine

Status: implemented
Branch: sprint01/sp1-001-cmake-ctest-spine
Start commit: this ticket-start commit on `main`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 4, AD-08: CMake is the canonical interface
- `docs/action_combat_engine_editor_design.md` section 18: Build, Presets, and Developer Interface
- `docs/action_combat_engine_editor_design.md` section 19: Regression Test Architecture
- `docs/action_combat_engine_editor_design.md` section 27: Recommended First Engineering Tasks

## Goal

Make CMake presets and CTest labels the durable way to configure, build, and test the project. PowerShell scripts can remain convenience wrappers, but Sprint 01 work should not depend on hidden `.ps1` behavior.

## Non-Goals

- Do not remove existing PowerShell scripts.
- Do not refactor the viewer, server, network protocol, or test implementations.
- Do not introduce CI provider-specific files.
- Do not add new third-party dependencies.
- Do not rename existing executables unless CTest compatibility requires it.

## Current Baseline

- The repo already uses CMake and Ninja through `CMakeLists.txt`.
- Current local commands are wrapped by `tools/build.ps1`, `tools/test.ps1`, and `tools/dev-shell.ps1`.
- Existing tests are registered with CTest: movement, control profile, network protocol, snapshot relay, and network e2e.
- Build output currently lives under `build/msvc-debug`.
- The project uses vcpkg manifest mode with `vcpkg.json`.
- Tooling installed before this ticket: Git LFS 3.7.1 and LLVM 22.1.8. Ninja, Vulkan SDK, Python, and RenderDoc source build are present.

## Data Flow

```text
CMakePresets.json
  -> configure preset
  -> CMake cache with vcpkg toolchain, MSVC, Ninja, build/test options
  -> build preset
  -> CMake/Ninja target graph
  -> test preset
  -> CTest labels/timeouts/output-on-failure
  -> developer or agent gets repeatable pass/fail output without PowerShell
```

Ownership boundaries:

- `CMakePresets.json` owns configure/build/test entry points.
- `CMakeLists.txt` owns targets, dependencies, test registration, labels, and timeouts.
- PowerShell wrappers may delegate to presets but must not be the only source of required environment.
- vcpkg remains the dependency manager; no package installs are encoded in presets.

## Implementation Plan

1. Inspect the existing CMake target/test graph and script assumptions.
2. Add `CMakePresets.json` with at least `msvc-debug` configure and build presets.
3. Add test presets for all tests and useful labels where supported by existing CMake.
4. Add CTest labels and explicit timeouts to existing tests.
5. Ensure CMake can find the vcpkg toolchain from `VCPKG_ROOT`, the known repo-local vcpkg path, or a user override.
6. Update README build/test instructions to prefer presets and mark PowerShell scripts as convenience wrappers.
7. Verify configure, build, and tests through presets.

## Test Plan

- Configure:
  - `cmake --preset msvc-debug`
- Build:
  - `cmake --build --preset msvc-debug`
- Test:
  - `ctest --preset msvc-debug`
  - If label presets are added, run at least unit/integration/network presets.
- Regression bridge:
  - `.\tools\test.ps1 -Preset msvc-debug` should still work unless intentionally updated to delegate.

## Acceptance Criteria

- A fresh shell can configure, build, and test through CMake/CTest presets.
- Existing tests retain coverage and pass.
- Tests have meaningful labels and timeouts.
- Existing PowerShell wrappers still work or clearly delegate to the preset path.
- README tells developers the preset-first workflow.
- No unrelated assets or generated build outputs are committed.

## Risks and Watchpoints

- Preset environment cannot assume this Codex process has refreshed PATH after installs.
- `VCPKG_ROOT` may exist only in the dev shell, so the preset needs a robust default or documented override.
- CMake preset syntax differs by CMake version; keep the file compatible with the installed CMake version.
- Network E2E tests should avoid fixed sleeps or hidden ports becoming worse in this ticket.
- Do not accidentally move build output in a way that breaks existing scripts without updating them.

## Progress Log

- 2026-06-20: Started ticket after installing Git LFS and LLVM tooling.

## Verification Results

- `cmake --list-presets`
- `ctest --list-presets`
- `. .\tools\dev-shell.ps1; cmake --preset msvc-debug`
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug`
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug`
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-unit`
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-network`
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-e2e`
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat`
- `.\tools\test.ps1 -Preset msvc-debug`

All test runs passed.

## Final Commits

- `7f7116d Add CMake and CTest presets`
