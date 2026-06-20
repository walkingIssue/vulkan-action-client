# SP2-001: Intake Summary

Status: not started
Branch: not created
Source plan: none in repository yet

## Summary

`SP2-001` is not currently defined as an implementation ticket. The only `SP2` reference in the repository is the future-sprint branch naming example in `docs/agent-ticket-workflow.md`.

Agents should not start implementation from `SP2-001` until a Sprint 02 implementation plan exists and names the ticket scope, branch, data flow, test plan, and acceptance criteria.

## Current Correct Work

The current active Sprint 01 sequence is:

1. `SP1-001`: CMake and CTest spine - complete.
2. `SP1-002`: Common host CLI and result files - complete.
3. `SP1-003`: Characterization before extraction - started.

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
