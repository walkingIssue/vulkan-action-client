# SP1-010 Result: Combat Scenario Runner and Golden Traces

Status: complete
Branch: `sprint01/sp1-010-scenario-runner-goldens`
Start commit: `8309d55`
Implementation commit: `d5f9679`

## What Changed

- Added `combat_scenario_core` for scenario schema loading, validation, execution, trace emission, golden comparison, and result JSON writing.
- Added `combat_scenario_runner` with `--scenario`, `--result-file`, `--ticks`, `--seed`, `--headless`, and guarded `--update-golden`.
- Added four scenario fixtures: hit, whiff, cancel-on-hit, and dodge-invulnerability boundary.
- Added four checked-in readable golden trace JSON files.
- Added scenario unit/integration tests plus process-level CTest runner coverage.
- Reconciled `CMakeLists.txt` with the SP1-012 map-command runner work from remote `main`.

## Behavior Now Covered

- Scenario runs load map, move, proxy animation, actor placement, and semantic input data.
- Fixed-tick traces include move starts, phase entries, authored events, hits, whiffs, blocked hits, cancels, completion, and final state hash.
- Repeated runs produce identical traces.
- Golden mismatches report expected and actual event snippets with tick context.
- Golden updates fail unless both `--update-golden` and `VAC_UPDATE_GOLDENS=1` are present.
- Process CTests assert each scenario fixture through `combat_scenario_runner`.

## Verification

- `cmake --preset msvc-debug` passed.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-combat` passed: 12/12 tests.
- `ctest --preset msvc-debug-unit` passed: 14/14 tests.
- `ctest --preset msvc-debug` passed: 22/22 tests.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.
- `build/msvc-debug/test-artifacts/scenario_hits_result.json` reported `status: ok`, `host: combat_scenario_runner`, `goldenCompared: true`, and `goldenMatched: true`.

No MSVC runtime/assertion dialog was visible after the verification run.

## Residual Risks

- Scenario runner still orchestrates hit and cancel signals around the existing combat runtime rather than a fully integrated damage/hit-reaction system.
- Actor placement and hurtboxes are scenario-authored, not yet derived from final character/animation assets.
- Golden traces are readable JSON but still need governance as combat language grows.
- The runner is headless only; SP1-011 still needs visual debug inspection for maps, proxy sockets, hitboxes, and interpolation samples.

## Next Recommended Ticket

Proceed to `SP1-011: Visual Map and Combat Lab`, using the scenario fixtures and traces from this ticket as the data source for debug rendering and playback inspection.
