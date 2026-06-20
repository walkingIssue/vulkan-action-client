# SP1-003 Result: Characterization Before Extraction

Status: complete
Branch: `sprint01/sp1-003-characterization`
Start commit: `63c66bf`
Implementation commit: `71ab398`

## What Changed

- Added `characterization_tests`, a fixture-backed CTest target that pins current bootstrap scene semantics, movement/interpolation behavior, protocol bytes, and relay world-state behavior.
- Added small checked-in characterization fixtures for the bootstrap scene and representative protocol packets.
- Registered `viewer_smoke` in CTest with `vulkan_scene_viewer --frames 3 --result-file build/msvc-debug/test-artifacts/viewer_smoke_result.json`.
- Added `msvc-debug-characterization` and `msvc-debug-viewer` CTest presets, and listed them in the README.

## Behavior Now Covered

- Bootstrap scene name, model counts, mesh/material/animation counts, vertex/triangle counts, floor size, transforms, actor spawn transforms, and local/world bounds.
- Fixed-tick framed movement trace through forward movement, backpedal, final actor transform, previous tick transform, and presentation interpolation.
- Interpolation clamping below zero and above one.
- Exact bitpacked packet bytes for connect, disconnect, server disconnect event, and actor snapshot.
- Relay cache behavior for late joiners and disconnect cleanup.
- Process-level Vulkan viewer smoke through CTest and structured result JSON.

## Verification

- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-unit` passed: 6/6 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug-viewer` passed: 1/1 test.
- `ctest --preset msvc-debug` passed: 8/8 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- `viewer_smoke` still opens a real Vulkan window because `vulkan_scene_viewer` does not support `--hidden-window` yet.
- Characterization intentionally pins imported paladin mesh counts and bounds, so asset import flag changes will require deliberate fixture review.
- The protocol fixture locks current byte layout, but it does not yet describe schema evolution/version negotiation beyond the current `ACV1` packet header.

## Next Recommended Ticket

Proceed to `SP1-004: AuthoringScene v0 and Primitive Map Schema`, using this characterization suite as the regression net before replacing the bootstrap scene path with authored primitive map data.
