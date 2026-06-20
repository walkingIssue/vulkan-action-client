# Sprint 01 Risk Mitigation Plan

Source results: `docs/tickets/SP1-001-*_result.md` through `docs/tickets/SP1-012-*_result.md`

Sprint 01 is implemented and merged. This minisprint reduces the risks that would slow down the next real product sprint: local-only asset dependencies, PowerShell-only process orchestration, weak cross-asset validation, and the temporary visual lab's direct asset wiring.

## Mitigation Outcome

After this minisprint, the project should be easier for agents and humans to run from a clean checkout, safer to extend with new combat data, and clearer about which temporary Sprint 01 systems are allowed to persist into the next sprint.

## Risk Intake

| Risk | Source Result | Mitigation Decision |
|---|---|---|
| Full debug CTest depends on ignored local extracted paladin assets. | `SP1-011`, `SP1-012`, earlier viewer/characterization results | Mitigate now by moving automated viewer/characterization tests to small tracked fixture assets while leaving the full paladin scene optional/manual. |
| `network_e2e` still shells through PowerShell. | `SP1-001` | Mitigate now with a compiled process runner or CTest-native process path so the canonical suite is not PowerShell-bound. |
| `--hidden-window` is parsed but not implemented by the Vulkan viewer. | `SP1-002`, `SP1-003` | Mitigate if practical by making smoke-friendly hidden/offscreen window behavior explicit; otherwise document the Vulkan-platform limitation and keep the visual lab smoke fast. |
| Move assets reference proxy sockets by string with no cross-asset validation. | `SP1-009` | Mitigate now with a content validation pass that checks moves, proxy animations, and scenarios together. |
| Scenario runner orchestrates hit/cancel signals around combat runtime rather than a fully integrated damage/reaction pipeline. | `SP1-010` | Defer full damage/reaction to the next combat sprint; add validation and trace governance now so behavior remains controlled. |
| Golden trace governance will matter as combat language grows. | `SP1-010` | Mitigate now with a golden manifest/update policy and tests that make accidental golden drift harder. |
| Visual lab directly renders Sprint 1 assets, not scenario playback controls. | `SP1-011` | Mitigate now by allowing the lab to load a scenario fixture for diagnostics/overlay source data. Full editor playback controls remain deferred. |
| Temporary spawn-point actor creation and scenario-authored placements are not final character definitions. | `SP1-005`, `SP1-010` | Defer final character definitions; keep scenario placement explicit and validated until the next design sprint defines character asset ownership. |
| Axis-aligned/approximate primitive collision and temporary proxy animation are not final animation/physics. | `SP1-008`, `SP1-009`, `SP1-011` | Defer full oriented volumes, Jolt-backed geometry, skeleton clips, sockets, and GPU skinning. Keep tests around current deterministic approximations. |
| Branch/worktree coordination produced duplicate visual-lab helper files in a sibling worktree. | `SP1-011` handoff notes | Mitigate through workflow/doc updates after the implementation minisprint, not engine code. |

## Tickets

### RM1-001: Asset-Independent Test Fixtures

Branch: `mitigation01/rm1-001-asset-independent-tests`

Goal: make `ctest --preset msvc-debug` pass from a clean checkout without ignored `assets/extracted/*` files by using tracked miniature scene assets for automated characterization and viewer smoke tests.

Mitigates:
- Full CTest dependence on ignored paladin assets.
- Confusion between optional visual/manual assets and required regression fixtures.

Out of scope:
- Removing or replacing the real paladin bootstrap scene for manual viewer/client work.
- Building a full asset registry or cook pipeline.

### RM1-002: Compiled Network E2E Runner

Branch: `mitigation01/rm1-002-compiled-network-e2e`

Goal: replace the PowerShell-backed `network_e2e` CTest with a compiled runner or CTest-native process orchestration so the canonical test suite is not tied to `.ps1` execution.

Mitigates:
- PowerShell-only behavior in the canonical test path.
- Hard-to-observe process failures when Windows script orchestration misbehaves.

Out of scope:
- Authoritative combat networking, prediction, reconciliation, or production matchmaking.

### RM1-003: Cross-Asset Content Validation

Branch: `mitigation01/rm1-003-cross-asset-validation`

Goal: add a validation path that checks maps, moves, proxy animations, and scenarios together, especially socket references and fixture paths.

Mitigates:
- Stringly typed move-to-animation socket references.
- Scenario fixtures drifting from available content assets.
- Golden traces being updated against invalid or incomplete content graphs.

Out of scope:
- A full asset database, package IDs, dependency graph UI, or cooker.

### RM1-004: Scenario-Driven Visual Lab Diagnostics

Branch: `mitigation01/rm1-004-scenario-visual-lab`

Goal: let `vulkan_scene_viewer --visual-lab` accept a scenario fixture as the lab data source and report scenario/golden diagnostics in the result file.

Mitigates:
- Visual lab showing direct Sprint 1 assets without proving alignment to scenario runner fixtures.
- Harder handoff between headless golden traces and visual inspection.

Out of scope:
- Full timeline controls, ImGui editor panels, undo/redo, or live asset editing.

## Deferred Risks

These are real, but should be handled by a later sprint with a broader design slice:

- Final character asset definitions and authored spawn rules.
- Stable network compatibility hash/version contracts.
- Full oriented collision, richer primitive geometry, and Jolt-backed runtime collision.
- Skeletal animation import, socket hierarchy, and GPU skinning.
- Final editor/debug-render architecture.
- Combat damage, hit reactions, resources, equipment, and rollback/prediction.

## Verification Gates

Each ticket follows `docs/agent-ticket-workflow.md`: ticket start doc on `main`, feature branch, implementation, focused tests, full debug CTest, result doc, no-gpg merge, and push.

The minisprint is complete when:

- `cmake --preset msvc-debug` passes.
- `cmake --build --preset msvc-debug` passes.
- `ctest --preset msvc-debug` passes without requiring ignored local asset extraction.
- The canonical CTest suite no longer depends on PowerShell for network E2E.
- Cross-asset validation catches at least one missing socket/path style error.
- Visual lab diagnostics can be driven from a checked-in scenario fixture.
