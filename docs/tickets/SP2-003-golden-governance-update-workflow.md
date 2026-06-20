# SP2-003: Golden Governance and Update Workflow v1

Status: planned
Branch: sprint02/sp2-003-golden-governance-update-workflow
Start commit: TBD by dispatched owner
Source plan: `docs/sprint-02-implementation-plan.md`
Source result docs:
- `docs/tickets/SP1-010-scenario-runner-goldens_result.md`
- `docs/tickets/RM1-003-cross-asset-validation_result.md`
- `docs/tickets/SP2-001-combat-effects-damage-reactions.md`

## Goal

Make scenario golden trace updates explicit, reviewable, and difficult to perform accidentally as combat trace language grows.

## Non-Goals

- Do not invent a screenshot golden system.
- Do not build a full external approval service or CI platform.
- Do not change combat behavior only to satisfy the governance workflow.
- Do not replace readable JSON traces with opaque binary artifacts.

## Current Baseline

- Golden updates are guarded by `--update-golden` plus `VAC_UPDATE_GOLDENS=1`.
- Scenario mismatch diagnostics show expected and actual snippets with tick context.
- Trace schema is about to grow with damage/reaction/effect fields.
- There is no manifest or documented review checklist for intentional golden drift.

## Data Flow

```text
scenario run
  -> actual trace/result JSON
  -> golden comparison
  -> guarded update path
  -> manifest/review metadata
  -> result doc records intentional drift
```

Ownership boundaries:

- Scenario runner owns comparison and update mechanics.
- Ticket/result docs own human review rationale.
- CTest owns preventing accidental drift in normal runs.

## Implementation Plan

1. Document the golden update command flow and required environment gate in repo docs.
2. Add a small manifest or metadata file if it improves review of scenario ids, golden paths, trace schema version, and expected hash fields.
3. Add tests proving normal runs cannot rewrite goldens.
4. Add tests or diagnostics proving mismatches include enough new damage/reaction fields to review drift.
5. Add a negative update test for missing gate or unsupported update mode.
6. Require result docs to list each intentional golden changed by a ticket and why.
7. Keep compatibility with existing checked-in goldens and CTest process coverage.

## Test Plan

- Unit:
  - Golden update gate refuses writes without both explicit command option and environment permission.
  - Manifest or metadata parsing rejects stale/missing scenario entries if a manifest is added.
  - Mismatch diagnostics include new effect fields after `SP2-001`.
- Integration:
  - Scenario tests still compare all checked-in goldens.
  - Intentional mismatch fixture produces readable expected/actual diagnostics.
- Process/CTest:
  - `ctest --preset msvc-debug-combat --output-on-failure`.
  - `cmake --build --preset msvc-debug`.
  - `ctest --preset msvc-debug --output-on-failure`.
  - MSVC runtime/assertion dialog check.

## Acceptance Criteria

- Normal CTest runs cannot update checked-in goldens.
- The documented update workflow names the command, environment gate, review fields, and result-doc expectations.
- Golden mismatch output remains readable after damage/reaction trace growth.
- Any manifest or metadata added is covered by tests and does not hide useful diffs.
- Future agents can identify intentional versus accidental golden drift from docs and result files.

## Risks and Watchpoints

- Do not overbuild governance before trace schema stabilizes.
- Avoid opaque generated metadata that makes reviews harder.
- Coordinate with `SP2-001` if both touch scenario goldens.

## Progress Log

- 2026-06-20: Planned by `SP2-PLAN`. Implementation not yet dispatched.

## Verification Results

Fill this before merging.

## Final Commits

Fill this after implementation.
