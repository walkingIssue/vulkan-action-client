# SP3-008: Visual Effects Authoring POC v1

Status: planned
Branch: sprint03/sp3-008-visual-effects-authoring-poc
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Source contract:
- `docs/asset-authoring-contracts.md`
- `docs/action_combat_engine_editor_design.md` section 14
Dependencies:
- `SP3-001` for UI label/guard conventions.

## Goal

Implement a minimal engine/editor-owned visual-effects authoring proof of concept so weapon swings and preview events can trigger visible particles/trails without moving gameplay truth into DCC files or renderer-only code.

## Non-Goals

- Do not build a full shader graph.
- Do not build a full particle editor.
- Do not make effects define damage, hit validity, invulnerability, armor, or cancel rules.
- Do not require custom Maya plugin work.

## Contract Split

DCC tools may provide:

- effect meshes
- textures, masks, sprite sheets, UVs, vertex colors
- locator names or animation event notes

The engine/editor owns:

- material assets and shader parameter ranges
- effect asset schema and timeline nodes
- spawn sockets and trail sockets
- particle/trail/camera/audio effect controls
- preview triggers and diagnostics
- pooling and overflow policy
- gameplay event bindings

## MVP Effect

The POC effect should be concrete:

```text
effect.sword_swing_poc
  material/shader: unlit or additive color
  particle burst: socket/offset spawn with count, lifetime, size, speed, spread, gravity, drag
  weapon trail: optional trail between weapon_base and weapon_tip with width, lifetime, fade
  preview: explicit editor button independent of combat truth
```

Initial authored files can live under future `content/effects/` and `content/materials/` paths, or equivalent paths chosen by the implementation ticket. They must be tracked fixtures when used by tests.

## Required UI Controls

- effect asset picker/path
- `Preview Effect` button
- material/shader mode combo, initially `unlit` and optional `additive`
- color RGBA controls with `0.0..1.0` guards
- duration ticks input with `1..600` guard
- spawn tick input with `0..durationTicks` guard
- spawn socket combo/text input and/or local offset input
- particle count input with `0..512` guard
- particle lifetime ticks input with `1..240` guard
- particle size input with `0.001..10.0` guard
- particle speed input with `0.0..50.0` guard
- spread degrees input with `0.0..180.0` guard
- gravity scale input with `-10.0..10.0` guard
- drag input with `0.0..20.0` guard
- trail enable toggle
- trail base socket, tip socket, width, lifetime, and fade controls

## Implementation Plan

1. Add the minimal effect/material schema or fixture JSON required for `effect.sword_swing_poc`.
2. Add parser/validation for material mode, effect duration, particle burst parameters, and weapon-trail parameters.
3. Add editor UI controls listed above with labels, guards, invalid states, and a preview trigger.
4. Make preview independent of combat truth, with optional binding to a selected socket/weapon preview when available.
5. Validate that effect references point to known materials, sockets, and allowed parameter ranges.
6. Emit result diagnostics for selected effect, material/shader mode, duration, node count, particle count, spawn socket/offset, trail settings, preview status, and validation diagnostics.
7. Add a small headless authoring/preview smoke path so the POC can be tested without visual capture.

## Acceptance Criteria

- Effects are data assets consumed by gameplay events, not owners of gameplay rules.
- The editor exposes labeled guarded controls for effect/material parameters.
- Missing shader/material/effect references produce fallback/error diagnostics.
- A simple sword swing effect can be previewed independently of a combat hit.
- Result diagnostics prove the selected effect, material/shader mode, particle count, duration, spawn socket/offset, and trail settings.
- Invalid values are rejected or visibly marked before preview/save.

## Test Plan

- Unit tests for effect/material schema validation.
- Headless preview/result smoke for `effect.sword_swing_poc`.
- Visible/capturable editor screenshot of effect controls and preview state.
- Renderer/material tests as shader integration appears.
- MSVC runtime/assertion dialog check.
