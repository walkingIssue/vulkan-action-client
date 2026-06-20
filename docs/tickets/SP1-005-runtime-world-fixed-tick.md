# SP1-005: RuntimeWorld and Fixed Tick Runner

Status: implemented
Branch: sprint01/sp1-005-runtime-world-fixed-tick
Start commit: `e187cfb`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 4: Architectural Decisions
- `docs/action_combat_engine_editor_design.md` section 5: Target Architecture
- `docs/action_combat_engine_editor_design.md` section 6.3: Authoring scene versus runtime world
- `docs/action_combat_engine_editor_design.md` section 13.3: InputFrame semantic input
- `docs/action_combat_engine_editor_design.md` section 17.3: Deterministic state hashes
- `docs/action_combat_engine_editor_design.md` section 19.5: Characterization suite before refactoring

## Goal

Introduce a headless fixed-tick simulation layer that can advance a minimal runtime world deterministically from semantic input frames, compute a stable gameplay state hash, and provide presentation interpolation samples without Vulkan, GLFW, XInput, or process/network dependencies.

## Non-Goals

- Do not implement move assets, attack phases, cancels, hitboxes, hurtboxes, or combat timeline logic.
- Do not replace `vulkan_scene_viewer` internals yet.
- Do not implement networking authority, prediction, reconciliation, rollback, or interpolation buffers.
- Do not build editor UI or scenario-runner CLI output; later tickets own those surfaces.
- Do not rewrite the whole engine into ECS.

## Current Baseline

- SP1-004 introduced `content_core::AuthoringScene`, primitive map validation, and a minimal compile DTO named `content::RuntimeWorld`.
- `combat_simulation` owns current actor movement helpers, fixed tick constants, and transform interpolation.
- Existing movement tests pin current locomotion semantics.
- There is no `simulation_core`, no semantic `InputFrame` stream, no repeatable fixed tick runner, and no state hash over gameplay-relevant fields.

## Data Flow

```text
AuthoringScene map JSON
  -> content_core load/validate/compile
  -> simulation_core RuntimeWorld import
  -> semantic InputFrame stream by tick
  -> fixed tick runner at 60 Hz
  -> actor state + eventless deterministic state hash
  -> presentation interpolation sample
  -> CTest fixture assertions across render cadences
```

Ownership boundaries:

- `content_core` remains responsible for authored schema and compile DTOs.
- `simulation_core` owns runtime actor state, fixed tick orchestration, semantic input playback, state hashing, and presentation sampling.
- `combat_simulation` movement helpers may be reused, but platform/window input must not enter `simulation_core`.

## Implementation Plan

1. Add a `simulation_core` library target.
2. Define `simulation::RuntimeWorld`, `RuntimeActor`, `InputFrame`, `InputStream`, `TickResult`, and fixed-step runner APIs.
3. Add import from `content::RuntimeWorld` to `simulation::RuntimeWorld`, using enabled spawn points as initial actor positions for now.
4. Advance actors through fixed ticks using semantic axes, camera/facing yaw, sprint flag, camera steering flag, and optional move-to target input.
5. Add deterministic state hashing with an explicit field order over tick, actor IDs, positions, yaws, speed/tuning-relevant flags, and active target state.
6. Add presentation interpolation helpers independent of Vulkan.
7. Add tests proving same input stream produces same hash across repeated runs and across simulated render cadences of 30, 60, 144, and irregular frame times.
8. Keep existing SP1-003 characterization and SP1-004 content tests passing.

## Test Plan

- Unit:
  - Import primitive arena runtime world from `content/maps/basic_primitive_arena.map.json`.
  - Same `InputFrame` stream produces the same final state hash across repeated runs.
  - Fixed tick result is identical for render cadences 30, 60, 144, and irregular frame times.
  - Presentation interpolation clamps and blends expected actor transforms.
- Process/CTest:
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug-content`
  - `ctest --preset msvc-debug-characterization`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- A headless `simulation_core` target exists and has no dependency on Vulkan, GLFW, WinSock, or platform input.
- Semantic `InputFrame` data can drive a fixed number of simulation ticks.
- Replaying the same input stream produces the same state hash.
- Render frame cadence does not affect the simulation hash.
- Presentation interpolation samples are available outside the renderer and covered by tests.
- Existing content, characterization, network, and viewer smoke tests remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Avoid duplicating movement rules; reuse the existing combat movement helpers until later extraction gives them a better home.
- Keep the state hash stable and documented, but do not pretend it is a network protocol hash yet.
- Decide explicitly how spawn points become actors in this temporary runtime import.
- Do not let render-cadence tests accidentally advance simulation from frame delta instead of fixed ticks.

## Progress Log

- 2026-06-20: Started after SP1-004 AuthoringScene v0 was merged.
- 2026-06-20: Added `simulation_core`, semantic input frames, fixed tick runner, deterministic state hash, render-cadence replay helper, and renderer-free presentation interpolation tests.

## Verification Results

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-simulation` passed: 1/1 test.
- `ctest --preset msvc-debug-unit` passed: 8/8 tests.
- `ctest --preset msvc-debug-content` passed: 1/1 test.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests.
- `ctest --preset msvc-debug` passed: 10/10 tests, including `network_e2e` and `viewer_smoke`.
- No MSVC runtime/assertion windows were visible after verification.

## Final Commits

- `7047932` Add SP1-005 fixed tick simulation runtime
