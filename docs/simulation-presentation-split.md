# Simulation And Presentation Split

This client should keep gameplay truth separate from visual presentation.

The authoritative layer is the simulation. It owns character state, action timelines, cancel windows, hitbox timing, movement intent, resource changes, and any data that must replay deterministically. Rendering, animation, VFX, audio, camera, and debug visualization should follow the simulation instead of becoming the source of truth.

## Direction

The action/combat state machine should be canonical:

- Character action: idle, run, light attack 02, dodge recovery, hitstun.
- Action phase: startup, active, recovery, cancel, blend-out.
- Frame or tick index: fixed-step time used for correctness.
- Gameplay windows: hitboxes, hurtboxes, invulnerability, armor, cancel permissions.
- Movement intent and root motion policy.
- Events emitted for presentation: animation cue, trail cue, impact cue, camera cue.

The visual state machine is a projection:

- Choose and blend animation clips.
- Sample poses.
- Spawn trails, particles, decals, sounds, camera effects, and debug overlays.
- Smooth over prediction corrections or late asset work.
- Render what the simulation said happened.

Animation notifies can still be authored data, but gameplay should consume them as deterministic timeline data. A render-frame callback should not be the reason damage happened.

## Practical Shape

Early engine modules can look like this:

```text
fixed tick simulation
  input commands
  character controller
  action state machine
  combat timeline
  hitbox and hurtbox system
  movement and physics intent

presentation
  animation graph
  pose sampling
  mesh and material rendering
  VFX, audio, camera
  debug views
```

The simulation can produce compact presentation commands each tick. The renderer can consume those commands at display rate and interpolate where needed. That gives us correctness first, then visual polish on top.

## Speed Model

Movement speed belongs to combat state, not rendering. Inputs should be dimensionless intent such as "move northeast at full stick tilt"; the combat layer turns that into velocity using authored constants.

The intent should carry an explicit frame conversion. Raw controls are local axes; character facing, camera yaw, or a lock-on target frame turns those axes into world-space movement when needed. For this prototype, player locomotion uses camera-relative strafing. The camera can freelook without rotating the player, right mouse button locks player facing to camera yaw, and backpedal always snaps facing to camera yaw so the character retreats toward the camera. That keeps "what the player meant" separate from "which way world +Z happens to be."

For now the prototype uses world units per second:

```cpp
inline constexpr float kPlayerMoveSpeedWorldUnitsPerSecond = 58.0f;
inline constexpr float kBackpedalSpeedScale = 1.0f / 3.0f;
inline constexpr float kFixedTickSeconds = 1.0f / 60.0f;
```

The healthy long-term model is the same, but with normalized scale: once character assets and arenas agree on a meter-like unit, the constants can become meters per second. Presentation should not decide how far a dodge, walk, or attack lunge moves. It should interpolate the previous and current combat transforms and render that state smoothly.

## C++ Bias

Use thin, explicit data types in hot logic:

```cpp
struct CharacterId { uint32_t value; };
struct ActionId { uint32_t value; };
struct Tick { int32_t value; };
```

Prefer value types, dense arrays, handles, spans, inline helpers, and explicit update functions. Avoid string lookups, heap allocation, virtual dispatch, and renderer dependencies in deterministic gameplay paths. The goal is readable semantics that still compile down to simple data movement.

## Current Prototype

The scene viewer is presentation-only. It loads the bootstrap scene, builds static draw geometry, and anchors the camera to the player so we can inspect controllable actors. It should not become gameplay authority; it is just the first rendering loop.
