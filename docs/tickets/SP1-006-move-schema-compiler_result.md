# SP1-006 Result: Move Schema v0 and Compiler

Status: complete
Branch: `sprint01/sp1-006-move-schema-compiler`
Start commit: `080eacc`
Implementation commit: `7e8ae95`

## What Changed

- Added move schema v1 to `content_core` with stable logical ID, duration, tags, known move references, input trigger, phases, movement tracks, hitbox tracks, hurtbox overrides, cancel windows, resource placeholder, and events.
- Added canonical JSON output for move assets with deterministic ordering.
- Added validation for half-open tick ranges, unknown tags, unknown cancel destinations, duplicate track IDs, empty cancel destinations, same-tick self-cancel cycles, event ticks, and malformed hitbox/hurtbox/movement data.
- Added compiler output with interned move IDs, tag IDs, event IDs, and track IDs.
- Added `content/moves/light_attack.move.json` and focused invalid fixtures.
- Added `move_asset_tests` under `unit;content;combat;move`.

## Behavior Now Covered

- Valid light attack fixture loads, validates, canonical-round-trips, and compiles.
- Compiled move tables are deterministic when authoring arrays are reordered.
- Invalid fixtures assert precise diagnostic code, component, and field path for invalid tick ranges, unknown tags, unknown destination moves, duplicate track IDs, empty cancel destinations, and zero-tick self-cancel cycles.
- Existing content map, simulation, movement, network, characterization, and viewer smoke tests remain green.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-content` passed: 2/2 tests.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 9/9 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 11/11 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Move schema v1 is intentionally compact and may need migration once proxy animation, socket sampling, resources, and hit effects become richer.
- Cancel graph validation only catches the narrow same-move tick-zero cycle required for this ticket; SP1-007 should expand runtime cancel semantics deliberately.
- Hitbox and hurtbox data are compiled but not executed yet, so SP1-008 must validate runtime overlap behavior.
- Compiled data uses strings for some shape/socket/type fields while IDs are interned for moves/tags/events/tracks; later cooker work may intern more domains.

## Next Recommended Ticket

Proceed to `SP1-007: Combat Runtime v0`, consuming the compiled move schema through semantic `InputFrame` command matching and fixed tick actor state.
