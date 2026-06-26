# Sprint 03 Editor UI Contract Matrix

Updated: 2026-06-26
Owner: SP3-001 Editor Feature UI Contract Matrix
Status: authoritative Sprint 03 contract

This document is the UI contract later Sprint 03 editor lanes must cite instead
of redefining labels, guards, result keys, and visual-QA expectations. It covers
currently implemented input-backed features plus the Sprint 03 surfaces planned
for current-scene rendering, asset browsing, guarded editing, export, armature
preview, and visual effects authoring.

The current `game_editor` is an inspection and preview shell. Rows marked
`live` already exist in the editor at the SP3-001 base. Rows marked `required`
are planned Sprint 03 controls and diagnostics that later tickets must implement
or explicitly supersede through Mia.

## Control Metadata Contract

Every input-backed editor feature must declare these fields in its code, test, or
ticket result when implemented:

| Field | Contract |
| --- | --- |
| Feature ID | Stable identifier in the `editor.<surface>.<feature>` form. |
| Visible label | User-facing label that appears in the UI and test notes. |
| Control type | One of `path-input`, `file-picker`, `directory-picker`, `button`, `numeric-input`, `slider`, `combo`, `toggle`, `table`, `result-panel`, `diagnostic-list`, or `viewport`. |
| Source of truth | Loader, validator, runtime subsystem, or build/export command that owns semantics. |
| Guard/range source | Exact validation function or documented numeric range. |
| Disabled state | Condition that prevents the control from running or saving. |
| Error state | Inline message, diagnostics panel entry, or result diagnostic emitted when invalid. |
| Result diagnostics | Stable `key=value` entries or JSON fields proving the action happened. |
| Visual QA | Screenshot state that proves the user-visible result for UI/rendering work. |

## Universal Rules

- Labels must use the domain term from this matrix. Later tickets may add helper
  text, but the primary label remains stable unless Mia approves a contract
  change.
- Path controls must show the current path, allow editing or picking, and reject
  a run/build/save action when the path is empty or invalid for that feature.
- Numeric controls must clamp, block, or visibly mark invalid values before the
  command executes. Do not silently coerce persisted JSON without feedback.
- Result diagnostics should be structured enough for headless tests. Use
  existing `key=value` entries when a key already exists; use the reserved keys
  below when adding new surfaces.
- Visual effects, materials, shaders, models, armatures, weapons, and previews
  are presentation/editor assets. Combat damage, hit validity, invulnerability,
  target filters, AOE truth, and cancel semantics remain owned by combat/move
  data.
- Screenshot QA supplements headless/process evidence. It is required for actual
  UI, viewport, viewer, Vulkan, ImGui layout, map preview, model loading, debug
  draw, or rendering changes, and is not required for this documentation-only
  ticket.

## Feature-To-UI Matrix

| Feature ID | State | Required visible controls and labels | Guard/range source | Disabled/error state | Result diagnostics | Visual-QA expectation |
| --- | --- | --- | --- | --- | --- | --- |
| `editor.project.refresh` | live | `Refresh All` button in `Editor Overview`; result/status panel. | All loaded fixture paths and validators below. | Keep enabled; errors appear in per-panel diagnostics and log. | `editor=true`, all loaded/status keys below. | Overview shows project path, current inputs, and refreshed status bullets. |
| `editor.map.path` | live, path editing required | `Scene Map` path input, `Open Map` file picker, `Reload Map` button, `Map Preview` viewport. Current live UI shows path text plus `Reload Map`. | `content::loadAuthoringScene`; `content::validateAuthoringScene`. JSON schema version 1; existing `.map.json` file. | Disable reload/build when path is empty, missing, unparsable, or invalid; show `ContentDiagnostic` lines. | `mapLoaded`, `mapObjectCount`, `mapSpawnPointCount`, `mapRuntimeEntityCount`, `mapRuntimeColliderCount`. | Map panel and map preview show world bounds, objects, spawn points, and diagnostics if invalid. |
| `editor.map.compile` | live | `Build Direct Visual Lab` button; later `Build Runtime World` button if exposed separately. | Map must load and compile through `content::compileRuntimeWorld`. | Disable while map invalid; show compile diagnostics. | `visualLabBuilt`, `visualLabPrimitiveCount`, `visualLabDebugLineVertexCount`; map command process may also emit `entityCount`, `colliderCount`, `spawnPointCount`. | Visual Lab panel shows generated primitive/debug-line counts from the current map. |
| `editor.command_script.run` | live, path editing required | `Command Script` path input, `Open Command Script` picker, `Run Command Script` button, result panel. Current live UI shows path text plus run buttons. | `authoring::runMapCommandScript`; command document must parse, commands must be valid, save/reopen paths must be resolvable. | Disable run when path is empty/missing/unparsable; show `MapCommandDiagnostic` entries. | Editor: `commandStatus`, `commandCount`, `commandTicksPlayed`. Process JSON: `commandCount`, `createdObjectIds`, `diagnostics`, `entityCount`, `colliderCount`, `spawnPointCount`, `ticks`, `stateHash`, optional `savedPath`. | Command panel shows status, message, counts, created IDs, saved path, ticks, state hash, and diagnostics. |
| `editor.move.selection` | live, guarded editing required | `Move` path input or picker, `Reload Move` button, `Move Timeline` table. Current live UI shows path text plus `Reload Move`. | `content::loadMoveAsset`; `content::validateMoveAsset`; `content::compileMoveAsset`. | Disable reload/save when path is invalid; show move validation diagnostics. | `moveLoaded`, `movePhaseCount`, `moveHitboxCount`, `moveCancelCount`. | Move panel shows logical ID, duration, command, phases, hitboxes, cancels, and diagnostics. |
| `editor.move.datapoints` | required | `Move Duration Ticks`, `Input Command`, `Input Buffer Ticks`, `Phase Begin Tick`, `Phase End Tick`, `Hitbox Begin Tick`, `Hitbox End Tick`, `Hitbox Shape`, `Hitbox Socket`, `Hitbox Size`, `Hitbox Offset`, `Damage`, `Hitstop Ticks`, `Stun Ticks`, `Reaction Move`, `Cancel Condition`, `Cancel Destination`, `Event Tick`, `Save Move`, `Reload Move`. | Move guard table below. Tick windows are half-open `[beginTick, endTick)` inside `durationTicks`; reaction/cancel move selectors use known move IDs. | Invalid fields mark the row and disable save/run/preview that consumes the move. | Reserve `moveEditValidationStatus`, `moveDirtyFields`, `moveSavedPath`, `moveSelectedLogicalId`, `moveSelectedHitboxId`. Existing load keys remain. | Screenshot shows at least one edited valid field and one invalid guarded field when SP3-005 implements editing. |
| `editor.proxy_animation.preview` | live | `Proxy Animation` path input or picker, `Reload Animation` button, `Preview Tick` slider, socket table/list. Current live UI shows path text, reload, and slider. | `animation::loadProxyAnimation`; `animation::validateProxyAnimation`; preview tick `0..min(durationTicks, 600)` in current UI. | Disable slider when animation invalid; show proxy validation diagnostics. | `proxyLoaded`, `proxySocketCount`. | Proxy panel shows root pose and socket world transforms changing when `Preview Tick` moves. |
| `editor.character.defaults` | live, guarded editing required | `Character File` path input or picker, `Reload Character File`, `Character` selector/tree, later `Default Health Current`, `Default Health Max`, `Default Move` selector, `Save Character File`. Current live UI shows path text, reload, character tree, health, default moves. | Character file validation in `combat_scenario` character loading; referenced moves must exist in scenario move set. | Disable save/scenario run when health/move references invalid; show scenario/character diagnostics. | `characterLoaded`, `characterCount`; scenario diagnostics include character errors such as `invalid_character_health` and `unknown_character_move`. | Character panel shows selectable character IDs, labels, health, default moves, and invalid field feedback when editing lands. |
| `editor.scenario.run` | live, path editing required | `Scenario` path input, `Open Scenario` picker, `Run Scenario`, `Build Scenario Lab`, trace event table, golden status display. Current live UI shows path text plus run/build buttons. | `combat::loadCombatScenario`; `combat::runCombatScenario`; schema version 1; scenario content graph and golden trace validation. | Disable run/build when scenario path is empty/missing/unparsable; show `ScenarioDiagnostic` entries and golden mismatch messages. | Editor: `scenarioStatus`, `scenarioGoldenCompared`, `scenarioGoldenMatched`, `scenarioTraceEventCount`, `scenarioFinalStateHash`. Process JSON: `scenarioId`, `golden`, `goldenCompared`, `goldenMatched`, `trace.finalStateHash`, `trace.ticksRun`, `trace.events`, `diagnostics`. | Scenario panel shows status, final hash, golden compare/match, trace rows, and diagnostics. |
| `editor.visual_lab.build` | live | `Build From Scenario`, `Build From Assets`, visual-lab summary/result panel. | `visual_lab::buildVisualLabSceneFromScenario` or `visual_lab::buildVisualLabScene`; source paths must pass map/move/proxy/scenario validators. | Show visual lab diagnostics; disable dependent playback when no lab exists. | `visualLabBuilt`, `visualLabPrimitiveCount`, `visualLabDebugLineVertexCount`, `visualLabPlaybackDuration`, plus `visualLabSource`, `scenarioId`, `scenarioStatus`, `scenarioTicksRun`, `scenarioGoldenPath`, `scenarioMessage` when scenario-backed. | Visual Lab panel shows primitives, colliders, hitboxes/hurtboxes, sockets, debug lines, and scenario evidence. |
| `editor.visual_lab.playback` | live | `Play`, `Pause`, `Step`, `Reset`, `Seek` slider. | `visual_lab::makePlaybackState`, `setPlaybackPaused`, `stepPlayback`, `resetPlayback`, `seekPlayback`; seek range `0..durationTicks`. | Disable or no-op playback controls when no lab is built; clamp seek to duration. | `visualLabPlayback`, `playbackCurrentTick`, `playbackDurationTicks`, `playbackPaused`, `playbackRunning`, `playbackCanStepForward`, `playbackPreviewStepTick`, `playbackPreviewResetTick`, `playbackPreviewSeekEndTick`. | Screenshot before/after seek or step shows the playback tick and visible panel state changing. |
| `editor.current_scene.viewport` | required | `Current Scene Viewport`, `Render Current Scene`, camera/orbit controls when available. | Shared renderer/viewer modules from SP3-002; scene state from current editor document, map, scenario, or visual lab. | Show fallback/error material or renderer diagnostics; do not block headless data validation on visual capture. | Reserve `editorViewportRendered`, `editorViewportSceneId`, `editorViewportFrameCount`, `editorViewportRenderStatus`, `editorViewportDiagnostics`. | Screenshot shows a nonblank renderer-backed viewport for the currently loaded scene, not only the 2D map preview. |
| `editor.asset_browser.renderables` | required | `Input Directory` directory picker, `Rescan Assets`, renderable asset table/list, selected asset preview metadata. | Directory must exist and be readable; initial renderable extensions are `.glb`, `.gltf`, `.obj`, `.fbx`; `.zip` may be listed/classified. | Disable rescan/selection actions for invalid directory; list unsupported/missing assets as diagnostics, not crashes. | Reserve `assetBrowserRoot`, `assetBrowserScanStatus`, `assetBrowserRenderableCount`, `assetBrowserZipCount`, `assetBrowserSelectedAsset`, `assetBrowserDiagnostics`. | Screenshot shows directory path, rescan result, and at least one classified renderable asset or a clear empty/error state. |
| `editor.armature_weapon.preview` | required | `Skeleton Asset`, `Clip`, `Preview Tick`, `Socket`, `Weapon Asset`, `Attach Weapon`, `Trail Base Socket`, `Trail Tip Socket`. | `docs/asset-authoring-contracts.md`; import manifest must record source hash, axis/unit conversion, node/mesh/material/bone/clip/socket counts. Preview tick `0..clipDurationTicks`. | Disable preview/attach when skeleton, clip, weapon, or socket references are missing; show import/socket diagnostics. | Reserve `armatureAssetId`, `skeletonBoneCount`, `clipCount`, `selectedClip`, `selectedClipDurationTicks`, `selectedTick`, `socketCount`, `weaponAssetId`, `weaponAttached`, `weaponBaseSocket`, `weaponTipSocket`. | Screenshot shows skeleton/clip metadata, selected tick pose, visible sockets, and attached weapon when SP3-007 lands. |
| `editor.effects.sword_swing_poc` | required | `Effect Asset`, `Preview Effect`, `Material Mode`, `Color RGBA`, `Duration Ticks`, `Spawn Tick`, `Spawn Socket`, `Spawn Offset`, `Particle Count`, `Particle Lifetime Ticks`, `Particle Size`, `Particle Speed`, `Spread Degrees`, `Gravity Scale`, `Drag`, `Trail Enabled`, `Trail Base Socket`, `Trail Tip Socket`, `Trail Width`, `Trail Lifetime Ticks`, `Trail Fade`. | Effect POC ranges below and `docs/asset-authoring-contracts.md`. Effects consume combat events but never define combat truth. | Disable preview/save on invalid range, missing material/shader/effect, or missing socket; show fallback/error material diagnostics separately. | Reserve `effectId`, `effectValidationStatus`, `effectMaterialMode`, `effectNodeCount`, `effectParticleCount`, `effectDurationTicks`, `effectSpawnSocket`, `effectSpawnOffset`, `effectTrailEnabled`, `effectTrailBaseSocket`, `effectTrailTipSocket`, `effectPreviewStatus`, `effectDiagnostics`. | Screenshot shows controls and visible preview state for `effect.sword_swing_poc`; invalid material/socket state must be visually obvious. |
| `editor.game_export.v1` | required | `Export Directory`, `Build Game Executable`, `Open Export`, export manifest/result panel. | Repository build/export command chosen by SP3-006; current scene/config must validate before export. | Disable build when output path invalid or scene/config invalid; show build/launch diagnostics. | Reserve `exportStatus`, `exportDirectory`, `exportManifestPath`, `exportExecutablePath`, `exportContentCount`, `exportLaunchStatus`, `exportDiagnostics`. | Screenshot shows export controls and result path/status; process smoke proves exported executable launches. |

## Guard And Range Reference

| Surface | Required guards |
| --- | --- |
| Map scene | `schemaVersion == 1`; `assetGuid` and `logicalId` non-empty; `unitsPerMeter` finite and `> 0`; `worldBounds` finite with `min < max`; object IDs non-empty and unique; parent IDs must exist; transforms finite and scale absolute value at least `0.0001`; primitive type one of `plane`, `box`, `sphere`, `capsule`, `cylinder`; primitive size positive; base color channels `0.0..1.0`; collider type one of `box`, `sphere`, `capsule`; collider size positive; collider mode/layer non-empty; spawn IDs non-empty and unique; spawn translation/yaw finite; spawn team non-empty; at least one enabled spawn point. |
| Map command script | Path must exist and parse as a command array/document; commands must be known; object references must exist before mutation; save/reopen commands require a resolved path; compile/play commands require a valid map compile; default play ticks are nonnegative unsigned ticks. |
| Move asset | `schemaVersion == 1`; `assetGuid`, `logicalId`, and `input.command` non-empty; `durationTicks > 0`; stamina cost finite and `>= 0`; track IDs non-empty and unique; every tick range uses `beginTick < endTick <= durationTicks`; event tick `< durationTicks`; tags must be declared; movement translation finite; hitbox shape one of `box`, `sphere`, `capsule`; hitbox socket non-empty; hitbox size positive and offset finite; reaction move and cancel destinations must be known move IDs; cancel destinations non-empty; same-move cancel at tick zero is invalid. |
| Proxy animation | `schemaVersion == 1`; `assetGuid` and `logicalId` non-empty; `durationTicks > 0`; root track non-empty; root and socket keyframes finite, strictly increasing by tick, and inside duration; socket names non-empty and unique; required sprint sockets `weapon_base`, `weapon_tip`, and `chest` exist. Current preview slider range is `0..min(durationTicks, 600)`. |
| Character defaults | Character file `schemaVersion == 1`; at least one character; character ID non-empty and unique; default health has `max > 0` and `current <= max`; default hurtbox has valid shape, positive size, and finite offset; at least one default move; default move IDs are non-empty and known to the scenario move set. |
| Scenario | `schemaVersion == 1`; `logicalId` non-empty; `durationTicks > 0`; map, moves, actors, and golden path present; actor IDs nonzero and unique; actor has spawn or explicit translation; actor team required when no spawn supplies ownership; combat bridge health has `maxHealth > 0` and `health <= maxHealth`; actor hurtbox positive and finite; input tick `< durationTicks`; input actor exists; input command non-empty; referenced map, characters, moves, proxy animations, and golden files exist unless a guarded golden update is explicitly requested; hitbox moves need proxy animation bindings; hitbox sockets must exist in the bound proxy animation. |
| Visual lab playback | Playback tick range is `0..durationTicks`; step cannot move beyond duration; reset returns tick `0`; play/pause toggles running/paused only after a lab exists. |
| Renderable asset browser | Input directory must exist and be readable; initial renderable extensions are `.glb`, `.gltf`, `.obj`, `.fbx`; `.zip` may be classified by contained extensions but direct archive import is optional unless dispatched. |
| Armature/weapon preview | Import metadata must record source path/hash, importer settings, axis/unit conversion, node count, mesh/submesh count, material slots, vertex/index count, bounds, bone count/root, clip names/durations/sample rates, socket/marker names, and diagnostics. Coordinate contract is `RH_Y_UP_NEG_Z_FORWARD` with `unitsPerMeter = 1.0`. Stable socket names include `hand_r`, `hand_l`, `weapon_base`, and `weapon_tip`. |
| Effect authoring POC | `durationTicks: 1..600`; `spawnTick: 0..durationTicks`; `particleCount: 0..512`; `particleLifetimeTicks: 1..240`; `particleSize: 0.001..10.0`; `particleSpeed: 0.0..50.0`; `spreadDegrees: 0.0..180.0`; `gravityScale: -10.0..10.0`; `drag: 0.0..20.0`; `trailWidth: 0.001..2.0`; `color RGBA channels: 0.0..1.0`; material mode initially `unlit` or `additive`. |
| Game export | Current scene/config must validate; export directory must be explicit; generated manifest and content references must be written before launch smoke; editor invokes a repository build/export command rather than embedding compiler behavior. |

## Existing Diagnostic Key Registry

Current `game_editor` writes a `host == "game_editor"` result with a
`diagnostics` array of `key=value` strings. Existing keys:

- Core editor: `editor`.
- Map: `mapLoaded`, `mapObjectCount`, `mapSpawnPointCount`, `mapRuntimeEntityCount`, `mapRuntimeColliderCount`.
- Command script: `commandStatus`, `commandCount`, `commandTicksPlayed`.
- Move: `moveLoaded`, `movePhaseCount`, `moveHitboxCount`, `moveCancelCount`.
- Proxy animation: `proxyLoaded`, `proxySocketCount`.
- Character defaults: `characterLoaded`, `characterCount`.
- Scenario: `scenarioStatus`, `scenarioGoldenCompared`, `scenarioGoldenMatched`, `scenarioTraceEventCount`, `scenarioFinalStateHash`.
- Visual lab summary: `visualLabBuilt`, `visualLabPrimitiveCount`, `visualLabDebugLineVertexCount`, `visualLabPlaybackDuration`.
- Scenario-backed visual lab: `visualLabSource`, `scenarioId`, `scenarioTicksRun`, `scenarioGoldenPath`, `scenarioMessage`.
- Playback: `visualLabPlayback`, `playbackCurrentTick`, `playbackDurationTicks`, `playbackPaused`, `playbackRunning`, `playbackCanStepForward`, `playbackPreviewStepTick`, `playbackPreviewResetTick`, `playbackPreviewSeekEndTick`.
- Scenario evidence: `scenarioEvidenceEventCount`, `scenarioEvidenceFirstTick`, `scenarioEvidenceLastTick`, `scenarioEvidenceEffectEventCount`, `scenarioEvidenceHitEventCount`, `scenarioEvidenceBlockedHitEventCount`, `scenarioEvidenceTotalDamage`, `scenarioEvidenceMaxDamage`, `scenarioEvidenceLowestRemainingHealth`, `scenarioEvidenceHasDamage`, `scenarioEvidenceHasReaction`, `scenarioEvidenceReactionMoves`.

Existing process JSON fields outside `game_editor` that UI tests may cross-check:

- `map_command_runner`: `status`, `message`, `commandCount`, `createdObjectIds`, `diagnostics`, `entityCount`, `colliderCount`, `spawnPointCount`, `ticks`, `stateHash`, optional `savedPath`.
- `combat_scenario_runner`: `status`, `message`, `scenarioId`, `golden`, `goldenCompared`, `goldenMatched`, `trace.scenarioId`, `trace.seed`, `trace.ticksRun`, `trace.finalStateHash`, `trace.events`, `diagnostics`.

Reserved keys for later Sprint 03 lanes are listed in the feature matrix. When
implemented, they must be emitted by the relevant headless/process smoke before
the lane is considered testable.

## Visual QA Checklist By Surface

| Surface | Minimum screenshot evidence when code changes |
| --- | --- |
| Overview/status | Project path, current inputs, status bullets, and log/result feedback are readable. |
| Map authoring/preview | Map objects, spawn points, bounds, and invalid-map diagnostics are visible and not clipped. |
| Command script | Run result counts, created IDs, saved path, ticks, state hash, and diagnostics are visible. |
| Move controls | Move selector/timeline and guarded fields show valid ranges; invalid fields visibly block save/run. |
| Proxy animation | Preview tick change updates root/socket displayed values; required sockets are visible. |
| Character defaults | Character selector/tree, health, default moves, and invalid validation feedback are visible. |
| Scenario runner | Scenario status, golden compare/match, final hash, trace table, and diagnostics are visible. |
| Visual lab playback | Build state, summary counts, playback tick, seek range, scenario evidence, and diagnostics are visible. |
| Current scene viewport | Renderer-backed viewport is nonblank and framed around the current loaded scene. |
| Asset browser | Input directory, rescan status, asset list, selected renderable metadata, and empty/error states are visible. |
| Armature/weapon preview | Skeleton/clip/socket metadata, selected tick, weapon attachment, and trail sockets are visible. |
| Effects POC | Effect controls, validation state, preview trigger feedback, and visible particles/trail/fallback material are visible. |
| Game export | Export directory, build/export status, manifest/executable paths, and launch smoke result are visible. |

## Ticket Ownership Notes

- SP3-002 owns viewer extraction needed by `editor.current_scene.viewport`.
- SP3-003 owns the editor current-scene render viewport.
- SP3-004 owns the renderable asset browser.
- SP3-005 owns guarded datapoint editing for implemented map/move/character/scenario controls.
- SP3-006 owns game executable export.
- SP3-007 owns animated armature and weapon preview.
- SP3-008 owns `effect.sword_swing_poc`.

Any ticket that moves a label, guard, result key, or public editor data contract
from this document should send the appropriate v2 public-surface notice or
scope request before publishing the change.
