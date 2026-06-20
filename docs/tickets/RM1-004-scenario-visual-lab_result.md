# RM1-004 Result: Scenario-Driven Visual Lab Diagnostics

Status: complete
Branch: `mitigation01/rm1-004-scenario-visual-lab`
Start commit: `b937b33`
Implementation commit: `de92a82`

## What Changed

- Added narrow combat scenario helpers for resolved scenario asset paths and scenario-authored runtime actor placement.
- Added `visual_lab_core` support for building a visual lab scene from a checked-in scenario fixture while preserving the direct map/move/proxy-animation path.
- Reused scenario golden comparison output to add viewer result diagnostics for scenario id, status, golden compared/matched state, trace event count, final state hash, ticks run, golden path, and scenario message.
- Added `vulkan_scene_viewer --visual-lab --scenario <path> --offline`.
- Added unit coverage for scenario-driven visual lab diagnostics.
- Added `visual_lab_scenario_smoke` CTest coverage that writes a structured viewer result file.

## Behavior Now Covered

- Existing direct-asset visual lab mode still builds and smokes through CTest.
- Scenario-driven visual lab mode uses scenario map/move/animation references and scenario-authored actor placements.
- The viewer result file now links visual inspection output to headless scenario/golden evidence.
- The scenario smoke proves `scenario.sword_light_hits_idle_target` matches its golden and reports the expected final state hash.

## Verification

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target visual_lab_tests vulkan_scene_viewer` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R "visual_lab" --output-on-failure` passed: 3/3.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-viewer --output-on-failure` passed: 5/5.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

The scenario visual lab smoke artifact reported:

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
    "scenarioTraceEventCount=8",
    "scenarioFinalStateHash=0x899156866903d60e"
  ]
}
```

## Residual Risks

- The lab still visualizes one representative scenario move/animation binding rather than full timeline playback.
- Viewer diagnostics remain structured smoke evidence; combat correctness still belongs to scenario runner goldens and headless tests.
- Scenario helpers expose only narrow path/runtime placement reuse, not a full asset registry or editor dependency graph.
- Full editor controls, live asset editing, and timeline scrubbing remain deferred.

## Next Recommended Ticket

Finish the risk mitigation minisprint by reviewing RM1 results against `docs/sprint-01-risk-mitigation-plan.md`, then update the workflow docs with the observed multi-agent, Windows lock, and verification lessons before deriving the next design-driven sprint.
