# SP1-009: Proxy Animation, Keyframes, and Interpolation

Status: implemented
Branch: sprint01/sp1-009-proxy-animation-keyframes
Start commit: `08277f5`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 13.6: Hit and hurt model
- `docs/action_combat_engine_editor_design.md` section 14.3: Move timeline editor target
- `docs/action_combat_engine_editor_design.md` section 19.3: Deterministic tests
- `docs/action_combat_engine_editor_design.md` section 27: Recommended First Engineering Tasks

## Goal

Add a temporary proxy animation format with root-motion keyframes and named socket tracks so move/collision work can sample `weapon_base`, `weapon_tip`, `chest`, and root transforms in fixed ticks without waiting for skeletal animation import or GPU skinning.

## Non-Goals

- Do not import skeletal FBX animation clips or skin meshes.
- Do not implement animation graphs, blending trees, retargeting, or clip state machines.
- Do not integrate visual debug rendering yet; expose the same sampled data that a visual lab can consume later.
- Do not replace this temporary format with the final animation asset/cooker.

## Current Baseline

- SP1-008 collision queries can consume manually supplied proxy socket transforms.
- SP1-006 move assets reference hitbox sockets such as `weapon_tip`.
- No authored proxy animation asset or keyframe sampler exists.

## Data Flow

```text
proxy animation JSON
  -> animation/proxy loader
  -> root and named socket keyframe tracks
  -> fixed tick sampler
  -> presentation interpolation sampler
  -> root-motion delta and socket world transforms
  -> CTest assertions
```

Ownership boundaries:

- Proxy animation is a temporary authoring/runtime data helper.
- Sampling must be headless and renderer-independent.
- Collision and visual debug systems consume sampled socket transforms; they do not own animation data.

## Implementation Plan

1. Add a small `animation_core` library target for proxy animation v1.
2. Define proxy animation schema with schema version, logical ID, duration ticks, root keyframes, and named socket keyframe tracks.
3. Implement load/canonical save helpers and validation for finite values, ordered ticks, required sockets, and in-range keyframes.
4. Implement fixed tick sampling and fractional presentation interpolation.
5. Implement root-motion delta/accumulation helpers.
6. Implement socket world transform sampling from root yaw/translation plus socket local translation.
7. Add `content/animations/sword_light_proxy.anim.json`.
8. Add `proxy_animation_tests` under `unit;animation;combat`.

## Test Plan

- Unit:
  - Exact keyframe sampling at authored ticks.
  - Interpolation midpoint for root and socket tracks.
  - Root-motion delta/accumulation between ticks.
  - Socket world transform for `weapon_base`, `weapon_tip`, and `chest`.
  - Integer-tick headless sampling and presentation sampling at the same tick return equivalent poses.
- Process/CTest:
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug-combat`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- A temporary proxy animation JSON fixture exists and validates.
- Root and named socket tracks can be sampled by simulation tick.
- Fractional interpolation works for presentation.
- Root-motion delta and socket world transforms are tested.
- Sampled socket data is renderer-independent and suitable for later visual lab/debug draw consumption.
- Existing Sprint 1 tests remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Keep the format intentionally small and clearly temporary.
- Avoid baking in final skeletal animation assumptions.
- Ensure socket transform math is deterministic and independent of renderer frame cadence.
- Do not let interpolation samples drive gameplay correctness; fixed tick samples remain authoritative.

## Progress Log

- 2026-06-20: Started after SP1-008 primitive combat collision was merged.
- 2026-06-20: Added `animation_core`, proxy animation schema/loading/canonicalization, root/socket keyframe sampling, interpolation, root-motion deltas, socket world transforms, fixture, and CTest coverage.

## Verification Results

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 6/6 tests.
- `ctest --preset msvc-debug-unit` passed: 12/12 tests.
- `ctest --preset msvc-debug` passed: 14/14 tests, including `network_e2e` and `viewer_smoke`.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.
- No MSVC runtime/assertion windows were visible after verification.

## Final Commits

- `a45d395` Add SP1-009 proxy animation sampling
