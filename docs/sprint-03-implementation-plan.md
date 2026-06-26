# Sprint 03 Implementation Plan: Usable Editor, UI Coverage, and Viewer Extraction

Source design: `docs/action_combat_engine_editor_design.md`
Source status:
- `docs/game-editor-bootstrap-status.md`
- `docs/main-project-dependency-graph.md`
- `docs/sprint-02-implementation-plan.md`
- `docs/tickets/SP2-004-scenario-playback-controls_result.md`

Status: active planning baseline. Sprint 02 is interrupted after SP2-004; `SP2-005` is deferred.

Sprint 03 restarts around usability. The current backend can load maps, moves, proxy animation, characters, scenarios, and visual-lab evidence, but the editor UI mostly displays data. The new sprint makes features discoverable and operable through the editor, extracts the oversized viewer into reusable rendering/viewer modules, and starts the path toward producing a runnable game executable from editor-selected content.

## Product Outcome

A user should be able to:

1. Open the editor and find a labeled button, slider, input, picker, or menu for each implemented feature that depends on user-controlled input data.
2. See valid ranges and guards for numeric/text inputs before running a feature.
3. Render the current scene inside the editor instead of relying only on a separate viewer.
4. Point the editor at an input directory and discover renderable assets.
5. Export or build a runnable "game" executable/package from the current scene/config.
6. Keep `vulkan_scene_viewer` behavior intact while its 2.5k-line implementation is split into maintainable modules.

## Sprint Reset Decision

No further Sprint 02 work starts unless Mia/user explicitly reopens it. `SP2-005` remains useful historical planning, but editor usability and renderer/editor architecture now take priority over network compatibility.

All workers must fetch current `main` and wait for Sprint 03 dispatch. Do not self-assign from this plan.

## UI Coverage Rule

Every implemented feature must have a UI affordance when it depends on input data.

Required affordance types:

- buttons for commands such as reload, run, build, export, open, save, and rescan
- text/path inputs or file pickers for file and directory inputs
- sliders or numeric inputs for bounded numeric values
- checkboxes/toggles for binary options
- combo boxes or menus for enum/option sets
- tables/lists for selectable assets, scenarios, moves, characters, and generated artifacts

Every input control must have:

- a precise visible label matching the domain term
- a guard or allowed range derived from schema/runtime validation
- disabled/error state when the current value is invalid
- clear feedback when a command succeeds or fails
- structured result evidence for tests

No feature is considered user-ready if its only interface is a hard-coded default, command-line flag, or source JSON edit.

## Feature UI Inventory

Initial required coverage:

| Feature/Data | Current state | Sprint 03 UI requirement |
| --- | --- | --- |
| Map file | loaded from path/default | labeled scene/map path input plus reload/open control |
| Command script | loaded from path/default | command-script path input, run button, status/result panel |
| Scenario | loaded from path/default | scenario path input, run button, golden status display |
| Move data | displayed read-only | move picker, editable guarded timing/damage/hitstop/reaction controls in a later ticket |
| Proxy animation | preview tick slider | labeled tick slider with bounds and socket/root preview |
| Character defaults | displayed read-only | character selector and guarded health/default move controls in a later ticket |
| Visual lab playback | play/step/reset/seek | keep controls, label/guard seek range, connect to current scene |
| Map authoring/runtime compile | reload/build buttons | explicit build/compile controls and diagnostics |
| Asset loading | no browser | input directory picker, rescan button, renderable asset list |
| Viewer render | separate executable | shared renderer path usable by editor viewport |
| Game executable | no export path | export/build button, output directory input, generated manifest/result JSON |

## Viewer Split Strategy

`src/vulkan_scene_viewer.cpp` is too large because it combines:

- CLI/result handling
- camera and controls
- scene/visual-lab/network state preparation
- Vulkan instance/device/swapchain/render-pass/framebuffer setup
- buffers/images/model upload
- render loop and synchronization
- scene/debug/visual-lab drawing
- process result diagnostics

Split by ownership, not by line count. The first extraction must be behavior-preserving.

Target shape:

| Module | Ownership |
| --- | --- |
| `viewer/viewer_options.*` | viewer CLI options and result diagnostics |
| `viewer/viewer_camera.*` | camera view, orbit/follow controls, bounds anchoring |
| `viewer/viewer_scene_state.*` | map/visual-lab/scenario/network presentation state |
| `viewer/vulkan_context.*` | Vulkan instance, device, queues, swapchain, render pass, framebuffers |
| `viewer/vulkan_resources.*` | buffers, images, uploads, model buffers |
| `viewer/scene_renderer.*` | draw-data upload and scene/debug/visual-lab rendering entry points |
| `src/vulkan_scene_viewer.cpp` | thin executable composition and `main` |

Do not use the split ticket to redesign rendering, add editor features, or alter smoke diagnostics. Once extracted, the editor can consume shared renderer pieces instead of cloning viewer internals.

## Editor/Game Requirements

### Render Current Scene

The editor must render the current loaded scene/document through a real renderer-backed viewport. The current 2D map preview remains useful, but it is not enough for "current scene" rendering.

### Explore Renderable Assets

The editor must accept an input directory, scan for renderable assets, and present a selectable list. Initial renderable extensions:

- `.glb`
- `.gltf`
- `.obj`
- `.fbx`

`.zip` files may be listed and classified by contained extensions, but direct import from archives is optional unless a ticket explicitly adds extraction/import handling.

### Produce The Game Executable

Sprint 03 should define and implement a testable v1 export path:

```text
current scene/config
  -> export manifest
  -> copied/selected content
  -> runnable game executable target or bundle
  -> launch smoke/result JSON
```

For v1, "produce executable" means the editor exposes a command that builds or packages a checked-in game runtime target and writes an export directory containing the executable plus manifest/config/content references. The editor should not embed a compiler; it may invoke the repository build/export command.

## Sprint Tickets

### SP3-001: Editor Feature UI Contract Matrix

Branch: `sprint03/sp3-001-editor-feature-ui-contract-matrix`

Goal: turn the UI coverage rule into an actionable inventory and helper contract so every input-backed feature has a required control, label, guard, result signal, and test expectation.

### SP3-002: Split Vulkan Scene Viewer Modules

Branch: `sprint03/sp3-002-split-vulkan-scene-viewer-modules`

Goal: split `src/vulkan_scene_viewer.cpp` into behavior-preserving modules so the editor can share renderer/viewer infrastructure without growing another monolith.

### SP3-003: Editor Current Scene Render Viewport

Branch: `sprint03/sp3-003-editor-current-scene-render-viewport`

Goal: render the current loaded scene/visual-lab state inside `game_editor` using shared rendering infrastructure.

### SP3-004: Editor Renderable Asset Browser

Branch: `sprint03/sp3-004-editor-renderable-asset-browser`

Goal: let the editor scan a user-selected input directory for renderable assets and present a usable, guarded asset list.

### SP3-005: Editor Input Controls and Guarded Datapoint Editing

Branch: `sprint03/sp3-005-editor-input-controls-guarded-datapoints`

Goal: expose implemented input-backed gameplay/editor datapoints through labeled controls with valid ranges, validation, dirty state, save/reload behavior, and result/test evidence.

### SP3-006: Game Executable Export v1

Branch: `sprint03/sp3-006-game-executable-export-v1`

Goal: add a v1 editor/build pipeline that produces a runnable game executable/package from the current scene/config and verifies it with a launch smoke.

## Recommended Order

```text
SP3-001 UI contract matrix
SP3-002 viewer split
SP3-003 editor current-scene rendering
SP3-004 renderable asset browser
SP3-005 guarded datapoint controls
SP3-006 game executable export
```

`SP3-004` can run in parallel with late `SP3-002` work if it only touches editor data scanning/UI and not shared renderer modules. `SP3-005` should wait until `SP3-001` defines labels/ranges and until its edited surfaces are clear. `SP3-006` should wait until the editor scene/config ownership path is stable enough to export.

## Verification Gates

All Sprint 03 lanes must follow `docs/agent-ticket-workflow-v2.md`.

Minimum gates:

- focused unit/CTest coverage for changed code
- `cmake --build --preset msvc-debug` or an explicitly isolated equivalent
- affected process smoke test
- `game_editor_headless_smoke` for editor data-path changes
- visible/capturable visual QA per `.agents/skills/visual-qa/SKILL.md` for editor/viewer/rendering/UI changes
- MSVC runtime/assertion dialog check
- no staged/modified ignored `assets/` or user-owned local files

## Non-Goals

- Reopening `SP2-005` network compatibility without explicit dispatch.
- Full production asset cooking/import.
- Full skeletal animation, material graph, particle/effect editor, or animation graph editor.
- Rolling authoritative networked gameplay into the editor export path.
- Rewriting combat truth into UI/viewer code.
- Making screenshots the gameplay correctness oracle.
