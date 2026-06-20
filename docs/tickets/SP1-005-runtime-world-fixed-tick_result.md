# SP1-005 Result: RuntimeWorld and Fixed Tick Runner

Status: complete
Branch: `sprint01/sp1-005-runtime-world-fixed-tick`
Start commit: `e187cfb`
Implementation commit: `7047932`

## What Changed

- Added `simulation_core`, a headless runtime layer with no Vulkan, GLFW, WinSock, or platform input dependency.
- Added simulation `RuntimeWorld`, `RuntimeActor`, `InputFrame`, fixed tick stepping, render-cadence replay, state hashing, actor lookup, and presentation transform sampling.
- Imported SP1-004 `content_core::RuntimeWorld` into simulation actors using enabled spawn points as temporary actor creation points.
- Reused current `combat_simulation` locomotion and interpolation helpers instead of duplicating movement rules.
- Added `simulation_runtime_tests` and the `msvc-debug-simulation` CTest preset.

## Behavior Now Covered

- Primitive arena content compiles and imports into a simulation world with stable actor IDs from spawn points.
- Same semantic input stream produces the same state hash across repeated runs.
- Input frame storage order does not affect same-tick playback because frames are sorted by runtime actor ID before application.
- Simulated render cadences of 30, 60, 144, and irregular frame times produce the same final simulation hash.
- Presentation interpolation can be sampled without renderer involvement and clamps outside `[0, 1]`.

## State Hash Contract

The current hash is FNV-style and intentionally local to the simulation test harness. Field order is: world tick, actor count, then actors sorted by runtime actor ID with quantized current translation, yaw, movement speed, target state, and last-tick movement flags.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 8/8 tests.
- `ctest --preset msvc-debug-content` passed: 1/1 test.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 10/10 tests.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Simulation actor creation from spawn points is temporary; later character definitions and scenario fixtures should control which actors exist.
- `simulation_core` currently links through `scene_core` to reuse `combat_simulation`; a later cleanup should split movement helpers into a smaller combat/simulation dependency.
- State hashes are deterministic for current tests but are not yet a serialized network compatibility contract.
- Viewer/client runtime is not yet migrated to `simulation_core`, so SP1-003 characterization remains the guardrail for future extraction.

## Next Recommended Ticket

Proceed to `SP1-006: Move Schema v0 and Compiler`, using `simulation_core::InputFrame` and fixed tick semantics as the runtime timing boundary.
