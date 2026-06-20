# SP1-003: Characterization Before Extraction

Status: implemented
Branch: sprint01/sp1-003-characterization
Start commit: `63c66bf`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 2: Current Baseline
- `docs/action_combat_engine_editor_design.md` section 5.4: Architecture regression requirements
- `docs/action_combat_engine_editor_design.md` section 19.5: Characterization suite before refactoring
- `docs/action_combat_engine_editor_design.md` section 27: Recommended First Engineering Tasks

## Goal

Capture the current behavior of the bootstrap scene, movement/interpolation semantics, packet protocol, relay cache behavior, and viewer smoke path before extracting runtime/editor/client/server ownership. Future tickets should be able to refactor with confidence because these tests fail on unintended behavior changes.

## Non-Goals

- Do not extract `RuntimeWorld` yet.
- Do not introduce the new map schema.
- Do not add move assets, proxy animation assets, or combat timeline logic.
- Do not change network semantics.
- Do not change renderer behavior beyond adding test registration or result-file smoke coverage.

## Current Baseline

- `config/scenes/bootstrap.scene.json` is the only current arena scene.
- `scene_probe` and `vulkan_scene_viewer` now support structured `--result-file` output.
- Existing CTest coverage includes movement behavior, control profile parsing, network protocol, snapshot relay, host CLI, and network E2E.
- Existing tests are useful but not all are fixture/golden characterization tests.
- Viewer smoke is manually runnable with `vulkan_scene_viewer --frames N`.

## Data Flow

```text
bootstrap.scene.json
  -> scene_runtime load + bounds refresh
  -> semantic fixture comparison
  -> characterization CTest failure on drift

fixed movement inputs
  -> combat movement/interpolation functions
  -> expected transforms/state traces
  -> characterization CTest failure on drift

snapshot/connect/disconnect packets
  -> bitpacked protocol encoder/decoder + relay ingest
  -> expected packet bytes/events/cache behavior
  -> characterization CTest failure on drift

vulkan_scene_viewer --frames N --result-file
  -> host CLI + Vulkan startup/render/shutdown
  -> JSON result artifact
  -> CTest smoke failure on launch/render regression
```

Ownership boundaries:

- Characterization tests may depend on existing modules but must not drive new architecture.
- Golden fixtures describe observed behavior; changing them requires an intentional review.
- Viewer smoke remains process-level coverage, not gameplay correctness.

## Implementation Plan

1. Add checked-in characterization fixtures for the current bootstrap scene and protocol bytes.
2. Add a characterization test executable that compares scene load/bounds/transforms against fixtures.
3. Add movement trace and interpolation assertions that pin current combat helper semantics.
4. Add exact packet byte checks for representative connect, disconnect, and actor snapshot messages.
5. Add relay behavior assertions for connect fanout, cached late join state, and disconnect removal.
6. Register characterization tests with CTest labels and timeouts.
7. Add a CTest viewer smoke using `vulkan_scene_viewer --frames 3 --result-file`.
8. Verify full CTest and targeted labels.

## Test Plan

- Unit:
  - `characterization_tests`
  - `movement_behavior_tests`
  - `network_protocol_tests`
  - `snapshot_relay_tests`
- Process/CTest:
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug`
  - viewer smoke test through CTest
- Manual/process:
  - Inspect generated viewer result JSON for smoke status if the CTest smoke fails.

## Acceptance Criteria

- Current bootstrap scene semantics are checked against a fixture.
- Movement trace and interpolation behavior are pinned by tests.
- Packet encoding has exact byte/hex characterization.
- Relay cache and disconnect behavior are pinned by tests.
- Viewer `--frames 3 --result-file` smoke runs under CTest.
- Existing network E2E still passes.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Exact model vertex/triangle counts may change if Assimp import flags change; that is intentional characterization and should require review.
- Viewer smoke opens a real window until hidden-window support exists.
- Avoid overfitting to machine-specific Vulkan validation warning text.
- Keep fixtures semantic and small; do not commit large generated artifacts.

## Progress Log

- 2026-06-20: Started after SP1-001 and SP1-002 were merged.
- 2026-06-20: Added fixture-backed characterization coverage for bootstrap scene semantics, movement/interpolation traces, protocol packet bytes, relay late-join/disconnect behavior, and viewer smoke under CTest.

## Verification Results

- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-unit` passed: 6/6 tests.
- `ctest --preset msvc-debug-characterization` passed: 2/2 tests, including `characterization_tests` and `viewer_smoke`.
- `ctest --preset msvc-debug-viewer` passed: 1/1 test.
- `ctest --preset msvc-debug` passed: 8/8 tests, including `network_e2e` and `viewer_smoke`.
- `build/msvc-debug/test-artifacts/viewer_smoke_result.json` reported `status: ok`, `host: vulkan_scene_viewer`, and `frames: 3`.
- No MSVC runtime/assertion windows were visible after verification.

## Final Commits

- `71ab398` Add SP1-003 characterization coverage
