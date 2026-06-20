# SP1-002: Common Host CLI and Result Files

Status: planned
Branch: sprint01/sp1-002-host-cli-results
Start commit: this ticket-start commit on `main`
Source plan: `docs/sprint-01-implementation-plan.md`
Source design sections:
- `docs/action_combat_engine_editor_design.md` section 18.5: Required executable test switches
- `docs/action_combat_engine_editor_design.md` section 19.3: Test support library
- `docs/action_combat_engine_editor_design.md` section 19.4: Determinism rules for tests
- `docs/action_combat_engine_editor_design.md` section 27: Recommended First Engineering Tasks

## Goal

Give project executables a shared, testable command-line and result-file foundation so agents, CTest, and future process runners can automate hosts without scraping logs or relying on one-off flags.

## Non-Goals

- Do not build the full process orchestrator.
- Do not replace the network E2E PowerShell runner in this ticket.
- Do not add editor command scripts or combat scenario scripts yet.
- Do not force all existing app-specific flags into a perfect final schema.
- Do not change runtime behavior beyond parsing/reporting and deterministic exit artifacts.

## Current Baseline

- `vulkan_scene_viewer` already supports `--frames`, `--scene`, `--control-profile`, `--orbit-camera`, `--static-camera`, `--net-server`, and `--net-client-id` through a local parser.
- `udp_state_server` has its own parser for bind/dump options.
- `scene_probe` has a tiny local `--scene` parser.
- Test executables currently report through process exit and stdout only.
- There is no standard `--result-file`, `--ticks`, `--headless`, `--hidden-window`, `--seed`, or `--log-file` behavior.

## Data Flow

```text
argv
  -> host_core command-line parser
  -> typed HostOptions plus app-specific options
  -> executable validation
  -> run host/tool
  -> HostResult
  -> optional result JSON file
  -> CTest/process runner consumes exit code and structured result
```

Ownership boundaries:

- `host_core` owns common option parsing, usage text helpers, and result-file serialization.
- Each executable owns its app-specific options and semantic validation.
- Result files are diagnostics and automation artifacts; they do not replace process exit codes.
- No gameplay/runtime library depends on platform windowing or network transport because of this parser.

## Implementation Plan

1. Add a small `host_core` static library.
2. Define `CommonHostOptions` for shared flags: `frames`, `ticks`, `scene`, `offline`, `headless`, `hidden-window`, `result-file`, `input-script`, `command-script`, `seed`, and `log-file`.
3. Define parse helpers that report unknown options and missing values precisely.
4. Define `HostResult` JSON writing with status, message, seed, frames, ticks, and optional diagnostics.
5. Add unit tests for common parsing and result-file output.
6. Integrate the common parser into at least `scene_probe` and `vulkan_scene_viewer`.
7. Add `--result-file` support to `vulkan_scene_viewer --frames N` smoke runs.
8. Update README or ticket docs with supported common switches.

## Test Plan

- Unit:
  - known common options parse into typed fields.
  - unknown option fails with usage text.
  - missing value fails with option name.
  - result JSON contains stable fields.
- Integration/process:
  - `scene_probe --scene <fixture> --result-file <temp>` exits successfully and writes JSON.
  - `vulkan_scene_viewer --frames 3 --result-file <temp>` exits successfully and writes JSON.
  - invalid option on integrated hosts exits nonzero.
- Regression:
  - existing CTest unit/network presets still pass.
  - viewer smoke still runs.

## Acceptance Criteria

- Shared parser and result writer have tests.
- At least two real executables use the shared common options.
- `--result-file` works for a short viewer smoke.
- Unknown common/app options fail loudly with nonzero exit.
- Existing app-specific flags still work.
- No generated result artifacts are committed.

## Risks and Watchpoints

- Avoid turning the parser into a framework; keep it boring and explicit.
- Do not break existing scripts that pass viewer/network options.
- Keep result writing safe: write to a temp file then replace where practical.
- `--headless` and `--hidden-window` may be parsed before they are fully implemented; unsupported combinations must either be ignored only when harmless or fail clearly.
- Viewer result files must still be written on handled exceptions where possible.

## Progress Log

- 2026-06-20: Started after SP1-001 merged.

## Verification Results

Pending.

## Final Commits

Pending.
