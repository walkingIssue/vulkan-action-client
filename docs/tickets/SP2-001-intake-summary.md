# SP2-001: Intake Summary

Status: superseded intake guardrail
Branch: not created
Source plan: superseded by `docs/sprint-02-implementation-plan.md`
Last reviewed: 2026-06-20

## Summary

This file is historical. It was created as a stop sign before Sprint 02 existed in the repository.

The actionable Sprint 02 plan now lives in `docs/sprint-02-implementation-plan.md`. The actionable `SP2-001` ticket is now `docs/tickets/SP2-001-combat-effects-damage-reactions.md`.

Do not implement from this intake summary. Keep it only as a guardrail for stale agents or old context.

## Current Correct Work

The currently defined Sprint 02 planning track is:

1. `docs/sprint-02-implementation-plan.md`
2. `docs/tickets/SP2-001-combat-effects-damage-reactions.md`
3. `docs/tickets/SP2-002-character-definitions-spawn-ownership.md`
4. `docs/tickets/SP2-003-golden-governance-update-workflow.md`
5. `docs/tickets/SP2-004-scenario-playback-controls.md`
6. `docs/tickets/SP2-005-network-compatibility-snapshot.md`

If an agent is looking for the next task, it should read `docs/sprint-02-implementation-plan.md`, the current Mia dispatch on the local coordination board, and the relevant planned ticket stub.

Do not infer permission to start implementation from the `SP2-001` label alone. Work still requires Mia dispatch or explicit user override.

## Before Starting SP2-001

Before starting the actionable `SP2-001` combat ticket:

- Read `docs/sprint-02-implementation-plan.md`.
- Read `docs/tickets/SP2-001-combat-effects-damage-reactions.md`.
- Fetch `main` and post checkout path, branch, base commit, remote/fetch evidence, tracked/untracked status, likely hot files, and verification gates to the board.
- Wait for Mia dispatch or explicit user override.
- Follow the normal ticket workflow: commit any start-doc refresh on `main`, branch from that commit, implement, verify, produce a result document, and merge back with `--no-gpg-sign`.
