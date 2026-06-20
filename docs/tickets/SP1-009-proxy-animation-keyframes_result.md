# SP1-009 Result: Proxy Animation, Keyframes, and Interpolation

Status: complete
Branch: `sprint01/sp1-009-proxy-animation-keyframes`
Start commit: `08277f5`
Implementation commit: `a45d395`

## What Changed

- Added `animation_core` with temporary proxy animation schema v1.
- Added root-motion keyframes and named socket keyframe tracks.
- Added validation/canonicalization for proxy animation assets.
- Added fixed tick and fractional presentation sampling.
- Added root-motion delta helper and socket world transform sampling using root yaw/translation.
- Added `content/animations/sword_light_proxy.anim.json`.
- Added `proxy_animation_tests` under `unit;animation;combat`.

## Behavior Now Covered

- Proxy animation fixture validates and canonical-round-trips.
- Root and socket keyframes sample exactly on authored ticks.
- Fractional midpoint interpolation works for root and socket tracks.
- Root-motion deltas are computed between arbitrary sample ticks.
- `weapon_base`, `weapon_tip`, and `chest` socket world transforms are deterministic.
- Integer-tick headless sampling and presentation sampling at the same tick produce equivalent pose data.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 6/6 tests.
- `ctest --preset msvc-debug-unit` passed: 12/12 tests.
- `ctest --preset msvc-debug` passed: 14/14 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Proxy animation format is intentionally temporary and not a skeletal animation replacement.
- Socket rotation is simple Euler addition with root yaw; no quaternion or hierarchy support yet.
- Visual lab/debug draw does not consume proxy samples yet.
- Move assets reference sockets by string; no cross-asset dependency validation exists yet.

## Next Recommended Ticket

Proceed to `SP1-010: Combat Scenario Runner and Golden Traces`, loading maps, moves, proxy animation, and input scripts into deterministic headless traces.
