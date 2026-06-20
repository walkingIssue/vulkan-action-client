# SP1-007 Result: Combat Runtime v0

Status: complete
Branch: `sprint01/sp1-007-combat-runtime-v0`
Start commit: `243b966`
Implementation commit: `aebfe8c`

## What Changed

- Added `combat_runtime_core`, a fixed-tick runtime consumer of compiled move data and semantic `simulation::InputFrame` command presses.
- Added `CombatActorState` with active move, move instance sequence, move tick, state tags, hitstop/stun counters, command buffer, and hit registry placeholder.
- Added deterministic `CombatMoveLibrary` over compiled moves.
- Added command buffering, authored move start/completion, startup/active/recovery phase events, authored move events, and cancel checks for hit/block/whiff/always placeholders.
- Added authored `dodge`, `hit_reaction`, and `light_followup` move files.
- Added `combat_runtime_tests` under `unit;combat;runtime`.

## Behavior Now Covered

- Light attack starts from semantic command input and emits move start, phase enter, authored event, and completion events.
- Buffered dodge command starts after light attack recovery when still inside the buffer window.
- Cancel-on-hit transitions from light attack to authored light follow-up.
- Hit-only cancel does not fire on whiff.
- Always cancel transitions to authored dodge.
- Hitstop and stun counters decrement deterministically and stun blocks move start.
- Event sequence summaries are stable across repeated runs.
- Dodge and hit reaction are present as authored compiled moves rather than runtime branches.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 4/4 tests.
- `ctest --preset msvc-debug-content` passed: 2/2 tests.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 10/10 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 12/12 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Cancel signals are explicit test placeholders; SP1-008 must replace them with real hit/hurt overlap results.
- Command matching has deterministic command-to-move lookup, but no authored priority field yet.
- Runtime move IDs are assigned by library logical ID order; a future cooker may need stable cross-asset package IDs.
- Viewer/client is still not integrated with combat runtime.

## Next Recommended Ticket

Proceed to `SP1-008: Primitive Combat Collision v0`, feeding real primitive hit/hurt results into the cancel and hit registry paths introduced here.
