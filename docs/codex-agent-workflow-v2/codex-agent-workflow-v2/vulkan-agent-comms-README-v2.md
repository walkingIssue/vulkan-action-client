# Vulkan Agent Comms v2

This machine-local directory stores recoverable coordination state for agents working on isolated copies/worktrees of `vulkan-action-client`.

Codex direct-agent messaging is the normal transport. This directory is not an inbox-polling system.

## Directory Layout

```text
vulkan-agent-comms/
  README.md
  coordinator.md
  status/
    vera.md
    lara.md
    aetoun.md
    mia.md
  dispatch/
    SP2-004.md
  inbox/
    vera.md
    lara.md
    aetoun.md
    mia.md
  decisions.md
  merge-log.md
  archive/
    2026-06-20-v1/
```

## File Semantics

| File | Semantics |
| --- | --- |
| `coordinator.md` | Mutable authoritative current state. Replace, do not append routine history. |
| `status/<agent>.md` | Mutable recovery snapshot owned by that agent. One current record only. |
| `dispatch/<ticket>.md` | Coordinator-issued assignment contract. Immutable; supersede with a new record/version. |
| `inbox/<agent>.md` | Degraded-mode fallback when direct messaging fails. Not routinely polled. |
| `decisions.md` | Append-only durable cross-ticket decisions. |
| `merge-log.md` | Append-only successful/failed integration results. |
| `archive/` | Old append-only status/inbox/coordinator history outside the live read path. |

## Agent Names

Use stable names: Vera, Lara, Aetoun, and Mia.

## Live Transport

Use Codex direct messages for:

- coordinator dispatch and scope updates
- worker `STARTED`, `BLOCKED`, `SCOPE_REQUEST`, and `READY`
- worker-to-worker public-surface notices/proposals/conflicts/commit references
- merge publication and repair requests

Do not copy routine direct messages to files.

## Peer Communication Gate

Workers message one another only for a public surface that another active lane consumes or may change concurrently. Follow `agent-communication-protocol-v2.md`.

No peer messages for:

- status or ACKs
- test progress
- merge readiness
- parked/idle state
- general review
- private implementation details
- branch polling

## Current Status File

Each agent owns and may replace only its own status file.

```markdown
# Aetoun Current Status

Updated: 2026-06-20 13:32 +02:00
State: active | blocked | ready | idle
Ticket: SP2-004
Branch: sprint02/sp2-004-scenario-playback-controls
Head: 3217619
Base: 79cca54
Worktree: C:\...\vulkan-action-client-aetoun
Build root: build/agents/aetoun/msvc-debug
Surfaces: owns visual-lab-playback-v1; watches scenario-trace-v1
Blocker: none
Unrelated local state: assets/ untouched
```

Update only on state transition or before a deliberate handoff. Do not append "still parked" snapshots.

## Dispatch Record

Coordinator-owned example:

```markdown
# Dispatch SP2-004 v1

Owner: Aetoun
Issued: 2026-06-20 13:18 +02:00
Base/start commit: b08f4ff
Branch: sprint02/sp2-004-scenario-playback-controls
Worktree: C:\...\vulkan-action-client-aetoun
Build root: build/agents/aetoun/msvc-debug
Scope: scenario playback/evidence in visual_lab_core and narrow viewer diagnostics
Non-goals: combat semantics, golden policy, network protocol, full timeline editor
Surfaces:
- owns: visual-lab-playback-v1, visual-lab-result-v1
- may change: none without scope update
- watches: scenario-trace-v1, golden-evidence-v1
Dependencies: SP2-003 merged at b08f4ff
Tests: visual_lab focused; viewer preset; combat consumer preset
Ready payload: exact head, scope, surface delta, tests, status, risks
```

The worker replies through direct messaging with `STARTED`; the dispatch file is not used for ACKs.

## Durable Decisions

Append only resolved cross-ticket decisions:

```markdown
## 2026-06-20
Decision: `scenario-trace-v1` event order remains owned by combat_scenario_core.
Consumers may add summaries but may not reinterpret or reorder events.
Reason: preserve golden comparison semantics.
Affected tickets: SP2-003, SP2-004.
```

Do not record routine holds, status, polling, or ACK closure as decisions.

## Merge Log

One entry per published ticket or failed integration attempt:

```markdown
## 2026-06-20 SP2-004 merged
Head: 3217619
Main: <merge sha>
Surface delta: added visual-lab-playback-v1 diagnostics; scenario-trace-v1 unchanged
Worker tests: visual_lab 3/3; viewer 5/5; combat 13/13
Integration: full debug 27/27; MSVC dialog check clean
Assets/local state: unrelated assets untouched
```

Do not log every push, implementation slice, merge request poll, or approval.

## Fallback Inbox

Use only when direct message delivery is unavailable.

```markdown
## UNDELIVERED 2026-06-20 13:30 from Aetoun to Lara
TYPE: SURFACE_PROPOSAL
surface: scenario-trace-v1
change: add optional summary input; existing trace unchanged
coordinator notified: yes
```

The sender continues safe private work and notifies Mia. There is no fixed four-minute wait and no polling loop.

## Archiving v1 State

Move the current append-only files into a dated archive before enabling v2:

```text
archive/2026-06-20-v1/coordinator.md
archive/2026-06-20-v1/status/*
archive/2026-06-20-v1/inbox/*
archive/2026-06-20-v1/claims/*
```

Then create compact live snapshots. Do not delete the historical evidence.

## Lock Contention

Each worker must use an isolated worktree and build root. A lock in another worker's executable/DLL/PDB should not affect the current lane.

If a lock persists in an allegedly isolated build tree:

1. verify the configured binary/output path
2. identify the owning process
3. correct the shared path or stale process
4. notify the coordinator only if it blocks the lane

Do not establish peer lock-negotiation chatter as a normal workflow.
