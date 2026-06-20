# Golden Trace Update Workflow

Scenario golden traces are checked-in semantic combat evidence. Normal build and CTest runs must compare them only; updates require an explicit local command, an explicit environment gate, and a ticket/result note explaining the drift.

## Normal Verification

Normal verification does not rewrite goldens:

```text
ctest --preset msvc-debug-combat --output-on-failure
ctest --preset msvc-debug --output-on-failure
```

For a single scenario, run the scenario runner without update flags:

```text
combat_scenario_runner --scenario tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json --headless
```

If the trace differs, the runner exits nonzero and reports a `golden_mismatch` diagnostic with tick context plus expected and actual event JSON. Damage/reaction fields such as `damage`, `targetRemainingHealth`, `reactionMove`, `hitstopTicks`, and `stunTicks` must remain visible in that diagnostic.

## Intentional Update

Golden updates require both controls:

1. The command-line option `--update-golden`.
2. The environment variable `VAC_UPDATE_GOLDENS=1`.

Example command:

```text
VAC_UPDATE_GOLDENS=1 combat_scenario_runner --scenario tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json --update-golden --headless
```

On PowerShell, set the environment variable for the current shell first:

```powershell
$env:VAC_UPDATE_GOLDENS = "1"
.\build\msvc-debug\combat_scenario_runner.exe --scenario tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json --update-golden --headless
Remove-Item Env:VAC_UPDATE_GOLDENS
```

Do not leave `VAC_UPDATE_GOLDENS` set while running broad test presets. Clear it before normal verification.

## Review Checklist

Every ticket that changes checked-in scenario goldens must record:

- the scenario id and golden file path for each changed golden
- the semantic reason for the drift
- whether event count, event order, effect fields, or final state hash changed
- representative expected/actual diagnostic fields if a mismatch drove the update
- the exact guarded update command or helper used
- focused scenario verification after clearing `VAC_UPDATE_GOLDENS`
- full preset verification before merge

If no goldens changed, the result doc must say so explicitly.

## Result Doc Contract

Use a dedicated `Intentional Golden Drift` section in ticket result docs.

For each changed golden, include one bullet per scenario. Prefer concrete combat language over implementation shorthand:

```text
- `sword_light_hits_idle_target.golden.json`: hit event now records 12 damage, 88 remaining health, `move.hit_reaction`, one tick of hitstop, and three ticks of stun.
```

For unchanged goldens:

```text
## Intentional Golden Drift

- None. Existing scenario goldens still match after this ticket.
```

## Ownership

- `combat_scenario_core` owns golden comparison and guarded update mechanics.
- Tickets and result docs own human review rationale.
- CTest owns preventing accidental drift in normal runs.
- Viewer, visual lab, and network code consume scenario/golden evidence; they do not define golden acceptance.
