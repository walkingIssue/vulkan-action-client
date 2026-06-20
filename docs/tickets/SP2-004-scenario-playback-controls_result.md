# SP2-004 Result: Scenario Playback Controls in Visual Lab v1

Status: ready for merge
Branch: `sprint02/sp2-004-scenario-playback-controls`
Start commit: `79cca54`
Implementation commit: `fd11fd3`

## What Changed

- Added a small `visual_lab_core` playback state model for scenario evidence: duration/current tick, paused/running state, step, reset, and bounded seek.
- Added scenario evidence summaries derived from `ScenarioTrace` events: event count, first/last tick, effect/hit/blocked-hit counts, total/max damage, lowest remaining health, and reaction moves.
- Threaded playback and evidence diagnostics into scenario visual lab result diagnostics without adding combat rules to viewer code.
- Extended `visual_lab_tests` with playback step/reset/seek coverage and scenario trace damage/reaction/effect summary coverage.
- Preserved the existing direct-asset visual lab path and scenario smoke path.

## Behavior Now Covered

- Scenario visual lab mode exposes deterministic playback state for checked-in scenario traces.
- Result diagnostics prove scenario/golden evidence loaded and expose a deterministic playback preview:
  - `visualLabPlayback=true`
  - `playbackCurrentTick=0`
  - `playbackDurationTicks=36`
  - `playbackPreviewStepTick=1`
  - `playbackPreviewResetTick=0`
  - `playbackPreviewSeekEndTick=36`
- Result diagnostics now summarize combat evidence without recomputing combat rules:
  - `scenarioEvidenceEventCount=13`
  - `scenarioEvidenceEffectEventCount=1`
  - `scenarioEvidenceHitEventCount=1`
  - `scenarioEvidenceTotalDamage=12`
  - `scenarioEvidenceLowestRemainingHealth=88`
  - `scenarioEvidenceReactionMoves=move.hit_reaction`

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target visual_lab_tests` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed: 3/3.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer` passed and refreshed the scenario smoke executable.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed again after rebuilding `vulkan_scene_viewer`: 3/3.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed with no work to do.
- `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug-viewer --output-on-failure` passed: 5/5.
- `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; Remove-Item Env:VAC_UPDATE_GOLDENS -ErrorAction SilentlyContinue; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

The refreshed scenario visual lab smoke artifact reported the new evidence:

```json
{
  "status": "ok",
  "host": "vulkan_scene_viewer",
  "frames": 3,
  "ticks": 20,
  "diagnostics": [
    "visualLab=true",
    "visualLabSource=scenario",
    "scenarioId=scenario.sword_light_hits_idle_target",
    "scenarioStatus=ok",
    "scenarioGoldenCompared=true",
    "scenarioGoldenMatched=true",
    "scenarioTraceEventCount=13",
    "scenarioFinalStateHash=0xf73237aa3baea830",
    "visualLabPlayback=true",
    "playbackPreviewStepTick=1",
    "playbackPreviewResetTick=0",
    "playbackPreviewSeekEndTick=36",
    "scenarioEvidenceEventCount=13",
    "scenarioEvidenceTotalDamage=12",
    "scenarioEvidenceReactionMoves=move.hit_reaction"
  ]
}
```

## Intentional Golden Drift

- None. Scenario goldens remained compare-only; `VAC_UPDATE_GOLDENS` was cleared for broad verification.

## Asset / Local State

- The untracked local `assets/` directory remained untouched and unstaged.

## Residual Risks

- Playback controls are a deterministic model and structured diagnostics layer, not a full editor timeline UI.
- The viewer still renders a representative scenario scene rather than animating every trace event over time.
- The visual lab consumes current `ScenarioTrace` fields directly; a later trace schema expansion may need companion summary fields.

## Next Recommended Ticket

Proceed only if Mia dispatches the next lane. `SP2-005 Network Compatibility Snapshot v1` remains held at the time of this result.
