# SP1-007: Combat Runtime v0

Status: planned
Branch: sprint01/sp1-007-combat-runtime-v0
Start commit: pending
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 13.1: Combat model
- `docs/action_combat_engine_editor_design.md` section 13.2: Input actions and command buffer
- `docs/action_combat_engine_editor_design.md` section 13.4: Move phases and state tags
- `docs/action_combat_engine_editor_design.md` section 13.5: Cancel model
- `docs/action_combat_engine_editor_design.md` section 13.6: Hit and hurt model

## Goal

Add the first fixed-tick combat runtime that consumes compiled move data, buffers semantic commands, starts authored moves, advances move ticks, emits stable phase/event diagnostics, applies simple cancel checks, and tracks hitstop/stun counters. This makes the SP1-006 move schema executable enough for boundary tests while leaving actual hitbox overlap to SP1-008.

## Non-Goals

- Do not implement primitive hit/hurt overlap, swept collision, damage, or once-per-target hit resolution; SP1-008 owns that.
- Do not add proxy animation/root motion/socket sampling; SP1-009 owns that.
- Do not build scenario runner/golden trace files; SP1-010 owns that.
- Do not integrate the viewer/client with combat runtime yet.
- Do not implement full cancel graph/resource/priority semantics beyond the planned v0 checks.

## Current Baseline

- SP1-005 added fixed-tick `simulation_core::InputFrame`.
- SP1-006 added compiled move tables with phases, events, cancels, tags, and interned IDs.
- No runtime actor state currently stores active moves, move ticks, state tags, command buffers, hitstop, stun, or per-move hit registry.

## Data Flow

```text
compiled move table
  -> CombatMoveLibrary
  -> semantic InputFrame pressed command
  -> command buffer by tick
  -> CombatActorState active move + move tick
  -> phase/event/cancel diagnostics
  -> fixed-tick CTest assertions
```

Ownership boundaries:

- `content_core` remains the move schema/compiler owner.
- Combat runtime consumes compiled move data and semantic commands; it does not parse JSON.
- Collision/runtime hit resolution is represented only as test-controlled hit/block/whiff placeholders in this ticket.

## Implementation Plan

1. Add `combat_runtime` types under the existing combat module or a small combat runtime target.
2. Define `CombatActorState` with active move ID, move instance sequence, move tick, state tags, hitstop/stun counters, command buffer, and hit registry placeholder.
3. Add `CombatMoveLibrary` over compiled moves with deterministic command-to-move lookup.
4. Add command buffering from semantic input frames with buffer age measured in ticks.
5. Add fixed tick advancement for active move start, startup/active/recovery phase enter events, authored move events, move completion, and hitstop/stun countdown.
6. Add cancel checks for whiff, hit/block placeholder, always, and dodge destination moves.
7. Add authored dodge and hit reaction fixtures or compiled test moves rather than hard-coded runtime branches.
8. Add tests for phase boundaries, input buffer timing, cancel-on-hit and no-cancel-on-whiff, hitstop/stun counters, and stable event IDs.

## Test Plan

- Unit:
  - Startup/active/recovery boundary events are emitted on expected ticks.
  - Buffered command starts when the actor is available and expires after buffer window.
  - Cancel-on-hit succeeds only when the hit placeholder is present.
  - Cancel-on-whiff does not fire for hit-only cancel windows.
  - Always/dodge cancel can transition to authored dodge.
  - Hitstop and stun counters decrement deterministically.
  - Event IDs are stable across repeated runs.
- Process/CTest:
  - `ctest --preset msvc-debug-unit`
  - `ctest --preset msvc-debug-content`
  - `ctest --preset msvc-debug-simulation`
  - `ctest --preset msvc-debug`

## Acceptance Criteria

- `CombatActorState` exists with active move, move tick, state tags, hitstop/stun counters, command buffer, and hit registry placeholder.
- Semantic commands from fixed-tick input can start authored moves.
- Startup/active/recovery and authored move events are emitted with stable IDs.
- Cancel-on-hit and no-cancel-on-whiff behavior is covered by tests.
- Dodge and hit reaction are represented as authored compiled moves, not hard-coded branches.
- Existing Sprint 1 content, simulation, characterization, network, and viewer smoke tests remain green.
- No generated build artifacts or untracked assets are committed.

## Risks and Watchpoints

- Keep this runtime event-oriented; do not smuggle SP1-008 collision into this ticket.
- Avoid letting command matching depend on JSON array order.
- Make placeholder hit/block/whiff test state explicit so it does not masquerade as real collision.
- Keep authored dodge/hit reaction data small but real enough for SP1-007 tests.

## Progress Log

- 2026-06-20: Started after SP1-006 move schema/compiler was merged.

## Verification Results

Pending.

## Final Commits

Pending.
