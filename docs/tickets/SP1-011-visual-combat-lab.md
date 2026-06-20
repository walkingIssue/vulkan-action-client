# SP1-011: Visual Map and Combat Lab

Status: complete
Branch: sprint01/sp1-011-visual-combat-lab
Start commit: `713afe1`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 9.2: Required panels
- `docs/action_combat_engine_editor_design.md` section 13.11: Move editor and preview requirements
- `docs/action_combat_engine_editor_design.md` section 13.12: Combat test arena
- `docs/action_combat_engine_editor_design.md` section 19.5: Characterization before extraction
- `docs/action_combat_engine_editor_design.md` section 24: Priority feature backlog

## Goal

Add a temporary visual lab mode to the existing Vulkan viewer so Sprint 1 map, move, collision, and proxy animation data can be inspected visually through primitive geometry and debug overlays without waiting for the full editor shell.

## Non-Goals

- Do not build the Dear ImGui editor, docking UI, hierarchy, inspector, asset browser, or timeline editor.
- Do not implement scenario golden trace orchestration; SP1-010 owns the headless scenario runner.
- Do not add skeletal animation, GPU skinning, imported static mesh map rendering, or production debug-render architecture.
- Do not make rendering the authority for combat correctness.
- Do not rewrite the legacy viewer into final engine/editor ownership.

## Current Baseline

- `vulkan_scene_viewer` can load the legacy bootstrap scene, render static triangles/lines, run a fixed-frame smoke test, and write a basic result file.
- `content_core` can load primitive authoring maps and move assets.
- `simulation_core` can import compiled primitive maps and create actors from spawn points.
- `combat_runtime_core` exposes authored move phases, hitbox/hurtbox tracks, and collision query helpers.
- `animation_core` can sample proxy root and socket keyframes.
- `render/scene_geometry` can draw the legacy floor grid and scene bounds, but it does not render authored primitive map boxes or Sprint 1 combat debug overlays yet.
- SP1-010 scenario playback has merged, but this ticket keeps the first visual lab slice on existing content assets rather than depending on scenario playback control flow.

## Data Flow

```text
primitive map JSON + move JSON + proxy animation JSON
  -> visual lab loader
  -> content validation and compileRuntimeWorld
  -> simulation RuntimeWorld actor import
  -> renderer-friendly primitive scene + debug overlay line set
  -> vulkan_scene_viewer --visual-lab --offline fixed-frame smoke
  -> result JSON and CTest assertions
```

Ownership boundaries:

- `content_core`, `simulation_core`, `combat_runtime_core`, and `animation_core` remain the source of data semantics.
- The visual lab owns temporary inspection geometry and overlay summaries only.
- The Vulkan viewer consumes prebuilt scene/debug data and should not duplicate validation or combat rules.
- Scenario playback integration remains a follow-up hook after SP1-010 lands.

## Implementation Plan

1. Add a small visual lab library that loads the checked-in primitive arena, light attack move, and sword proxy animation.
2. Convert compiled primitive map entities into renderer-friendly procedural geometry with base colors.
3. Add debug overlay generation for world bounds, spawn points, actor root boxes, move phase windows, hit/hurt volumes, proxy sockets, root-motion paths, and interpolation samples.
4. Add `vulkan_scene_viewer --visual-lab --offline` support using `host_core` common flags and default Sprint 1 lab assets.
5. Keep network initialization disabled in offline visual lab mode.
6. Add unit coverage for visual lab scene/overlay summaries.
7. Add a viewer CTest smoke that launches the visual lab for fixed frames and writes a result file.

## Test Plan

- Unit:
  - Visual lab loads the checked-in primitive arena, compiles it, and reports primitive, collider, spawn, and actor counts.
  - Debug overlays include spawn markers, actor roots, move phase windows, hitbox/hurtbox boxes, proxy socket markers, root-motion path segments, and interpolation samples.
  - Missing or invalid lab assets produce diagnostics instead of silent empty overlays.
- Process/CTest:
  - `vulkan_scene_viewer --visual-lab --offline --scene content/maps/basic_primitive_arena.map.json --frames 3 --result-file <temp>` exits successfully.
  - The result file identifies visual lab mode and includes overlay diagnostics.
  - `ctest --preset msvc-debug-viewer`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- A visual lab mode exists on the viewer and rejects unsupported or unknown options clearly.
- The lab renders Sprint 1 primitive map geometry using base colors.
- The lab overlays actor roots, bounds, spawn points, move phases, hitboxes, hurtboxes, proxy sockets, root-motion paths, and interpolation samples.
- Offline lab mode does not initialize networking.
- A fixed-frame Vulkan smoke test covers the lab path.
- The implementation stays compatible with SP1-010 by using existing content assets and leaving scenario playback as an integration hook.
- No generated build artifacts or unrelated untracked assets are committed.

## Risks and Watchpoints

- Keep the viewer changes narrow; extract helpers instead of burying combat/data logic in `vulkan_scene_viewer.cpp`.
- `CMakeLists.txt` and `src/vulkan_scene_viewer.cpp` are high-conflict files, so reconcile remote and the agent board before merge.
- Visual overlays are inspection aids, not a gameplay oracle.
- Avoid adding opaque screenshot goldens until render capture tooling is stable.
- The existing full viewer smoke still needs ignored local extracted paladin assets; the new visual lab smoke should not.

## Progress Log

- 2026-06-20: Claimed as Aetoun after SP1-012 merged and while Lara/Vera work on SP1-010 in parallel.
- 2026-06-20: Reconciled onto the pushed SP1-010 merge before implementation.
- 2026-06-20: Added `visual_lab_core`, primitive map geometry rendering, debug overlay summaries, `vulkan_scene_viewer --visual-lab --offline`, unit tests, and a process smoke.

## Verification Results

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer` passed.
- `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed: 2/2 tests.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-viewer --output-on-failure` passed: 3/3 tests.
- `ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13 tests.
- `ctest --preset msvc-debug --output-on-failure` passed: 24/24 tests.
- `build/msvc-debug/test-artifacts/visual_lab_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, `frames: 3`, `ticks: 20`, and visual lab overlay diagnostics.
- Full legacy viewer/characterization tests still require ignored local `assets/extracted` paladin files; the new `visual_lab_smoke` path does not depend on imported model assets.

## Final Commits

- `b35d713` Add SP1-011 visual combat lab
