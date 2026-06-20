# SP1-008: Primitive Combat Collision v0

Status: implemented
Branch: sprint01/sp1-008-primitive-combat-collision
Start commit: `95a0d70`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 10.2: Required collision layers
- `docs/action_combat_engine_editor_design.md` section 10.4: Combat collision
- `docs/action_combat_engine_editor_design.md` section 10.5: Hit ordering
- `docs/action_combat_engine_editor_design.md` section 10.6: Physics regression tests
- `docs/action_combat_engine_editor_design.md` section 13.6: Hit and hurt model

## Goal

Add primitive combat collision queries for authored hit/hurt volumes so the combat runtime can reason about sphere, capsule, and box overlaps, swept weapon motion, stable hit ordering, once-per-target hit registry behavior, and dodge invulnerability boundaries. This ticket creates the query layer that SP1-007 cancel placeholders can consume later.

## Non-Goals

- Do not implement full physics world integration, character controller collision, depenetration, or Jolt-backed queries.
- Do not apply damage, hit reactions, hitstop, or stun from collision results yet.
- Do not add skeletal animation or real socket sampling; SP1-009 owns proxy sockets/keyframes.
- Do not integrate viewer debug drawing yet.

## Current Baseline

- SP1-007 added combat runtime placeholders for hit/block/whiff cancel signals and a hit registry placeholder.
- SP1-006 compiled hitbox and hurtbox tracks but did not execute overlaps.
- There are no primitive combat volume overlap/sweep helpers, no stable hit ordering query result, and no once-per-target hit registry behavior.

## Data Flow

```text
authored/compiled hitbox window
  -> primitive hit volume at root or proxy socket transform
  -> primitive hurt volumes for targets
  -> overlap or swept segment/capsule query
  -> stable ordered CombatHitCandidate list
  -> once-per-target hit registry filter
  -> CTest assertions
```

Ownership boundaries:

- `combat_runtime_core` may own primitive combat query structs and hit registry filtering.
- This ticket does not require a general physics abstraction or renderer debug draw.
- Proxy socket binding is represented as named transforms supplied to the query; SP1-009 will author/sample those transforms.

## Implementation Plan

1. Add primitive volume structs for sphere, capsule, and oriented/axis-aligned box v0.
2. Add root and named proxy socket binding inputs for volume transforms.
3. Implement overlap helpers for sphere/sphere, sphere/box, box/box v0, capsule approximations, and generic broad tests sufficient for sprint fixtures.
4. Implement swept segment/capsule helper for fast weapon motion between previous/current tick positions.
5. Add stable `CombatHitCandidate` ordering by attacker, move instance sequence, hitbox track ID, victim, and contact sub-index.
6. Add once-per-target hit registry filtering.
7. Add dodge/invulnerability tag boundary filtering based on active target state tags.
8. Add `combat_collision_tests` under `unit;combat;collision`.

## Test Plan

- Unit:
  - Hitbox active only on intended half-open ticks.
  - Swept weapon segment catches a target crossed between ticks.
  - Multi-target order is stable across insertion order.
  - Once-per-target registry prevents duplicate hits.
  - Dodge invulnerability boundary prevents strike hits only during the invulnerable window.
- Process/CTest:
  - `ctest --preset msvc-debug-combat`
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- Sphere, capsule, and box combat volumes are represented and queryable.
- Hit volumes can be resolved from root or named proxy socket transforms supplied by tests.
- Swept fast-motion query catches a crossed target.
- Hit candidates are deterministically ordered independent of input container order.
- Once-per-target hit registry behavior is covered.
- Dodge invulnerability boundary behavior is covered.
- Existing Sprint 1 tests remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Keep primitive math simple but deterministic; do not overbuild a physics engine inside the sprint.
- Clearly label capsule/sweep approximations so future Jolt integration can replace them deliberately.
- Do not couple query ordering to vector insertion order.
- Avoid adding renderer-only concepts to collision data.

## Progress Log

- 2026-06-20: Started after SP1-007 combat runtime was merged.
- 2026-06-20: Added primitive combat volume helpers, root/proxy socket binding, overlap/sweep queries, stable hit ordering, once-per-target filtering, dodge invulnerability boundary coverage, and CTest coverage.

## Verification Results

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 5/5 tests.
- `ctest --preset msvc-debug-unit` passed: 11/11 tests.
- `ctest --preset msvc-debug-content` passed: 2/2 tests.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 13/13 tests, including `network_e2e` and `viewer_smoke`.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.
- No MSVC runtime/assertion windows were visible after verification.

## Final Commits

- `54e1699` Add SP1-008 primitive combat collision
