# SP3-003: Editor Current Scene Render Viewport

Status: planned
Branch: sprint03/sp3-003-editor-current-scene-render-viewport
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Dependencies:
- Prefer after `SP3-002`.

## Goal

Render the current loaded scene/visual-lab state inside `game_editor` through shared renderer infrastructure.

## Non-Goals

- Do not build object picking, gizmos, hierarchy editing, or save semantics.
- Do not replace the 2D map preview; keep it as an inspection aid.
- Do not duplicate viewer rendering code into `game_editor`.
- Do not require ignored local assets.

## Implementation Plan

1. Add a renderer-backed editor viewport panel with a clear visible label.
2. Feed it from the editor's current loaded map/visual-lab state.
3. Add a `Render Current Scene` or equivalent button if rendering is not automatic.
4. Expose camera or preview inputs with guarded controls when user-adjustable.
5. Emit structured result diagnostics proving the editor rendered a current-scene path.
6. Capture visual QA evidence of the viewport.

## Acceptance Criteria

- `game_editor` can display the current scene in a renderer-backed viewport.
- Current scene source is clear to the user.
- Existing `game_editor_headless_smoke` remains stable or gets equivalent diagnostics.
- Visual QA evidence shows a nonblank current-scene viewport.

## Test Plan

- Build `game_editor`.
- `game_editor_headless_smoke`.
- Visible/capturable editor launch and screenshot per `.agents/skills/visual-qa/SKILL.md`.
- Affected viewer/editor CTest.
- MSVC runtime/assertion dialog check.
