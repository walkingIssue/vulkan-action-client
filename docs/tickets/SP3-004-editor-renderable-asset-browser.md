# SP3-004: Editor Renderable Asset Browser

Status: planned
Branch: sprint03/sp3-004-editor-renderable-asset-browser
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`

## Goal

Let the editor scan a user-selected input directory for renderable assets and present a selectable list with clear labels, status, and guards.

## Non-Goals

- Do not commit or modify ignored local `assets/`.
- Do not require direct import from `.zip` archives unless explicitly added.
- Do not build a full asset registry/cook pipeline.
- Do not implement material/animation editing.

## Renderable Extensions

Initial file allowlist:

- `.glb`
- `.gltf`
- `.obj`
- `.fbx`

`.zip` files may be detected and reported separately when they contain renderable filenames, but loading from archives can remain a future ticket.

## Implementation Plan

1. Add an input-directory control with a correct label and existence/readability guard.
2. Add a `Rescan Assets` button.
3. Recursively scan the directory for renderable files.
4. Show extension, relative path, size, status, and diagnostics in a selectable table/list.
5. Allow selecting an asset for preview/load if the current renderer/import path supports it.
6. Emit result diagnostics for scanned count, renderable count, selected asset, and rejected files.

## Acceptance Criteria

- The editor can scan a directory chosen by the user.
- Renderable candidates are visible with clear status.
- Invalid/missing directories are guarded and reported.
- No local asset payloads are staged or modified.

## Test Plan

- Unit test scan/filter logic with temporary fixture directories.
- `game_editor_headless_smoke` or a new headless asset-scan smoke.
- Visible/capturable editor screenshot of the asset browser.
- MSVC runtime/assertion dialog check.
