# Direct Agent Communication Protocol v2

## Purpose

Use Codex direct-agent messages as the low-latency coordination channel. Use Git and the local board for durable state, not for delivery or polling.

This protocol keeps coordinator authority intact while allowing workers to resolve the only peer-level hazard that requires immediate coordination: a public surface that another active worker consumes or may change concurrently.

## Runtime Scope

This protocol assumes the workers are addressable agent threads inside the same Codex multi-agent workflow. Codex can route instructions and messages within that agent tree. Separate top-level Codex sessions should be treated as separate systems: route through the coordinator/host or use the fallback board rather than assuming a direct peer address exists.

External user/app input should enter through the coordinator thread; workers should not depend on an app injecting turns directly into a subagent thread.

## Authority

- The user may override any assignment or scope.
- The coordinator authorizes tickets, branches, scope, dependencies, and merge order.
- A peer message does **not** authorize a new ticket, branch, scope expansion, merge, or `main` mutation.
- A worker may not assign work to another worker with a follow-up task. Requests for new work go through the coordinator.
- Use the Codex direct-message primitive exposed by the current runtime for message delivery. Use a follow-up-task primitive only when the coordinator intentionally assigns a new turn or changes an existing assignment.

## Transport Versus Record

| Channel | Use |
| --- | --- |
| Codex direct message | Immediate delivery of dispatches, blockers, public-surface notices, conflicts, and readiness. |
| Git branch/commit | Exact implementation state and durable code/document changes. |
| `coordinator.md` | Current authoritative assignment and merge state. |
| `decisions.md` | Durable architecture or process decisions. |
| `merge-log.md` | Durable integrated merge result. |
| file inbox | Emergency fallback only when direct messaging is unavailable. |

Do not copy the same message into multiple files. A durable decision is summarized once after it is resolved.

## The Peer-Message Gate

A non-coordinator worker may message another worker only when all conditions are true:

1. The sender is within an active coordinator dispatch.
2. The sender will change, or has discovered a conflict in, a public surface.
3. The recipient owns, watches, consumes, or may concurrently change that surface in an active lane.
4. The message can change the recipient's implementation decision or prevent invalidation.

If any condition is false, do not send a peer message.

Examples that do **not** justify a peer message:

- private `.cpp` refactoring
- a new private helper
- test progress or test success
- merge readiness
- current branch/status
- being parked or idle
- a file lock in another build tree
- a request for general review
- a speculative future concern with no active consumer

Send coordinator-facing messages for blockers, scope changes, and readiness.

## What Counts as a Public Surface

A public surface is a contract whose change can invalidate another active lane without that lane editing the same implementation file.

### Code and build surfaces

- exported/public headers, types, signatures, enum values, ownership rules, or invariants
- CMake target names, link dependencies, generated outputs, and preset semantics consumed by another lane
- plugin/registry names and generated interfaces

### Data and process surfaces

- JSON schema fields, defaults, requiredness, identifiers, or validation semantics
- scenario and golden trace event shape, ordering, or meaning
- CLI flags, exit codes, structured diagnostics, and result-file keys
- network packet bytes, magic, version, packet kinds, and compatibility behavior
- shared fixture formats and checked-in generated artifacts
- control-profile binding names or movement-tuning contract

### Project examples

- `ScenarioTrace` fields are a public surface for visual-lab consumers.
- Golden event order and damage/reaction fields are a public semantic surface.
- `visualLabPlayback`, playback diagnostic keys, and scenario evidence keys are a result-contract surface.
- `state_protocol` magic/version/packet kinds are a wire surface.
- A local implementation detail inside `visual_lab.cpp` is not a public surface unless it changes one of those contracts.

## Surface Registry

Every dispatch includes:

```yaml
surfaces:
  owns:
    - scenario-trace-v1
  may_change:
    - combat-result-diagnostics-v1
  watches:
    - character-definition-v1
```

The coordinator derives recipients from active `owns`, `may_change`, and `watches` entries. Workers do not broadcast.

A file can implement several surfaces, and a surface can span several files. Coordinate the surface, not merely the filename.

## Change Classes

### Class A: Additive and compatible

Examples:

- add an optional diagnostic key
- add a new function without changing existing behavior
- add a CMake target without renaming or relinking a target another lane uses

Action:

1. Send `SURFACE_NOTICE` to affected active consumers.
2. Proceed immediately.
3. No ACK is expected.
4. Send `SURFACE_COMMIT` after push if the recipient needs the exact commit.

### Class B: Behavioral but compatibility-preserving

Examples:

- change validation timing while retaining accepted input and output shape
- change default behavior with an explicit compatibility path
- extend trace semantics in a way that affects summaries

Action:

1. Send `SURFACE_PROPOSAL` before publishing the mutation.
2. Continue private implementation or tests; do not idle.
3. Recipient responds only with `SURFACE_CONFLICT` or a concrete adaptation constraint.
4. If no conflict is reported and the change remains compatible, publish it and send `SURFACE_COMMIT`.
5. Escalate to the coordinator if the two lanes require incompatible semantics.

### Class C: Breaking or destructive

Examples:

- rename/remove a field, type, target, CLI flag, result key, packet kind, or event
- change serialized bytes or protocol version
- reorder golden events or reinterpret an existing field
- transfer ownership between subsystems

Action:

1. Send `SCOPE_REQUEST` to the coordinator before committing the public mutation.
2. Include affected active lanes and a compatibility/migration plan.
3. The coordinator chooses ordering, a shim, a shared prerequisite ticket, or a hold.
4. Notify peers only after coordinator direction.
5. Do not use peer silence as approval.

## Message Types

### Coordinator to worker

- `DISPATCH`: authorizes a lane.
- `SCOPE_UPDATE`: modifies authorized scope or surfaces.
- `HOLD`: pauses a specific action; private safe work may continue if stated.
- `REPAIR`: requests changes after integration/review.
- `MERGED`: reports publication and new baseline.
- `CANCEL`: revokes the lane.

### Worker to coordinator

- `STARTED`: confirms dispatch and exact working state.
- `BLOCKED`: requires coordinator action.
- `SCOPE_REQUEST`: asks to expand or break a public surface.
- `READY`: immutable merge candidate and evidence.
- `FAILED`: lane cannot satisfy its contract without redesign.

### Worker to worker

- `SURFACE_NOTICE`: additive compatible change; no reply expected.
- `SURFACE_PROPOSAL`: behavior-affecting compatible proposal.
- `SURFACE_CONFLICT`: concrete incompatibility or simultaneous mutation.
- `SURFACE_COMMIT`: exact pushed commit containing an agreed/notified change.

No other peer message type is part of the normal protocol.

## Message Envelope

Keep messages compact and machine-skimmable. Link to commits or files instead of pasting logs.

```text
TYPE: SURFACE_PROPOSAL
from: Lara/SP2-003
to: Aetoun/SP2-004
surface: scenario-trace-v1
base: e833bc3
change: add optional stunTicks summary input; existing fields/order unchanged
compatibility: Class B; old traces remain valid
needed: reply only with a concrete conflict before your next surface commit
```

Maximum recommended payload: 10 lines plus one short code/signature block when essential.

## Required Payloads

### `STARTED`

```text
TYPE: STARTED
ticket: SP2-004
branch: sprint02/sp2-004-scenario-playback-controls
base: 79cca54
worktree: <path>
build-root: build/agents/aetoun/msvc-debug
surfaces: owns visual-lab-playback-v1; watches scenario-trace-v1
status: tracked clean; unrelated assets untouched
```

A valid `STARTED` needs no coordinator ACK. The coordinator replies only to correct or hold it.

### `BLOCKED`

```text
TYPE: BLOCKED
ticket: SP2-004
blocker: scenario-trace-v1 field semantics are ambiguous
impact: cannot finalize evidence summary; playback core can continue
need: choose current semantics or order SP2-003 first
```

### `READY`

```text
TYPE: READY
ticket: SP2-004
head: 3217619
base: 79cca54
scope: visual_lab core, viewer diagnostics, focused tests, ticket/result docs
surfaces: adds visual-lab-playback-v1; consumes scenario-trace-v1 unchanged
tests: visual_lab 3/3; viewer 5/5; combat 13/13
status: tracked clean; assets untouched
risk: full timeline rendering remains out of scope
```

After `READY`, the branch is frozen except for a coordinator-requested repair.

### `SURFACE_NOTICE`

```text
TYPE: SURFACE_NOTICE
from: Aetoun/SP2-004
to: Lara/SP2-005
surface: visual-lab-result-v1
change: adds optional playbackDurationTicks diagnostic
compatibility: Class A; no existing key changed
reply: none expected
```

### `SURFACE_CONFLICT`

```text
TYPE: SURFACE_CONFLICT
from: Lara/SP2-005
to: Aetoun/SP2-004
surface: result-json-versioning-v1
conflict: my lane snapshots the full diagnostics key set as exact output
minimum-resolution: mark new key optional or bump networkContractVersion
escalation: coordinator if neither option fits dispatch scope
```

## Reply Rules

- Do not send "ACK", "seen", "looks good", or status-only replies.
- Reply to `SURFACE_NOTICE` only when there is a conflict.
- Reply to `SURFACE_PROPOSAL` only with a concrete constraint, conflict, or required adaptation.
- One conflict message should contain the minimum resolution needed.
- The sender resolves and sends `SURFACE_COMMIT`; do not maintain a conversational thread after resolution.
- If resolution changes ticket scope or merge order, notify the coordinator immediately.

## No Wall-Clock Waiting

Direct delivery removes the four-minute claim window.

- Class A proceeds immediately.
- Class B sender continues private work while awaiting any conflict response; only publication of the contested surface is held.
- Class C is held for coordinator action, not peer consensus.
- A delivery failure or closed agent is reported to the coordinator; workers do not poll files for a response.

## Durable Recording

Record only outcomes:

- New cross-ticket semantic decision -> one entry in `decisions.md`.
- Integrated branch -> one entry in `merge-log.md`.
- Current assignment/state -> replace the relevant snapshot in `coordinator.md` and `status/<agent>.md`.
- Routine messages and non-conflicts are not copied to disk.

## Fallback When Direct Messaging Is Unavailable

1. Write one compact message to `inbox/<recipient>.md`.
2. Send the coordinator a `BLOCKED` notice through the available channel.
3. Do not start a four-minute polling loop.
4. Continue private safe work.
5. The coordinator either routes the message, reassigns, or serializes the surface.

The file inbox is a degraded-mode transport, not the normal protocol.

## Anti-Patterns

- all-agent clean-status ACK requests
- broadcast claims for every hot file
- peer requests to start a new ticket
- repeated "still parked" or "no change" messages
- posting the same readiness data to status, inbox, claim, decision, and merge files
- waiting idle for an additive surface notice
- treating silence as approval for a breaking change
- sending full build logs instead of a test summary and artifact/commit reference
