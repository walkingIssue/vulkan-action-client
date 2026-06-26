# Sprint 02 Implementation Plan: Combat Semantics and Character Foundations

Status: interrupted after SP2-004. `SP2-005` was never dispatched and is deferred by the Sprint 03 reset.

Active replacement plan: `docs/sprint-03-implementation-plan.md`

Source design: `docs/action_combat_engine_editor_design.md`
Source closeouts:
- `docs/sprint-01-implementation-plan.md`
- `docs/sprint-01-risk-mitigation-plan.md`
- `docs/tickets/SP1-007-combat-runtime-v0_result.md`
- `docs/tickets/SP1-008-primitive-combat-collision_result.md`
- `docs/tickets/SP1-010-scenario-runner-goldens_result.md`
- `docs/tickets/SP1-011-visual-combat-lab_result.md`
- `docs/tickets/RM1-003-cross-asset-validation_result.md`
- `docs/tickets/RM1-004-scenario-visual-lab_result.md`

Sprint 02 turns the Sprint 1 combat loop from timing/collision proof into the first durable combat semantics layer. The goal is still not a full game, editor, animation stack, or networked combat product. The goal is to make damage, reactions, actor ownership, and golden trace governance explicit enough that later visual, editor, asset, and networking work can consume them without inventing rules in the wrong layer.

## Product Outcome

A developer should be able to:

1. Author checked-in combat scenarios where hits apply deterministic damage and reactions.
2. Inspect event traces and final state hashes that include damage, remaining health, reaction/effect identifiers, and timing state that affects combat truth.
3. Move temporary scenario-authored actor defaults toward minimal checked-in character definitions without depending on ignored local `assets/`.
4. Update golden traces through a documented guarded workflow with enough metadata to review intentional drift.
5. Use the visual lab as a consumer of scenario/golden evidence, not as the owner of combat correctness.
6. Keep the current network protocol stable and observable without freezing premature authoritative combat networking.

## Guiding Decisions

### Headless Combat Truth

Gameplay correctness stays in `combat_runtime_core`, `combat_scenario_core`, fixtures, traces, hashes, and CTest. Viewer diagnostics and screenshots are inspection aids only.

### Scenario Bridge Before Final Character Assets

`SP2-001` may add provisional scenario-owned health/resource/effect fields so damage semantics can be proven quickly. `SP2-002` owns replacing that bridge with minimal character definitions and spawn ownership.

### Checked-In Fixtures Before Imported Assets

Sprint 02 uses tracked JSON and proxy/primitive fixtures. Do not require ignored extracted assets, full asset cooking, skeletal import, GPU skinning, or the paladin bootstrap scene for sprint correctness.

### Golden Governance Before Trace Sprawl

Every trace schema expansion should preserve readable mismatch diagnostics. Full governance lands in `SP2-003`; any tiny guard needed by `SP2-001` should be documented in that ticket result.

### Visual And Network As Consumers

The visual lab and network stack can expose results, contracts, and diagnostics after combat semantics exist. They should not define damage, reactions, character ownership, or authoritative combat replication in Sprint 02.

### Coordinator-Gated Work

Multiple agents are active on this machine. Implementation starts only from a Mia dispatch or explicit user override. Each implementation branch must post checkout, branch, base, likely hot files, remote/fetch evidence, clean tracked state, untracked state, and verification gates before editing.

## Required Data Flow

```text
move JSON + scenario JSON + character JSON
  -> content/scenario validation
  -> CombatRuntime actor state and hit/effect application
  -> ScenarioTraceEvent damage/reaction/resource fields
  -> final state hash including combat-affecting state
  -> guarded golden comparison/update workflow
  -> visual lab and network diagnostics consume stable evidence
```

Ownership boundaries:

- `combat_runtime_core` owns deterministic actor combat state, effect application, hitstop/stun/reaction timing, and once-per-target damage behavior.
- `combat_scenario_core` owns scenario bridge data, scenario result JSON, trace schema emission, final hash, and golden comparison.
- Character definition content owns default health, default hurtboxes, default moves, and spawn ownership only after `SP2-002`.
- `visual_lab_core` owns display/playback of scenario evidence, not combat rules.
- `network` owns current packet/result contracts only until a later sprint defines authoritative combat networking.

## Sprint Tickets

### SP2-001: Combat Effects and Damage/Reactions v1

Branch: `sprint02/sp2-001-combat-effects-damage-reactions`

Goal: integrate first-pass damage, remaining health, hit reaction/effect identifiers, and combat-affecting timing state into runtime/scenario traces and goldens.

Primary owner recommendation after this plan lands: Vera, because the planning review identified the combat/scenario/golden validation boundaries in detail. Mia still dispatches the actual owner.

### SP2-002: Character Definition and Spawn Ownership v1

Branch: `sprint02/sp2-002-character-definitions-spawn-ownership`

Goal: introduce minimal tracked character definitions so default combat state, hurtboxes, move lists, and spawn ownership stop living only in scenario-local bridge fields.

### SP2-003: Golden Governance and Update Workflow v1

Branch: `sprint02/sp2-003-golden-governance-update-workflow`

Goal: make scenario golden updates harder to do accidentally and easier to review as damage/reaction traces grow.

### SP2-004: Scenario Playback Controls in Visual Lab v1

Branch: `sprint02/sp2-004-scenario-playback-controls`

Goal: add basic visual lab scenario playback controls after damage/reaction traces and actor ownership boundaries are stable enough to display.

### SP2-005: Network Compatibility Snapshot v1

Branch: `sprint02/sp2-005-network-compatibility-snapshot`

Goal: document and test the current relay/protocol/result contract as a late/stretch hardening ticket. This is not required for the core Sprint 02 combat path unless Mia explicitly pulls it forward.

## Recommended Ticket Order

```text
SP2-001 Combat effects and damage/reactions v1
SP2-002 Character definition and spawn ownership v1
SP2-003 Golden governance and update workflow v1
SP2-004 Scenario playback controls in visual lab v1
SP2-005 Network compatibility snapshot v1, late/stretch
```

`SP2-003` may move immediately before the final `SP2-001` merge if the damage/reaction trace update needs a broader manifest/update policy before landing. Otherwise, `SP2-001` should include only the smallest guardrail it needs and leave full governance to `SP2-003`.

## Coordination Rules For Sprint 02

Before any implementation branch edits files, the owner must post to the board:

- Agent name and checkout path.
- Branch name and exact base commit.
- Confirmation that `git fetch --prune origin` works, including whether SSH or HTTPS is being used.
- Tracked status and untracked status, especially `assets/`.
- Likely hot files and any shared headers, CMake files, viewer files, scenario fixtures, or goldens.
- Focused tests, full tests, result artifact expectations, and MSVC runtime/assertion dialog check.

Hot files that should be serialized through Mia:

- `CMakeLists.txt`
- `CMakePresets.json`
- `src/vulkan_scene_viewer.cpp`
- public combat, scenario, content, visual lab, and network headers
- `tests/fixtures/scenarios/*.scenario.json`
- `tests/fixtures/scenarios/goldens/*.golden.json`
- shared content fixtures under `content/`

Parallel review-only work is allowed when dispatched. Parallel implementation work should wait until `SP2-001` and `SP2-002` settle the combat/character ownership boundary.

## Verification Gates

Every implementation ticket follows `docs/agent-ticket-workflow.md`: start doc, feature branch, focused verification, broader preset verification, result doc, serialized merge, and push.

Minimum Sprint 02 gates:

- Focused CTest for the subsystem touched.
- `cmake --build --preset msvc-debug`.
- `ctest --preset msvc-debug --output-on-failure`.
- Structured result artifacts when an executable/test writes them.
- Explicit note about whether `assets/` is untouched.
- MSVC runtime/assertion dialog check after native verification.

## Non-Goals

- Full Dear ImGui editor.
- Full asset registry/cook pipeline.
- Skeletal animation import, sockets, clips, and GPU skinning.
- Production debug-render architecture.
- Authoritative combat networking, rollback, command replication, matchmaking, reliability, prediction, or reconciliation.
- Network compatibility promises for future combat commands/events before those semantics exist.
- Replacing the current visual lab with final editor document ownership.

## Intake Guardrail Supersession

`docs/tickets/SP2-001-intake-summary.md` was a historical stop sign created before Sprint 02 existed. It is superseded by this plan and remains only to prevent stale agents from treating the old placeholder as active work. The actionable `SP2-001` ticket is now `docs/tickets/SP2-001-combat-effects-damage-reactions.md`.
