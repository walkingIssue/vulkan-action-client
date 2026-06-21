# Agent Instructions

Read before repository work:

- `docs/agent-ticket-workflow-v2.md`
- `docs/agent-communication-protocol-v2.md`
- the coordinator dispatch for your ticket
- the ticket plan and cited design/result documents

## Authority

- Mia is the coordinator/dispatcher unless the user explicitly overrides.
- Work only on the exact dispatched ticket, branch, base SHA, scope, and public surfaces.
- Do not self-assign, switch tickets, expand scope, or mutate `main`.
- Peer messages never authorize work.

## Concurrency

- Use your assigned checkout/worktree and isolated build root.
- Do not branch-switch or build in another worker's checkout/output directory.
- Independent lanes may run concurrently; do not assume another active worker requires you to stop.

## Peer Communication

Do not message another worker during normal private implementation.

Message an affected active worker only when you will change or have found a conflict in a public surface that the worker consumes or may concurrently change. Use only:

- `SURFACE_NOTICE`
- `SURFACE_PROPOSAL`
- `SURFACE_CONFLICT`
- `SURFACE_COMMIT`

No peer ACKs, status messages, test-progress messages, merge-readiness messages, parked-state messages, or general review requests.

Breaking/destructive public changes require `SCOPE_REQUEST` to Mia before commit.

## State Events

Send Mia:

- one `STARTED` after validating dispatch
- `BLOCKED` only when Mia must act
- `SCOPE_REQUEST` before widening scope or breaking a public surface
- one `READY` with exact head, scope, surface delta, tests, clean status, and risks

A valid `STARTED` needs no ACK. After `READY`, freeze the branch unless Mia sends `REPAIR`.

## Git

- Ticket plans are committed to `main` by Mia, normally in a batched start commit.
- Work and commit logical slices only on your feature branch.
- Rebase only your own private branch and only when directed or before freezing the ready candidate.
- Use `--force-with-lease` only on your own feature branch; never force-push `main`.
- Do not merge to `main` without an exact merge lease.
- Never stage unrelated `assets/` or user changes.

## Verification

Run targeted tests first, then the affected subsystem/contract presets required by dispatch. Mia owns integrated full-suite verification unless the dispatch marks the ticket high-risk.

Clear `VAC_UPDATE_GOLDENS` for normal verification. Follow the guarded golden workflow for intentional updates.

## Records

Use direct messages for live coordination. Do not duplicate them into status, inbox, claim, decision, and merge files.

- Current status files contain one replaceable recovery snapshot.
- `decisions.md` records resolved cross-ticket decisions only.
- `merge-log.md` records integrated results only.
- File inboxes are fallback transport only.
