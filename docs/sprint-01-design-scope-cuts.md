# Sprint 01 Scope Cuts from the Full Design

Source design: `docs/action_combat_engine_editor_design.md`

This document records what we are intentionally crossing off for Sprint 01. Crossed off means deferred, not rejected. The goal is to protect the first sprint from becoming a full engine/editor rewrite while still creating a real map and combat authoring loop.

## Sprint 01 Focus Kept

These parts of the full design remain mandatory for Sprint 01:

- CMake and CTest as the canonical interface.
- Characterization tests before major extraction.
- A headless fixed-tick path.
- Authoring data separate from runtime state.
- Stable identities for authored objects and runtime entities.
- Versioned map and move schemas.
- Tick-based combat windows with half-open ranges `[begin, end)`.
- Semantic input frames rather than platform key codes in move definitions.
- Golden event traces and state hashes.
- Visual debug inspection as a consumer of runtime state.

## Crossed Off for Sprint 01

| Full design area | Crossed off for Sprint 01 | Minimum substitute | Return trigger |
|---|---|---|---|
| Full `game_editor` host | Docking editor shell, asset browser, hierarchy, inspector, polished gizmos | Command-script map authoring plus visual combat lab | Map schema and RuntimeWorld compile path are stable |
| Full editor document lifecycle | Recent files, autosave recovery, dirty prompts, Save As UX | Deterministic load/save/reopen fixtures and command scripts | GUI editor work begins |
| Full undo/redo system | General command transactions for all editor actions | Deterministic command execution; optional apply/revert tests for map commands | Transform gizmos and inspector editing start |
| GPU object-ID picking | R32_UINT picking pass and delayed readback | CPU/simple debug selection or no selection in visual lab | Arbitrary mesh authoring needs viewport picking |
| Offscreen editor viewport | ImGui viewport texture, resize handling, UI final pass | Existing swapchain viewer/lab with fixed-frame smoke tests | Real editor shell starts |
| Asset registry and cook pipeline | GUID registry, derived binary cache, import manifests, reimport UI | Versioned JSON fixtures and direct paths for sprint assets | Imported static/skinned assets become part of authoring loop |
| Indexed mesh renderer | Index buffers, submeshes, material slots, instancing | Existing flattened geometry plus primitive debug geometry | Materials, sockets, or skeletal work requires mesh structure |
| Material/texture system | Descriptor-set material pipeline, texture import, alpha modes | Base color/debug color only | Static mesh authoring needs materials/textures |
| Skeletal import and GPU skinning | Skeleton hierarchy import, clip import, skin matrices | Proxy animation/keyframe tracks for root and named sockets | Need real character/weapon animation fidelity |
| Full animation state graph | Animation graph/blend editor and locomotion blending | Move selects proxy animation/keyframe track | Multiple real clips need blending |
| Socket editor | Bone-relative socket editing UI | Named proxy sockets in data fixtures | Real skeleton and weapon alignment begins |
| Weapon asset/equipment system | Equipment compatibility, runtime attachment, reliable replication | Single training weapon proxy encoded as move/socket data | Character/weapon preview and replication work starts |
| Jolt character controller | Full physics interface and Jolt-backed capsule controller | Simple ground, bounds, primitive blockers, and explicit overlap/sweep queries | Maps need robust collision/steps/slopes |
| Full collision layer matrix | Complete layer/mask data-driven physics integration | Minimal world, character, hurtbox, hitbox query categories | Physics backend lands |
| Authoritative networking | Input-authoritative server, prediction, reconciliation | Keep current relay characterized; combat scenarios run offline | Multiplayer combat integrity sprint starts |
| Reliable network channels | Sequence/ack/resend and logical channels | Existing UDP snapshot tests only | Server-authoritative input protocol begins |
| Network fault injection | Loss, jitter, reorder, duplication harness | Current network E2E remains as compatibility check | Prediction/reconciliation exists |
| Effect system | Effect assets, pooling, deduplication, trails, particles | Combat event records for effect/audio/camera cues | Visual presentation sprint begins |
| Render golden image gates | Fixed camera image diff artifacts | Viewer/lab smoke plus validation where available | Renderer changes become substantial |
| Sanitizer/fuzz suite | ASan/fuzz/nightly setup for all binary readers | Targeted invalid JSON and parser unit tests | Binary cook format and broader decoders appear |
| Prefabs | Prefab assets, overrides, nested prefabs | Flat authored scene with primitive entities | Reuse pressure appears in maps/characters |
| Terrain/CSG/navmesh | Advanced map construction tools | Primitive shapes, spawn points, bounds, kill height | Basic combat spaces are no longer enough |
| Production performance gates | CI hardware baselines, soak/perf thresholds | Determinism and bounded test runtime | Engine becomes large enough to regress performance often |

## Crossed Off by Design Section

### Section 5: Target Architecture

Kept:

- Host/library split direction.
- Immediate ownership corrections.
- Headless simulation requirement.

Deferred:

- Full target list implementation.
- Replacing the viewer completely.
- All final host loops in production form.

Sprint substitute:

- Add the minimum libraries/executables needed for headless scenarios and visual lab. Preserve the legacy viewer until parity is proven.

### Section 6: World and Scene Model

Kept:

- Strong IDs.
- Authoring versus runtime distinction.
- Primitive scene components.
- Validation and canonical serialization.

Deferred:

- Prefab system.
- Complex parent/child editing UX.
- Full component catalog.

Sprint substitute:

- Flat primitive maps with optional parent transforms only if needed. Compile into a simple RuntimeWorld.

### Section 7: Asset Registry, Import, and Cooking

Kept:

- Source/runtime separation as a design constraint.
- Primitive asset generation.

Deferred:

- Asset registry.
- Derived binary format.
- Full import settings and reimport cache.

Sprint substitute:

- Direct fixture paths and explicit schema versions. Avoid pretending direct source import is the final runtime path.

### Section 8: Vulkan Renderer Evolution

Kept:

- Debug draw.
- Viewer smoke.
- Renderer consumes runtime/presentation state.

Deferred:

- Offscreen editor viewport.
- Descriptor-set material system.
- Indexed mesh and instancing.
- Skeletal skinning.
- GPU object-ID picking.

Sprint substitute:

- Existing renderer path plus enough debug lines/primitives to inspect maps, hitboxes, hurtboxes, keyframes, and interpolation.

### Section 9: Editor Application

Kept:

- Map authoring minimum slice as product intent.
- Command-script e2e strategy.

Deferred:

- Dear ImGui docking shell.
- Full panel set.
- Full document lifecycle.
- Undo/redo and transform gizmos.

Sprint substitute:

- Command-script authoring and a visual lab. This gives design iteration without committing to GUI architecture too early.

### Section 10: Physics and Collision

Kept:

- Engine-owned collision/query concepts.
- Primitive hit/hurt volumes.
- Stable hit ordering.
- Collision must not leak renderer or platform types.

Deferred:

- Jolt integration.
- Full character controller.
- Full layer/mask matrix.

Sprint substitute:

- Minimal deterministic primitive queries and map bounds sufficient for combat language tests.

### Section 11: Skeletal Animation

Kept:

- Tick-sampled animation data must be reproducible headlessly.
- Root motion is simulation data, not final rendered pose magic.
- Socket transforms feed both visuals and combat queries.

Deferred:

- FBX skeletal import.
- Clip sampling for real skeletons.
- GPU skinning.
- Animation golden images.

Sprint substitute:

- Proxy root and socket keyframes in JSON. This lets us design move timing, interpolation, and weapon trails before real animation lands.

### Section 12: Attachments and Weapons

Kept:

- The conceptual need for weapon base/tip sockets.
- Combat and visuals should consume the same socket chain.

Deferred:

- Weapon assets.
- Compatibility tags.
- Equip/unequip runtime and replication.

Sprint substitute:

- A single training sword proxy represented by named socket tracks on the move/animation fixture.

### Section 13: Combat Language

Kept:

- This is a core sprint feature.
- Semantic `InputFrame`.
- Move phases.
- Cancels.
- Hit/hurt model.
- Hitstop/stun.
- Movement/facing tracks.
- Ordered event stream.
- Golden traces.

Deferred:

- Deep resource economy.
- Full guard/block/clash system unless needed for one fixture.
- Complex command grammar.
- Full move editor timeline UI.

Sprint substitute:

- JSON-authored move fixtures, validator/compiler, headless scenario runner, and visual debug overlay.

### Section 14: Effects

Kept:

- Effects are presentation-only consumers of combat events.
- Event IDs must be stable for future prediction/deduplication.

Deferred:

- Effect assets, particles, trails, pooling, audio, camera shake.

Sprint substitute:

- Emit effect/audio/camera cue events into traces. Draw simple debug markers if useful.

### Section 15: Networking

Kept:

- Current relay remains characterized.
- Sprint code should not deepen client-transform authority.

Deferred:

- Authoritative server.
- Prediction/reconciliation.
- Reliable channels.
- Lag-aware melee validation.

Sprint substitute:

- Offline combat correctness first. Networked combat follows when the language is testable.

### Section 18 and 19: Build and Tests

Kept:

- CMake presets.
- CTest labels.
- Result files.
- Test support direction.
- Golden traces.

Deferred:

- Full CI matrix.
- Nightly fuzz/soak gates.
- Full static analysis and sanitizer presets if they slow the first sprint.

Sprint substitute:

- Local preset-driven suite with clear labels and deterministic artifacts.

## Risks Accepted for Sprint 01

| Accepted risk | Why acceptable | Guardrail |
|---|---|---|
| Proxy animation may not match final skeleton behavior | It unblocks combat language design early | Keep proxy format explicitly temporary and test tick sampling semantics |
| Visual lab is not a real editor | It gives fast inspection without UI architecture debt | Do not put gameplay rules in the visual lab |
| Minimal collision may be replaced | We need stable combat tests before Jolt integration | Keep collision behind small query functions/interfaces |
| Asset registry is absent | Fixture-driven data is enough for first sprint | Do not use file paths as permanent authored identity in new schemas |
| Networking is not combat-authoritative yet | Offline combat correctness is prerequisite | Characterize current network behavior and avoid expanding transform authority |

## What We Must Not Cross Off

These cannot be deferred without damaging the sprint:

- Deterministic fixed tick semantics.
- Stable event ordering.
- Half-open tick ranges.
- Headless scenario runner.
- Golden traces.
- Result files for process tests.
- Validation diagnostics that point to bad data.
- Separation between authored data, runtime state, and presentation.
- CMake/CTest ownership of acceptance.

## Re-entry Rules

A crossed-off area can re-enter scope only when one of these is true:

- A sprint fixture cannot be authored or inspected without it.
- A test cannot be made deterministic without it.
- A later feature would create worse rework if the foundation is not added now.
- The user explicitly chooses product value over sprint containment.

When re-entering scope, add a ticket with a narrow exit criterion rather than importing the whole deferred subsystem.
