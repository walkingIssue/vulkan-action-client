# SP3-008: Effects, Materials, and Shader Contract v1

Status: planned
Branch: sprint03/sp3-008-effects-materials-shader-contract
Start commit: TBD by Mia dispatch
Source plan: `docs/sprint-03-implementation-plan.md`
Source contract:
- `docs/asset-authoring-contracts.md`
- `docs/action_combat_engine_editor_design.md` section 14
Dependencies:
- `SP3-001` for UI label/guard conventions.

## Goal

Define the first engine/editor-owned contract for effects, materials, and shader parameters so weapon swings can trigger visual effects without moving gameplay truth into DCC files or renderer-only code.

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

## Implementation Plan

1. Document or add the initial effect/material JSON schema.
2. Define allowed shader/material parameters with labels, types, ranges, and defaults.
3. Add editor controls for at least one simple effect type such as weapon trail or hit spark.
4. Add a preview trigger button that does not depend on combat truth.
5. Validate that effect references point to known materials, sockets, and allowed parameter ranges.
6. Emit result diagnostics for selected effect, duration, node count, material, spawn socket, and preview status.

## Acceptance Criteria

- Effects are data assets consumed by gameplay events, not owners of gameplay rules.
- The editor exposes labeled guarded controls for effect/material parameters.
- Missing shader/material/effect references produce fallback/error diagnostics.
- A simple weapon swing effect can be previewed independently of a combat hit.

## Test Plan

- Unit tests for effect/material schema validation.
- Headless preview/result smoke for at least one effect asset.
- Visible/capturable editor screenshot of effect controls and preview state.
- Renderer/material tests as shader integration appears.
- MSVC runtime/assertion dialog check.
