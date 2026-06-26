# SP3-002: Split Vulkan Scene Viewer Modules

Status: planned
Branch: sprint03/sp3-002-split-vulkan-scene-viewer-modules
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Hot files:
- `src/vulkan_scene_viewer.cpp`
- `CMakeLists.txt`
- new `src/viewer/*`

## Goal

Split the oversized `src/vulkan_scene_viewer.cpp` into behavior-preserving modules so rendering/viewer infrastructure can be reused by `game_editor` without duplicating a monolith.

## Non-Goals

- Do not change viewer CLI behavior or result diagnostics.
- Do not add editor UI features.
- Do not alter scene, combat, visual-lab, or network semantics.
- Do not make broad rendering design changes beyond extraction.

## Target Module Boundaries

- `viewer/viewer_options.*`: CLI options and result diagnostics.
- `viewer/viewer_camera.*`: camera view, bounds anchoring, follow/orbit state.
- `viewer/viewer_scene_state.*`: scene, visual-lab, network snapshot, and presentation state assembly.
- `viewer/vulkan_context.*`: instance/device/queues/swapchain/render pass/framebuffers/command buffers/sync.
- `viewer/vulkan_resources.*`: buffers, images, uploads, model-buffer ownership.
- `viewer/scene_renderer.*`: render-data upload and draw entry points.
- `src/vulkan_scene_viewer.cpp`: thin executable composition and `main`.

## Acceptance Criteria

- `vulkan_scene_viewer.cpp` is reduced to thin host glue.
- Public behavior and result JSON fields stay compatible.
- Viewer smoke tests produce equivalent diagnostics.
- Module headers expose only what `game_editor` or the viewer host needs.
- No editor feature work is hidden in this refactor.

## Test Plan

- Build `vulkan_scene_viewer`.
- `ctest --preset msvc-debug-viewer --output-on-failure`.
- Full `ctest --preset msvc-debug --output-on-failure` if CMake/shared renderer surfaces move.
- Visual QA screenshot/capture if rendering paths visibly change.
- MSVC runtime/assertion dialog check.
