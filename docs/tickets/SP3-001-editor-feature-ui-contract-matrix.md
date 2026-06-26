# SP3-001: Editor Feature UI Contract Matrix

Status: planned
Branch: sprint03/sp3-001-editor-feature-ui-contract-matrix
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Source docs:
- `docs/game-editor-bootstrap-status.md`
- `docs/main-project-dependency-graph.md`
- `docs/action_combat_engine_editor_design.md`

## Goal

Create the authoritative Sprint 03 UI contract for implemented features: every input-backed feature must map to a visible control, label, guard, result signal, and verification expectation.

## Non-Goals

- Do not implement the full editor inspector in this ticket.
- Do not split `vulkan_scene_viewer.cpp`.
- Do not add the asset browser or game export pipeline.
- Do not change combat/runtime semantics.

## Implementation Plan

1. Add a tracked feature-to-UI matrix covering current map, command script, scenario, move, proxy animation, character, visual lab, asset loading, viewer rendering, and export surfaces.
2. Define standard UI control metadata: label, type, guard/range source, disabled/error state, result diagnostic key, and visual-QA expectation.
3. Add small editor-side helper types only if needed to prevent later tickets from inventing incompatible label/range conventions.
4. Update `docs/game-editor-bootstrap-status.md` to point at the matrix.
5. Add focused tests if helper code is introduced.

## Acceptance Criteria

- The matrix names each previously implemented input-backed feature and its required UI control.
- Numeric/text/path controls have documented allowed-value guards.
- Later tickets can cite the matrix instead of redefining labels/ranges.
- No user-owned `assets/` are touched.

## Test Plan

- Documentation-only: `git diff --check`.
- If helper code is added: focused unit tests and `game_editor_headless_smoke`.
