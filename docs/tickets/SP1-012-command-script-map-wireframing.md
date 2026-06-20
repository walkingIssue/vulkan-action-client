# SP1-012: Command-Script Map Wireframing

Status: planned
Branch: sprint01/sp1-012-command-script-map-wireframing
Start commit: 11b3220
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 9.9: Map authoring workflow
- `docs/action_combat_engine_editor_design.md` section 9.10: Editor regression tests
- `docs/action_combat_engine_editor_design.md` section 18.5: Required executable test switches

## Goal

Add a deterministic command-script runner for primitive map authoring operations so a non-UI workflow can create a small arena, save it, reopen it, compile it, and run fixed simulation ticks with structured result output.

## Non-Goals

- Do not build editor UI, undo/redo stacks, gizmos, selection, docking, or viewport interaction.
- Do not introduce imported static mesh authoring beyond primitive map data.
- Do not implement combat scenario golden traces; SP1-010 owns scenario runner behavior.
- Do not integrate visual debug rendering; SP1-011 owns visual lab behavior.
- Do not mutate the legacy Vulkan viewer for this ticket.

## Current Baseline

- `content_core` can load, validate, canonicalize, save, and compile primitive `AuthoringScene` map JSON.
- `simulation_core` can import compiled content runtime worlds and run deterministic fixed ticks.
- `host_core` already parses `--command-script`, `--result-file`, `--ticks`, `--headless`, and related common flags.
- There is no command-script map authoring runner, command fixture, or process test for the minimum map wireframing loop.

## Data Flow

```text
command script JSON
  -> map command runner
  -> in-memory AuthoringScene document
  -> save/reopen canonical map JSON
  -> content validation and compileRuntimeWorld
  -> simulation_core import and playTicks
  -> structured result JSON with created IDs, diagnostics, counts, ticks, and state hash
  -> CTest unit/process assertions
```

Ownership boundaries:

- `content_core` continues to own map schema, validation, canonical serialization, and compile DTOs.
- The command-script layer owns deterministic command application and result reporting.
- `simulation_core` owns fixed-tick playback once the map compiles.
- No renderer, platform input, networking, or editor UI should be required.

## Implementation Plan

1. Add a small map command-script library target with deterministic command execution.
2. Support `newFromTemplate`, `createPrimitive`, `setTransform`, `setBaseColor`, `addCollider`, `addSpawnPoint`, `setBounds`, `saveAs`, `reopen`, `compile`, `playTicks`, and `stop`.
3. Add a `map_command_runner` executable using `host_core` common flags, especially `--command-script`, `--result-file`, `--ticks`, and `--headless`.
4. Write structured result JSON with created object IDs, validation diagnostics, saved paths, compile counts, ticks played, and final state hash.
5. Add a command fixture that creates a small arena, saves, reopens, compiles, and runs 300 ticks.
6. Add unit coverage for deterministic command execution and process/CTest coverage for the executable result file.
7. Keep existing content, simulation, combat, and characterization tests passing.

## Test Plan

- Unit:
  - Command fixture creates deterministic authored IDs and canonical map output.
  - Save/reopen/compile succeeds and returns entity, collider, and spawn counts.
  - `playTicks` imports to simulation and reports a stable final state hash.
  - Invalid command scripts return structured diagnostics and non-ok status.
- Process/CTest:
  - `map_command_runner --command-script <fixture> --result-file <temp> --headless` exits successfully and writes created IDs, diagnostics, saved map path, compile counts, ticks, and state hash.
  - `ctest --preset msvc-debug-content`
  - `ctest --preset msvc-debug-simulation`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- A command-script runner executable exists and rejects unknown or unsupported options through `host_core`.
- The runner supports the required map authoring operations for a small primitive arena.
- A checked-in script creates, saves, reopens, compiles, and plays 300 ticks deterministically.
- Result files include created object IDs and validation diagnostics.
- Command execution is covered without UI automation.
- Existing Sprint 1 tests remain green.
- No generated build artifacts or unrelated untracked assets are committed.

## Risks and Watchpoints

- Avoid inventing a full editor document or undo model in this ticket.
- Keep template data small and explicit so later editor work can replace it cleanly.
- Do not hide validation failures; command errors should become structured diagnostics and nonzero process exits.
- Keep generated paths under build/test artifact directories in tests.
- Coordinate `CMakeLists.txt` changes with other agents.

## Progress Log

- 2026-06-20: Started after SP1-009 proxy animation was started in a parallel lane.

## Verification Results

Pending.

## Final Commits

Pending.
