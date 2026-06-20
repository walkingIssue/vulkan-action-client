# SP1-012 Result: Command-Script Map Wireframing

Status: complete
Branch: `sprint01/sp1-012-command-script-map-wireframing`
Start commit: `d45f91d`
Implementation commits:
- `e7252a6` Add SP1-012 map command runner
- `a9da27e` Harden map command save paths

## What Changed

- Added `map_command_core` for deterministic command-script execution over map authoring data.
- Added `map_command_runner`, a headless executable that uses `host_core` option parsing and writes structured JSON results.
- Supported the Sprint 1 map wireframing commands: `newFromTemplate`, `createPrimitive`, `setTransform`, `setBaseColor`, `addCollider`, `addSpawnPoint`, `setBounds`, `saveAs`, `save`, `reopen`, `compile`, `playTicks`, and `stop`.
- Added a checked-in arena command fixture at `tests/fixtures/commands/wire_arena.commands.json`.
- Added unit tests for deterministic execution, invalid command diagnostics, and path handling.
- Added CTest process coverage for the executable result file.

## Behavior Now Covered

- A command script can create a primitive arena, save canonical map JSON, reopen it, compile it through `content_core`, import it into `simulation_core`, and run fixed ticks.
- Result JSON includes created object IDs, diagnostics, saved map path, entity/collider/spawn counts, ticks played, and a deterministic state hash.
- Unknown commands and unsupported runner options fail with structured diagnostics and nonzero process exit behavior.
- Relative save paths are resolved against the script/base artifact directory, while explicit rooted paths remain allowed for tests and tools.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug --target map_command_runner_tests map_command_runner` passed.
- `ctest --test-dir build/msvc-debug -R "map_command_runner" --output-on-failure` passed: 2/2 tests.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-content --output-on-failure` passed: 4/4 tests.
- `ctest --preset msvc-debug-simulation --output-on-failure` passed: 1/1 tests.
- `ctest --preset msvc-debug --output-on-failure` passed: 16/16 tests after restoring ignored local extracted assets required by pre-existing characterization/viewer tests.

The SP1-012 process artifact reported:

```json
{
  "status": "ok",
  "commandCount": 17,
  "entityCount": 2,
  "colliderCount": 2,
  "spawnPointCount": 2,
  "ticks": 300,
  "stateHash": 10715893869485234459
}
```

## Residual Risks

- The command-script layer is intentionally narrow and does not replace a future editor document, undo stack, or UI command model.
- Primitive arena authoring is covered; imported static mesh authoring remains out of scope.
- Full debug CTest depends on ignored local extracted paladin assets for existing characterization/viewer tests.

## Next Recommended Ticket

Proceed to `SP1-010: Combat Scenario Runner and Golden Traces`, which can consume the headless map/run pattern established here for deterministic combat scenario traces.
