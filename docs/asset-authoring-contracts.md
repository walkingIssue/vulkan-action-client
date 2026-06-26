# Asset Authoring Contracts

Updated: 2026-06-26

This document is the working contract between DCC tools such as Maya and the `vulkan-action-client` editor/runtime.

The short version: Maya owns source art. The game editor/engine owns import settings, stable asset IDs, gameplay meaning, sockets, effect triggers, shader/material parameters, validation, preview, and export.

## Current Coordinate Contract

The current project scene convention is:

- `unitsPerMeter`: `1.0`
- `coordinateSystem`: `RH_Y_UP_NEG_Z_FORWARD`

Maya exports may use different native units or axes, but the importer must record the conversion. Keep source assets meter-scaled and consistent; avoid relying on silent export-scale fixes.

## What To Provide From Maya

For the first useful character/weapon animation pack, provide:

- one rigged humanoid character mesh
- one or two weapon meshes
- a clean bind pose
- a simple skeleton hierarchy with stable, readable bone names
- skin weights with no more than four meaningful influences per vertex for the initial path
- separate clips for idle, light attack/swing, hit reaction, and optional run/dodge
- weapon attachment markers or clear notes for `hand_r`, `hand_l`, `weapon_base`, and `weapon_tip`
- UVs and texture references if available
- source files plus exported interchange files, preferably FBX and optionally GLB/glTF

Keep a small first pack. One clean character, one sword/weapon, and three to five short clips will expose more engine/editor issues than a large library.

## Naming Guidelines

Use stable names because the engine will validate references by name or stable imported ID:

- bones: readable names such as `Root`, `Hips`, `Spine_01`, `RightHand`, `LeftHand`
- sockets/markers: `hand_r`, `hand_l`, `weapon_base`, `weapon_tip`
- clips: `idle`, `light_attack_01`, `hit_reaction`, `run`, `dodge`
- weapons: `weapon.training_sword`, `weapon.training_axe`, etc.

Avoid changing names after the assets are referenced by editor/game data. If a rename is necessary, it should become an explicit migration.

## Rig And Animation Constraints

Initial importer/runtime expectations:

- one skeleton root
- acyclic hierarchy
- finite transforms and normalized rotations
- no dependency on Maya constraints at runtime; bake animation to joints before export
- no runtime IK requirement for the first pass
- root motion is explicit: either authored as a root curve or deliberately absent
- gameplay sampling happens at fixed simulation ticks, not render frames
- skeleton topology changes invalidate dependent clips, sockets, weapons, and characters

## Weapon And Socket Contract

Weapon gameplay and presentation share the same socket chain.

The editor/engine must be able to:

- attach a weapon to `hand_r` or another compatible slot
- preview attachment alignment on a selected animation clip
- sample `weapon_base` and `weapon_tip` for trails and swept hit volumes
- validate that move hitboxes and effect trails reference existing sockets
- save/reopen socket offsets and reproduce transforms at fixed ticks

Maya can provide locator/empty markers, but the editor is the authority for saved socket metadata and compatibility tags.

## Effects And Shaders Contract

Visual effects are usually authored in the engine/editor, not wholly in Maya.

DCC tools can provide:

- meshes used by effects
- textures, masks, sprite sheets, UVs, and vertex colors
- optional locator names or animation event notes

The editor/engine owns:

- effect asset schemas
- material/shader parameter ranges
- particle/trail/camera/audio effect timelines
- gameplay event bindings
- validation, preview, pooling limits, and fallback behavior

Effects consume combat events. They must not define damage, hit validity, invulnerability, armor, cancel rules, or authoritative gameplay state.

Shader source and runtime shader variants are engine code/assets. The editor should expose material parameters and effect controls rather than asking Maya to define engine shader behavior.

## Visual Effects Authoring POC

The first useful effect-authoring milestone should be small and tactile:

```text
effect.sword_swing_poc
  material/shader: unlit or additive color
  particle node: burst from a socket or offset
  trail node: ribbon/line between weapon_base and weapon_tip
  preview: explicit editor button
```

Required editor-authored fields:

- effect logical ID
- duration in ticks
- material/shader mode
- RGBA color
- particle count
- particle lifetime in ticks
- particle size, speed, spread, gravity, and drag
- spawn tick
- spawn socket or offset
- trail enabled flag
- trail base socket, tip socket, width, lifetime, and fade

Suggested guard ranges for the POC:

- duration ticks: `1..600`
- spawn tick: `0..durationTicks`
- particle count: `0..512`
- particle lifetime ticks: `1..240`
- particle size: `0.001..10.0`
- speed: `0.0..50.0`
- spread degrees: `0.0..180.0`
- gravity scale: `-10.0..10.0`
- drag: `0.0..20.0`
- trail width: `0.001..2.0`
- color channels: `0.0..1.0`

The POC may preview from an editor origin or a named socket. Once character/weapon preview exists, it should also preview the weapon trail during a swing clip. It still remains visual-only: damage, hit validity, target filters, and area rules stay in combat/move data.

## Import Validation Requirements

Every imported animated asset should eventually produce a semantic manifest with:

- source path and content hash
- importer version/settings
- unit and axis conversion
- node count
- mesh/submesh count
- material slots
- vertex/index count
- bounds
- bone count and root bone
- clip names, durations, and sample rates
- socket/marker names
- diagnostics for unsupported features

This lets agents and tests compare imports without relying only on screenshots.

## First Acceptance Target

The first "real toy around" milestone should be:

1. Import/cook one skeletal character and one weapon.
2. Display skeleton, skin, and sockets in the editor.
3. Scrub `idle` and `light_attack_01`.
4. Attach the weapon to `hand_r`.
5. Show `weapon_base` and `weapon_tip` following the pose.
6. Trigger a simple trail/spark effect preview from the swing.
7. Save/reopen and verify identical socket/weapon transforms at fixed ticks.
