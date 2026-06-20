# SP2-001: Intake Summary

Status: intake guardrail only
Branch: not created
Source plan: none in repository yet
Last reviewed: 2026-06-20

## Summary

`SP2-001` is not currently defined as an implementation ticket. There is no Sprint 02 implementation plan in the repository, and there are no accepted goals, non-goals, branches, data flows, test plans, or acceptance criteria for a Sprint 02 kickoff.

Agents must not start implementation from `SP2-001` until a Sprint 02 implementation plan exists and names the ticket scope, branch, data flow, test plan, and acceptance criteria.

This file is a stop sign, not a work ticket.

## Current Correct Work

The currently defined implementation track is still Sprint 01. The current safe reading of `main` is:

1. `SP1-001` through `SP1-010` are merged.
2. `SP1-012`: Command-script map wireframing is merged.
3. `SP1-011`: Visual map and combat lab is the active defined implementation lane that still needs completion.

If an agent is looking for the next task, it should use `docs/sprint-01-implementation-plan.md` and the committed ticket/result documents under `docs/tickets/`, not this `SP2-001` placeholder.

Do not infer a Sprint 02 kickoff from the `SP2-001` label. The next actionable work is the unfinished Sprint 01 visual lab track unless a newer committed Sprint 02 plan explicitly supersedes this guardrail.

## Before Starting SP2-001

Create or update a Sprint 02 plan that defines:

- The product outcome for Sprint 02.
- The exact `SP2-001` goal and non-goals.
- The feature branch name.
- Data flow and ownership boundaries.
- A concrete implementation plan.
- Unit, integration, process, and smoke test coverage.
- Acceptance criteria and risks.

Then follow the normal ticket workflow: commit the start document on `main`, branch from that commit, implement, verify, produce a result document, and merge back with `--no-gpg-sign`.
