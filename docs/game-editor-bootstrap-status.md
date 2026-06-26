# Game Editor Bootstrap Status

Updated: 2026-06-26

The provisional editor shell landed in `045551a Add game editor bootstrap and visual QA skill`.

Sprint 03 restarts active editor work around UI coverage, current-scene rendering, renderable asset discovery, viewer extraction, animated armature/weapon preview, effects/shader contracts, and game executable export. See `docs/sprint-03-implementation-plan.md` and `docs/asset-authoring-contracts.md`.

The authoritative Sprint 03 feature-to-UI contract matrix is
`docs/sprint-03-editor-ui-contract-matrix.md`. Later editor tickets should cite
that matrix for labels, control types, guards/ranges, disabled/error states,
result diagnostic keys, and visual-QA expectations.

## What Exists

- `game_editor` is a separate CMake executable backed by GLFW, Vulkan, and ImGui.
- `.agents/skills/visual-qa/SKILL.md` defines the required screenshot-based workflow for editor/viewer/visual changes.
- `game_editor_headless_smoke` exercises the editor data path without opening the UI.
- The visible UI currently exposes panels for overview, map preview, map authoring, command-script results, move timeline, proxy animation, character defaults, scenario runner, visual lab, and log output.
- The editor can load current map, command-script, move, proxy-animation, character, and scenario fixtures, run scenario/command-script paths, and build visual-lab previews from scenario or direct assets.

Default inputs include:

- `content/maps/basic_primitive_arena.map.json`
- `tests/fixtures/commands/wire_arena.commands.json`
- `content/moves/light_attack.move.json`
- `content/animations/sword_light_proxy.anim.json`
- `content/characters/basic_duelists.character.json`
- `tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json`

## Current UI Capability

The UI is an inspection and preview shell, not a datapoint authoring tool yet.

Current interactive controls:

- reload or rerun loaded inputs
- run the scenario
- run the map command script
- build visual-lab data from scenario or direct assets
- scrub proxy-animation preview tick
- play, pause, step, reset, and seek visual-lab playback

Current read-only displays:

- map objects, spawn points, materials, runtime entities, and colliders
- command-script result counts and saved path
- move phases, hitboxes, damage, hitstop, reactions, and cancels
- proxy root/socket sampled positions
- character default health and moves
- scenario trace events and golden status
- visual-lab summary and scenario evidence

## Datapoint Editing Status

You cannot currently alter gameplay/effect datapoints in the UI and persist those changes.

The code uses buttons and preview sliders, but it does not expose `Input*`, `Drag*`, `ColorEdit*`, editable tables, inspector fields, undo/redo commands, save/export buttons, or JSON writeback for move/effect/map/character/scenario datapoints.

The controls that do mutate state are in-memory preview controls:

- proxy animation `Preview Tick`
- visual-lab `Play`, `Pause`, `Step`, `Reset`, and `Seek`
- reload/rerun/build commands that reprocess existing files

To adjust effects right now, edit the source JSON/content files directly and reload or rerun the editor path. The likely next implementation step is an inspector/timeline editing lane that adds editable controls, validation, dirty state, save/export, and regression coverage.

## Verification Expectations

For editor or visual changes:

1. Build `game_editor`.
2. Run `game_editor_headless_smoke` or the affected process smoke test.
3. Launch a visible or capturable editor run.
4. Capture and inspect at least one screenshot of the relevant ImGui panel.
5. Check the relevant labels, guards, diagnostics, and expected visible state
   against `docs/sprint-03-editor-ui-contract-matrix.md`.
6. Report any blocked visual capture separately from build/test evidence.
