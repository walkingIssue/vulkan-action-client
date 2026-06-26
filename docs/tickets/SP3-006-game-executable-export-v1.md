# SP3-006: Game Executable Export v1

Status: planned
Branch: sprint03/sp3-006-game-executable-export-v1
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Dependencies:
- Prefer after `SP3-003`.

## Goal

Add a v1 path for the editor to produce a runnable game executable/package from the current scene/config.

## Non-Goals

- Do not embed a compiler inside the editor.
- Do not build production packaging, installer, signing, patching, or asset cooking.
- Do not add networked/authoritative gameplay.
- Do not require ignored local assets.

## Export Contract

For v1, exporting produces:

- an output directory chosen by the user or test
- a runnable game executable target or copied executable
- a manifest naming scene/config/content inputs
- result JSON with executable path, manifest path, source scene, status, and diagnostics

The editor may invoke a repository build/export command. The export path must also be testable headlessly.

## Implementation Plan

1. Define a minimal runtime/game executable target if one does not already exist.
2. Define an export manifest schema.
3. Add a headless export command for deterministic testing.
4. Add editor UI controls: output directory input, build/export button, status panel, and open/reveal affordance when practical.
5. Add a launch smoke that runs the exported executable with the selected scene/config and writes result JSON.

## Acceptance Criteria

- A user can produce a runnable game executable/package from editor-selected content.
- The export command has guarded inputs and clear status.
- The exported executable launches in a smoke mode and reports success.
- Output artifacts are written outside tracked source paths unless explicitly configured.

## Test Plan

- Build the game runtime/export target.
- Headless export smoke to a temp build artifact directory.
- Launch exported executable with `--frames`/`--result-file` or equivalent.
- Visible/capturable editor screenshot of export controls.
- MSVC runtime/assertion dialog check.
