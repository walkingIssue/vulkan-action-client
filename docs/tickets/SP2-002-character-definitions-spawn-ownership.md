# SP2-002: Character Definition and Spawn Ownership v1

Status: ready for merge
Branch: sprint02/sp2-002-character-definitions-spawn-ownership
Start commit: `dc260f0`
Source plan: `docs/sprint-02-implementation-plan.md`
Source result docs:
- `docs/tickets/SP1-004-authoring-scene-v0_result.md`
- `docs/tickets/SP1-006-move-schema-compiler_result.md`
- `docs/tickets/SP1-010-scenario-runner-goldens_result.md`
- `docs/tickets/RM1-003-cross-asset-validation_result.md`

## Goal

Introduce minimal checked-in character definitions so actor defaults, default hurtboxes, default moves, and spawn ownership can move out of scenario-local bridge fields and into validated content.

## Non-Goals

- Do not build a full asset registry, cook pipeline, equipment system, or character editor.
- Do not depend on ignored local `assets/` or imported skeletal assets.
- Do not introduce GPU skinning, skeleton clips, animation retargeting, or socket hierarchy ownership.
- Do not add combat networking.
- Do not rewrite map authoring or scenario schema beyond the minimum needed to reference character definitions.

## Current Baseline

- Maps own spawn points, but scenarios still author actor placement and hurtboxes directly.
- Move assets and proxy animation assets are tracked JSON fixtures.
- `RM1-003` validates scenario graph references across maps, moves, proxy animations, and goldens.
- `SP2-001` is expected to leave provisional scenario-owned health/resource/effect bridge fields.

## Data Flow

```text
character JSON
  -> content validation
  -> scenario actor references character id + spawn id
  -> runtime actor defaults from character definition
  -> scenario trace/hash/golden output
```

Ownership boundaries:

- Character definitions own durable defaults.
- Scenarios own test setup, input scripts, and overrides needed for regression cases.
- Maps own spawn locations and facing.
- Move and proxy animation assets keep their current owners.

## Implementation Plan

1. Add a minimal character definition schema under a tracked content or fixture path, such as `content/characters`.
2. Include stable logical id, display/debug label, default health, default hurtbox or hurtbox reference, and default move ids.
3. Validate missing character ids, duplicate ids, missing move references, invalid default health, and invalid default hurtbox data.
4. Let scenario actors reference a character id and spawn ownership instead of duplicating all defaults inline.
5. Keep narrow scenario overrides for tests that intentionally vary defaults.
6. Update existing scenario fixtures to use the minimal character definition where practical.
7. Add negative fixtures proving invalid character definitions fail before simulation.
8. Document which provisional `SP2-001` bridge fields are replaced and which remain.

## Test Plan

- Unit:
  - Character definition load/validate tests.
  - Negative fixtures for duplicate ids, missing moves, invalid health, invalid hurtboxes, and unknown character references.
- Integration:
  - Scenario validation fails before simulation when a character definition is missing or invalid.
  - Existing combat scenarios still match goldens after migration.
- Process/CTest:
  - `ctest --preset msvc-debug-content --output-on-failure`.
  - `ctest --preset msvc-debug-combat --output-on-failure`.
  - `cmake --build --preset msvc-debug`.
  - `ctest --preset msvc-debug --output-on-failure`.
  - MSVC runtime/assertion dialog check.

## Acceptance Criteria

- At least one checked-in character definition drives scenario actor defaults.
- Missing or invalid character definitions produce stable diagnostics with useful field paths.
- Scenario-local duplicated defaults are reduced or clearly marked as test overrides.
- No untracked asset dependency is introduced.
- Existing scenario/golden behavior remains deterministic or intentional drift is documented.

## Risks and Watchpoints

- Avoid making the schema too broad before editor/cook ownership is designed.
- Keep character definitions compatible with later skeletal/imported assets without requiring them now.
- Serialize scenario fixture and golden changes with `SP2-001` and `SP2-003`.

## Progress Log

- 2026-06-20: Planned by `SP2-PLAN`. Implementation not yet dispatched.
- 2026-06-20: Implemented by Vera from `origin/main` `dc260f0` after Mia fallback dispatch. Added tracked character definitions, migrated scenario actors to character refs/spawn-owned teams, and added character validation coverage.

## Verification Results

- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug --target combat_scenario_tests` passed.
- `. .\tools\dev-shell.ps1; .\build\msvc-debug\combat_scenario_tests.exe` passed.
- `. .\tools\dev-shell.ps1; ctest --test-dir build/msvc-debug -R combat_scenario_tests --output-on-failure` passed: 1/1.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-content --output-on-failure` passed: 4/4.
- `. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug` passed with existing `std::getenv` deprecation and Xinput import warnings.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat --output-on-failure` passed: 13/13 after rebuilding stale runner/viewer executables.
- `. .\tools\dev-shell.ps1; ctest --preset msvc-debug --output-on-failure` passed: 27/27.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

## Final Commits

Pending final branch commit.
