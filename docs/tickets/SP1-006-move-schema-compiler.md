# SP1-006: Move Schema v0 and Compiler

Status: implemented
Branch: sprint01/sp1-006-move-schema-compiler
Start commit: `080eacc`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 13.3: InputFrame semantic input
- `docs/action_combat_engine_editor_design.md` section 13.4: Move phases and state tags
- `docs/action_combat_engine_editor_design.md` section 13.5: Cancels
- `docs/action_combat_engine_editor_design.md` section 13.6: Hit/hurt volumes
- `docs/action_combat_engine_editor_design.md` section 14.3: Move timeline editor target

## Goal

Define the first move asset schema and compiler so combat moves can be authored as versioned JSON, validated with precise diagnostics, and compiled into deterministic runtime lookup tables with interned IDs. This establishes the combat-language data contract before SP1-007 executes moves in runtime actor state.

## Non-Goals

- Do not execute moves, match commands, resolve cancels, apply hitstop/stun, or detect hits; SP1-007 and SP1-008 own runtime behavior.
- Do not build the move timeline editor UI.
- Do not add skeletal animation, proxy sockets, or root-motion sampling yet; SP1-009 owns proxy animation.
- Do not add scenario runner/golden traces yet; SP1-010 owns that surface.

## Current Baseline

- SP1-005 added semantic `InputFrame` and fixed tick simulation, but there is no authored move data.
- `content_core` owns versioned map schemas and structured diagnostics.
- No move IDs, tag IDs, event IDs, track IDs, cancel windows, hitbox tracks, or hurtbox override data are compiled yet.

## Data Flow

```text
move JSON fixture
  -> content_core MoveAsset loader
  -> schema and tick-range validation
  -> structured diagnostics with file/object/component/field paths
  -> deterministic compiler and intern tables
  -> compiled move lookup tables
  -> CTest valid/invalid fixture assertions
```

Ownership boundaries:

- `content_core` owns move JSON schema, validation, and compiled data tables.
- `simulation_core` consumes compiled move tables later but does not execute them in this ticket.
- Runtime data uses interned IDs; authored strings remain stable JSON-facing logical IDs.

## Implementation Plan

1. Add move asset types under `content_core`: move IDs, tag IDs, event IDs, track IDs, tick ranges, phases, input trigger, movement tracks, hitbox tracks, hurtbox overrides, cancel windows, resource placeholder, and events.
2. Add JSON load/canonical save helpers for move schema v1.
3. Validate duration, half-open tick ranges `[begin, end)`, event ticks, unknown tags, unknown cancel destinations, duplicate track IDs, empty cancel destinations, and same-tick self-cancel cycles.
4. Compile valid move JSON into deterministic tables sorted by stable IDs, independent of JSON array order.
5. Intern move IDs, tags, event IDs, and track IDs into compact runtime IDs.
6. Add a valid light attack fixture and focused invalid fixtures.
7. Add `move_asset_tests` under CTest labels `unit;content;combat;move`.
8. Keep content, simulation, characterization, and full debug CTest green.

## Test Plan

- Unit:
  - Valid light attack fixture loads, validates, and compiles.
  - Compiled move tables are identical after reordering JSON arrays.
  - Invalid fixtures reject invalid tick ranges, unknown tags, unknown destination moves, duplicate track IDs, empty cancel destinations, and same-tick self-cancel cycles.
- Process/CTest:
  - `ctest --preset msvc-debug-content`
  - `ctest --preset msvc-debug-simulation`
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- Move schema v1 includes stable logical ID, duration, phases, input trigger, movement tracks, hitbox tracks, hurtbox overrides, cancel windows, resources placeholder, and events.
- All tick windows use half-open `[begin, end)` validation.
- Compiled move data uses interned move, tag, event, and track IDs.
- Invalid move fixtures produce precise structured diagnostics.
- Compiled output is deterministic regardless of JSON object/array ordering where ordering is not semantically meaningful.
- Existing Sprint 1 tests remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Avoid designing the full future timeline editor format; keep v1 compact and evolvable.
- Do not let validation depend on JSON insertion order.
- Keep same-tick cycle detection narrow and documented so SP1-007 can expand cancel graph semantics intentionally.
- Keep runtime execution out of this ticket even if tests make execution tempting.

## Progress Log

- 2026-06-20: Started after SP1-005 fixed tick runtime was merged.
- 2026-06-20: Added move schema v1, compiler, intern tables, canonical JSON, valid light attack fixture, invalid diagnostics fixtures, and CTest coverage.

## Verification Results

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-content` passed: 2/2 tests.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 9/9 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 11/11 tests, including `network_e2e` and `viewer_smoke`.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.
- No MSVC runtime/assertion windows were visible after verification.

## Final Commits

- `7e8ae95` Add SP1-006 move schema compiler
