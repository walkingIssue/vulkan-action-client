# Agent Ticket Workflow

This workflow is for Codex and other agents working on this project. It is designed to keep long engine/editor tasks traceable, testable, and easy to resume.

The core rule from the user:

```text
When starting a task/ticket from the implementation plan, make a commit on main at the start with the detailed plan, test plan, data flow, and feature branch name. Then branch out and attack the ticket.
```

## Golden Loop

```mermaid
flowchart TD
  Intake["Read ticket source\nimplementation plan + design docs"]
  Baseline["Inspect repo state\nstatus, tests, relevant code"]
  TicketDoc["Create ticket doc\nbranch, plan, test plan, data flow"]
  StartCommit["Commit ticket doc on main\n--no-gpg-sign"]
  Branch["Create feature branch"]
  Implement["Implement in small steps"]
  Verify["Run targeted tests\nthen broader preset tests"]
  UpdateDoc["Update ticket doc\nresults, decisions, artifacts"]
  Merge["Merge back to main\nno unrelated files"]
  Final["Report outcome\ncommit ids, tests, residual risk"]

  Intake --> Baseline
  Baseline --> TicketDoc
  TicketDoc --> StartCommit
  StartCommit --> Branch
  Branch --> Implement
  Implement --> Verify
  Verify --> UpdateDoc
  UpdateDoc --> Merge
  Merge --> Final
```

## Branch Naming

Use:

```text
sprint01/sp1-###-short-slug
```

Examples:

```text
sprint01/sp1-001-cmake-ctest-spine
sprint01/sp1-006-move-schema-compiler
sprint01/sp1-010-scenario-runner-goldens
```

Future sprints should use the same pattern:

```text
sprint02/sp2-###-short-slug
```

## Ticket Document Location

Create one ticket document per ticket:

```text
docs/tickets/SP1-###-short-slug.md
```

If `docs/tickets` does not exist, create it in the start commit.

## Start Commit Requirements

Before implementation begins:

1. Be on `main`.
2. Inspect tracked and untracked status.
3. Do not stage unrelated user changes.
4. Create the ticket document.
5. Commit the ticket document to `main`.
6. Create the feature branch from that commit.

Commands:

```powershell
git switch main
git status --short
git add -- docs/tickets/SP1-###-short-slug.md
git commit --no-gpg-sign -m "Start SP1-### short title"
git switch -c sprint01/sp1-###-short-slug
```

The start commit is not busywork. It pins the intended scope before implementation pressure starts bending the work.

## Ticket Document Template

````markdown
# SP1-###: Title

Status: planned
Branch: sprint01/sp1-###-short-slug
Start commit: <hash after commit>
Source plan: docs/sprint-01-implementation-plan.md
Source design sections:
- docs/action_combat_engine_editor_design.md section X

## Goal

One paragraph describing the user-visible outcome and why this ticket exists.

## Non-Goals

- Explicitly out of scope.
- Things that are tempting but should not happen in this ticket.

## Current Baseline

- Relevant files and current behavior.
- Known tests that cover nearby behavior.
- Existing limitations.

## Data Flow

```text
input -> transform -> output -> tests/artifacts
```

Include important ownership boundaries. Example:

```text
Move JSON -> validator -> compiled move table -> combat tick -> event trace -> golden comparison
```

## Implementation Plan

1. Step one.
2. Step two.
3. Step three.

## Test Plan

- Unit:
- Integration:
- Process/CTest:
- Manual/visual if needed:

## Acceptance Criteria

- Concrete pass/fail bullets.

## Risks and Watchpoints

- Determinism risks.
- API ownership risks.
- Renderer/network/editor coupling risks.

## Progress Log

- YYYY-MM-DD: Started.

## Verification Results

Fill this before merging.

## Final Commits

Fill this after implementation.
````

## Implementation Rules

- Keep gameplay correctness headless when possible.
- Renderer, editor UI, networking transport, and platform input are consumers of runtime state, not owners of combat truth.
- No ticket should deepen `vulkan_scene_viewer.cpp` as a permanent engine object unless the ticket is explicitly preserving the legacy viewer.
- Prefer small library targets and explicit dependencies.
- Do not introduce required PowerShell-only behavior.
- Do not stage `assets/` or other unrelated untracked files unless the ticket explicitly owns them.
- Use `apply_patch` for manual edits.
- Use `--no-gpg-sign` on commits.

## Test Selection

Each ticket should run the narrowest meaningful suite first, then a broader suite before merge.

Suggested order:

```text
targeted unit test executable
targeted CTest label
full debug CTest preset
viewer or process smoke if touched
```

Examples:

```powershell
ctest --preset msvc-debug-unit
ctest --preset msvc-debug-integration
ctest --preset msvc-debug
```

Until presets exist, use the current repo scripts as a bridge, but ticket acceptance should move toward CMake/CTest.

## Data Flow Requirement

Every ticket must include a data flow section because this project is easy to accidentally couple:

- Input ownership: file, device, network, or test fixture.
- Transform/compile step: parser, validator, runtime compiler, tick runner, renderer adapter.
- Runtime owner: AuthoringScene, RuntimeWorld, CombatCore, AnimationCore, Renderer, Network.
- Output artifact: event trace, state hash, result file, rendered frame, packet.
- Test oracle: field compare, golden trace, state hash, validation result, visual smoke.

If the data flow cannot be described simply, the ticket is probably too large.

## Commit and Merge Policy

During implementation, commit logical slices on the feature branch.

Commit messages should name behavior:

```text
Add move tick range validation
Add combat scenario result files
Render proxy hitbox debug lines
```

Before merging:

1. Update the ticket document with verification results.
2. Run required tests.
3. Ensure `git status --short` only shows intended changes.
4. Merge back to main.

Preferred merge when the branch is ready:

```powershell
git switch main
git merge --no-gpg-sign --no-ff sprint01/sp1-###-short-slug
```

If the project later prefers fast-forward-only history, update this workflow. Until then, a merge commit keeps ticket boundaries visible.

## Handoff Summary

When stopping or handing off, leave enough context for another agent:

- Current branch.
- Last commit.
- What changed.
- What tests passed.
- What tests failed or were not run.
- Open design decisions.
- Files most relevant to continue.
- Any user changes deliberately left untouched.

## When to Ask the User

Ask before:

- Deleting or moving user-created assets.
- Committing large untracked content that the ticket did not create.
- Changing the sprint scope.
- Choosing between incompatible product semantics, such as cancel rules or combat priority policy.
- Adding heavy dependencies not already accepted by the design.

Do not ask for routine implementation details when the ticket and codebase make a conservative choice clear.

## Definition of Done for an Agent Ticket

A ticket is done when:

- The ticket document exists and has verification results.
- The implementation matches the ticket acceptance criteria.
- Required tests pass through the best available CMake/CTest path.
- New behavior has regression coverage.
- The final response reports commits, tests, artifacts, and residual risk.
- Unrelated user changes are untouched.

If a ticket cannot meet these criteria, mark the ticket document with the blocker and stop before widening scope silently.
