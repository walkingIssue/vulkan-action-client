# SP1-011 Result: Visual Map and Combat Lab

Status: complete
Branch: `sprint01/sp1-011-visual-combat-lab`
Start commit: `713afe1`
Implementation commit: `b35d713`

## What Changed

- Added `visual_lab_core` for building a temporary Sprint 1 visual lab scene from the checked-in primitive arena, light attack move, and sword proxy animation.
- Converted compiled primitive map entities into renderer-friendly procedural geometry with base colors.
- Extended scene/render geometry support for authored boxes and actor-root proxy boxes.
- Added debug overlays for world bounds, spawn markers, actor roots, move phase windows, hitbox/hurtbox boxes, proxy sockets, root-motion paths, and interpolation samples.
- Added `vulkan_scene_viewer --visual-lab --offline` with result-file diagnostics for overlay counts.
- Added `visual_lab_tests` and `visual_lab_smoke` CTest coverage.

## Behavior Now Covered

- The visual lab renders Sprint 1 primitive map data without imported paladin model assets.
- Offline lab mode avoids network actor/client initialization.
- The viewer result file identifies visual lab mode and reports deterministic overlay counts.
- Existing scenario/combat tests from SP1-010 remain green on the combined Sprint 1 branch.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer` passed.
- `ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed: 2/2 tests.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-viewer --output-on-failure` passed: 3/3 tests.
- `ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13 tests.
- `ctest --preset msvc-debug --output-on-failure` passed: 24/24 tests.

The visual lab smoke artifact reported:

```json
{
  "status": "ok",
  "host": "vulkan_scene_viewer",
  "frames": 3,
  "ticks": 20,
  "diagnostics": [
    "visualLab=true",
    "primitiveCount=3",
    "colliderCount=3",
    "spawnPointCount=2",
    "actorRootCount=2",
    "movePhaseWindowCount=3",
    "hitboxVolumeCount=1",
    "hurtboxVolumeCount=1",
    "proxySocketMarkerCount=3",
    "rootMotionPathSegmentCount=2",
    "interpolationSampleCount=3",
    "debugLineVertexCount=208"
  ]
}
```

## Residual Risks

- The lab is still a temporary inspection path, not the final editor/debug-render architecture.
- Scenario playback controls are not wired into the viewer yet; this first slice renders the Sprint 1 content assets directly.
- Box rendering approximates non-box primitive debug geometry until a richer primitive renderer/debug API exists.
- Full legacy viewer and characterization tests still depend on ignored local extracted paladin assets.

## Next Recommended Ticket

Sprint 1 implementation is complete. Proceed to a risk-mitigation minisprint based on the Sprint 1 result docs, with special attention to asset hydration, duplicate visual-lab branch coordination, and hardening the temporary visual/debug paths.
