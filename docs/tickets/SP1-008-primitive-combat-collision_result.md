# SP1-008 Result: Primitive Combat Collision v0

Status: complete
Branch: `sprint01/sp1-008-primitive-combat-collision`
Start commit: `95a0d70`
Implementation commit: `54e1699`

## What Changed

- Added primitive combat volumes for sphere, capsule, and box under `combat_runtime_core`.
- Added root and named proxy socket volume binding through supplied socket transforms.
- Added overlap checks for sprint v0 fixtures and swept segment/capsule-style crossing checks for fast weapon motion.
- Added deterministic `CombatHitCandidate` ordering by attacker, move instance sequence, hitbox track ID, victim, and contact sub-index.
- Added once-per-target hit registry filtering.
- Added dodge/strike-invulnerability boundary filtering through target state tags.
- Added `combat_collision_tests` under `unit;combat;collision`.

## Behavior Now Covered

- Hitbox windows use half-open tick ranges.
- Root-bound and proxy socket-bound volumes resolve to expected world positions.
- Swept weapon motion catches a target crossed between previous and current tick positions.
- Multi-target hit order is stable across insertion order.
- Once-per-target filtering suppresses duplicate victim hits.
- Dodge invulnerability blocks strike hits during the authored window and allows hits on boundary ticks outside it.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 5/5 tests.
- `ctest --preset msvc-debug-unit` passed: 11/11 tests.
- `ctest --preset msvc-debug-content` passed: 2/2 tests.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 13/13 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Box math is axis-aligned v0, not full oriented-box collision.
- Capsule checks use a deterministic approximation suitable for sprint fixtures; later Jolt or richer geometry can replace it.
- Collision query results are not yet fed into combat runtime damage/hit reaction.
- Proxy socket transforms are supplied manually by tests; SP1-009 still needs authored keyframe/socket sampling.

## Next Recommended Ticket

Proceed to `SP1-009: Proxy Animation, Keyframes, and Interpolation`, which should provide the socket transforms consumed by these collision queries.
