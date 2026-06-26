# SP3-005: Editor Input Controls and Guarded Datapoint Editing

Status: planned
Branch: sprint03/sp3-005-editor-input-controls-guarded-datapoints
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Dependencies:
- `SP3-001` for labels/guards.

## Goal

Expose implemented input-backed gameplay/editor datapoints through labeled controls with guards, validation feedback, dirty state, save/reload behavior, and tests.

## Non-Goals

- Do not implement every future combat/effect system.
- Do not make UI/viewer code own combat truth.
- Do not silently rewrite JSON without validation and explicit save action.
- Do not touch ignored local `assets/`.

## Candidate Datapoints

Initial Sprint 03 candidates:

- move duration, phase ranges, hitbox ranges, damage, hitstop, stun, reaction move
- proxy animation preview tick and selected socket display
- scenario ticks/seed when exposed as run controls
- character health/default move selection
- visual-lab playback seek/duration controls
- command-script play ticks when run from editor

## Guard Requirements

- Ranges must come from schema/compiler/runtime validation where possible.
- Range begin must not exceed range end.
- Ticks must stay nonnegative and bounded by the relevant duration.
- Damage/health/hitstop/stun must stay in documented unsigned ranges.
- Reaction move selectors must use known move IDs.
- Save must be explicit and must preserve valid JSON formatting.

## Acceptance Criteria

- Each edited datapoint has a visible label and guard.
- Invalid edits are blocked or clearly marked before save/run.
- Save/reload round trip is covered for at least one representative content file.
- Result diagnostics report edited fields and validation status.
- Visual QA shows the controls and validation feedback.

## Test Plan

- Focused content/editor tests for validation and save/reload.
- `game_editor_headless_smoke` or a new guarded-edit smoke using temp files.
- Visible/capturable editor screenshot before/after an edit.
- Full affected CTest when content schema behavior changes.
- MSVC runtime/assertion dialog check.
