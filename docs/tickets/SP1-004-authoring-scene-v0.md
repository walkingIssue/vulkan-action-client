# SP1-004: AuthoringScene v0 and Primitive Map Schema

Status: planned
Branch: sprint01/sp1-004-authoring-scene-v0
Start commit: pending
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 4: Architectural Decisions
- `docs/action_combat_engine_editor_design.md` section 5: Target Architecture
- `docs/action_combat_engine_editor_design.md` section 6.3: Authoring scene versus runtime world
- `docs/action_combat_engine_editor_design.md` section 6.4: Scene file shape
- `docs/action_combat_engine_editor_design.md` section 7.5: Primitive assets
- `docs/action_combat_engine_editor_design.md` section 9.7: Edit, simulate, and play modes

## Goal

Introduce the first versioned map-authoring data model so primitive arenas can be loaded, validated, saved canonically, and compiled into minimal runtime-ready world data. This ticket creates the data foundation for map design without building the full editor UI or fixed-tick simulation loop yet.

## Non-Goals

- Do not build `game_editor` UI, gizmos, hierarchy, inspectors, undo, or docking.
- Do not replace the existing Vulkan bootstrap viewer scene path yet.
- Do not implement fixed-tick simulation, input streams, state hashes, or render-cadence determinism; that is SP1-005.
- Do not add move assets, proxy animation, hitboxes, or combat runtime behavior.
- Do not introduce a generic ECS rewrite.

## Current Baseline

- `config/scenes/bootstrap.scene.json` is a viewer-oriented scene file with models, procedural floor, and entities.
- `scene_core` loads current viewer scene data and imports Assimp models.
- SP1-003 added characterization tests that pin current bootstrap scene, movement, packet, relay, and viewer smoke behavior.
- There is no `AuthoringScene`, no versioned primitive map schema, no canonical save path, no structured content diagnostics, and no compile boundary from authored map data to runtime world data.

## Data Flow

```text
map JSON fixture
  -> content_core AuthoringScene loader
  -> schema/version/default validation
  -> structured diagnostics with file/object/component/field paths
  -> canonical JSON writer with deterministic ordering
  -> minimal RuntimeWorld compile output
  -> CTest fixture and invalid-case assertions
```

Ownership boundaries:

- `content_core` owns authored map schema, canonical serialization, validation, and compile DTOs.
- `scene_core` remains the current viewer scene loader for now.
- Runtime compile output is a minimal data target for tests and future SP1-005 extraction, not a full simulation world.

## Implementation Plan

1. Add a `content_core` library target with authored IDs, runtime IDs, transforms, primitives, colliders, spawn points, world bounds, diagnostics, and runtime compile DTOs.
2. Define map schema v1 with deterministic JSON fields for schema version, asset GUID/logical ID, units, world bounds, kill height, primitive objects, colliders, and spawn points.
3. Add strong ID wrappers for authored object IDs and runtime entity IDs while keeping JSON logical IDs human-readable.
4. Implement load, validate, canonical save, and compile functions.
5. Validate duplicate object IDs, invalid transforms, NaN/Inf values, missing parents, invalid collider sizes, invalid world bounds, and missing spawn points.
6. Add map fixtures for a valid primitive combat arena and focused invalid cases.
7. Add `authoring_scene_tests` under CTest labels `unit;content;authoring;map`.
8. Keep SP1-003 characterization tests passing.

## Test Plan

- Unit:
  - Valid map fixture loads, validates, saves canonically, and round-trips.
  - Canonical save output is stable across repeated loads/saves.
  - Invalid fixtures reject duplicate IDs, invalid transforms, NaN/Inf, missing parents, invalid collider sizes, invalid world bounds, and missing spawn points.
- Integration:
  - Compile primitive map to minimal runtime world and compare runtime entity count, spawn count, world bounds, kill height, primitive bounds, and collider metadata.
- Process/CTest:
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug-characterization`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- Versioned primitive map JSON can be loaded into `AuthoringScene`.
- Valid map fixture round-trips through canonical save without nondeterministic ordering.
- Validation diagnostics include file, object, component, and field path where applicable.
- Invalid authoring fixtures fail for the planned validation cases with assertive tests.
- Compile path creates minimal runtime world data with stable runtime IDs and expected bounds/entity counts.
- Existing SP1-003 characterization coverage and full debug CTest remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Avoid making the compile DTO too large; SP1-005 should still own fixed-tick runtime extraction.
- Avoid coupling the authoring schema to the existing viewer-only `bootstrap.scene.json`.
- Canonical save should use structured JSON APIs and explicit ordering, not ad hoc string manipulation.
- ID wrappers should be lightweight and compiler-friendly, but this ticket should not chase clever zero-cost abstraction machinery at the expense of clarity.

## Progress Log

- 2026-06-20: Started after SP1-003 characterization was merged.

## Verification Results

Pending.

## Final Commits

Pending.
