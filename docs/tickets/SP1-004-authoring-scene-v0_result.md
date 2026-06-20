# SP1-004 Result: AuthoringScene v0 and Primitive Map Schema

Status: complete
Branch: `sprint01/sp1-004-authoring-scene-v0`
Start commit: `7aa00bb`
Implementation commit: `acd43be`

## What Changed

- Added `content_core` as the first content-authoring library, separate from the current viewer-oriented `scene_core`.
- Added `AuthoringScene` schema v1 with stable authored object IDs, runtime entity IDs, transforms, primitives, material base color, colliders, spawn points, world bounds, and kill height.
- Added canonical JSON load/save helpers with deterministic object and spawn ordering.
- Added structured validation diagnostics carrying file, object, component, field path, code, severity, and message.
- Added a minimal `RuntimeWorld` compile DTO for primitive maps, runtime IDs, primitive bounds, collider metadata, spawn points, world bounds, and kill height.
- Added `content/maps/basic_primitive_arena.map.json` plus invalid authoring fixtures.
- Added `authoring_scene_tests` and the `msvc-debug-content` CTest preset.

## Behavior Now Covered

- Valid primitive map fixture loads, validates, saves, and round-trips canonically.
- Canonical save is stable across object/spawn ordering changes.
- Save-to-file and reload path is covered through a build test artifact.
- Invalid fixtures reject duplicate object IDs, invalid transform values, missing parents, invalid collider sizes, invalid world bounds, and missing enabled spawn points.
- Compile path emits a minimal runtime world with stable entity ordering, collider metadata, spawn point ordering, bounds, and kill height.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-content` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 7/7 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 9/9 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- The runtime DTO is intentionally minimal; SP1-005 still needs a proper fixed-tick `RuntimeWorld` and simulation runner.
- The authoring schema is not yet integrated into `vulkan_scene_viewer`, so the bootstrap scene path remains active for rendering.
- Primitive bounds are local analytic bounds only; generated render geometry and collider physics integration remain later work.
- Diagnostics are structured and assertive, but they are not yet exposed through a CLI validator host.

## Next Recommended Ticket

Proceed to `SP1-005: RuntimeWorld and Fixed Tick Runner`, using `content_core::compileRuntimeWorld` as the authored-map input boundary.
