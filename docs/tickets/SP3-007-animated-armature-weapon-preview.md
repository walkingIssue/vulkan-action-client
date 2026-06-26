# SP3-007: Animated Armature and Weapon Preview v1

Status: planned
Branch: sprint03/sp3-007-animated-armature-weapon-preview
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Source contract:
- `docs/asset-authoring-contracts.md`
- `docs/action_combat_engine_editor_design.md` sections 7, 11, and 12
Dependencies:
- Prefer after `SP3-004`.

## Goal

Make animated armature and weapon loading a hard editor requirement: the editor must import or classify skeletal source assets, display skeleton/clip/socket metadata, preview clips at fixed ticks, attach a weapon to a named socket, and validate weapon trail/socket contracts.

## Non-Goals

- Do not build full animation retargeting.
- Do not build a full animation graph editor.
- Do not require production asset cooking before the first preview path.
- Do not make rendered pose sampling the gameplay authority.
- Do not commit user-owned source assets unless the user explicitly provides fixture assets for the repo.

## Required Inputs

Initial accepted source formats:

- `.fbx`
- `.glb`
- `.gltf`

The editor/import path must record unit and axis conversion into `RH_Y_UP_NEG_Z_FORWARD` with `unitsPerMeter = 1.0`.

## Implementation Plan

1. Add an animated asset import/classification result that reports skeleton, skin, clip, material-slot, and socket/marker metadata.
2. Expose a character/weapon preview panel in `game_editor`.
3. Add a clip picker and guarded preview tick/frame slider.
4. Display bone count, clip count, clip duration, selected tick, and socket names.
5. Attach a selected weapon to `hand_r` or another compatible socket.
6. Display and validate `weapon_base` and `weapon_tip` trail sockets.
7. Add headless manifest/result diagnostics for fixed-tick socket and weapon transforms.

## Acceptance Criteria

- The editor can inspect at least one skeletal source asset and one weapon asset.
- The UI exposes labeled controls for asset path, clip, preview tick, socket, and weapon attachment.
- Invalid/missing skeletons, clips, weights, or sockets produce clear diagnostics.
- The same fixed-tick pose/socket result is reproducible without Vulkan.
- No local/user asset payloads are staged unintentionally.

## Test Plan

- Unit tests for manifest parsing/classification and socket validation.
- Integration fixture for a small skeletal asset when a repo-safe fixture exists.
- Headless editor smoke producing bone/clip/socket/weapon transform diagnostics.
- Visible/capturable editor screenshot of the character/weapon preview.
- MSVC runtime/assertion dialog check.
