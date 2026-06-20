# RM1-002: Compiled Network E2E Runner

Status: ready for merge
Branch: mitigation01/rm1-002-compiled-network-e2e
Start commit: `9f06853`
Source plan: docs/sprint-01-risk-mitigation-plan.md
Source result docs:
- docs/tickets/SP1-001-cmake-ctest-spine_result.md
- docs/tickets/SP1-002-host-cli-results_result.md
- docs/tickets/SP1-003-characterization_result.md

## Goal

Replace the canonical PowerShell-backed `network_e2e` CTest path with a compiled runner that exercises the UDP snapshot client/relay behavior and emits structured diagnostics.

## Non-Goals

- Do not add authoritative combat networking, prediction, rollback, reconciliation, matchmaking, auth, or NAT traversal.
- Do not change packet layout, protocol version, or viewer network semantics.
- Do not remove manual PowerShell launch scripts unless the CTest path no longer depends on them.
- Do not touch RM1-003 scenario validation files or RM1-004 visual-lab scope.

## Current Baseline

- `network_protocol_tests` and `snapshot_relay_tests` cover packet encoding and relay state in-process.
- `network_e2e` currently runs `powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-network-e2e.ps1`.
- The script starts `udp_state_server.exe`, starts two `network_e2e_client.exe` processes, scrapes stdout/logs, and force-stops the server in cleanup.
- `network_e2e_client.cpp` already uses the real `SnapshotClient` and UDP path, but the orchestration and oracle live in PowerShell.
- `host_core` provides common option parsing and structured result-file output for process tools.

## Data Flow

```text
CTest network_e2e
  -> network_e2e_runner executable
  -> host_core options and result-file path
  -> local UDP relay socket + SnapshotRelay loop
  -> client worker SnapshotClient instances
  -> deterministic connect/snapshot/disconnect packets
  -> per-client observations and relay final state
  -> structured result JSON + process exit code
```

Ownership boundaries:

- `network_core` owns packet encoding, UDP sockets, `SnapshotClient`, and `SnapshotRelay`.
- `network_e2e_runner` owns deterministic test orchestration and structured diagnostics.
- CTest owns the canonical process test entrypoint.
- Manual scripts remain optional developer helpers, not canonical regression dependencies.

## Implementation Plan

1. Add `network_e2e_runner` under `tests/` and link it with `network_core` and `host_core`.
2. Parse common host options, especially `--result-file`, plus runner-specific options for client count, snapshot count, port, timeout, and minimum remote snapshots.
3. Start a local nonblocking UDP relay loop in-process using `UdpSocket` and `SnapshotRelay`.
4. Start deterministic client workers using `SnapshotClient`.
5. Record remote snapshot counts and peer connect/disconnect events per client.
6. Fail with clear diagnostics if clients do not observe enough remote snapshots, if disconnects do not drain the relay, or if invalid CLI options are supplied.
7. Replace the `network_e2e` CTest command so it invokes `network_e2e_runner` directly with a result file.
8. Keep `network_e2e_client` and the PowerShell script available as optional manual/process-boundary helpers.

## Test Plan

- Build:
  - `cmake --build --preset msvc-debug --target network_e2e_runner`
- Focused:
  - `ctest --test-dir build/msvc-debug -R "network_e2e" --output-on-failure`
  - `ctest --preset msvc-debug-network --output-on-failure`
  - `ctest --preset msvc-debug-e2e --output-on-failure`
- Broader:
  - `cmake --build --preset msvc-debug`
  - `ctest --preset msvc-debug --output-on-failure`
- Failure-path/manual:
  - Run `network_e2e_runner --client-count 1` and confirm it exits nonzero with a result diagnostic.
  - Run an unknown option and confirm host CLI rejection reaches stderr/nonzero.
  - Check for MSVC runtime/assertion dialogs if any native process appears to hang or abort.

## Acceptance Criteria

- The canonical CTest `network_e2e` command no longer invokes `powershell`.
- `network_e2e_runner` writes a structured result file with client count, snapshot count, per-client remote snapshot counts, connect/disconnect event counts, final relay client count, elapsed milliseconds, and diagnostics.
- Normal two-client E2E passes through CTest and exercises real UDP sockets plus `SnapshotClient`/`SnapshotRelay`.
- Invalid or impossible runner options return nonzero and produce useful diagnostics when `--result-file` is provided.
- `ctest --preset msvc-debug-network` and `ctest --preset msvc-debug-e2e` pass.
- No `assets/` files or unrelated worktree changes are staged.

## Risks and Watchpoints

- UDP loopback timing can be flaky if the runner relies on fixed sleeps only; use deadlines and drain loops.
- Reusing a fixed port can collide with a manually running relay; support `--port` for debugging and use an uncommon default.
- `SnapshotClient` sends disconnect packets in its destructor, so client lifetime/shutdown order must be explicit.
- Keep CMake edits focused to the new runner target and `network_e2e` test command.
- If RM1-003 merges while this branch is active, rebase before final verification.

## Progress Log

- 2026-06-20: Started after RM1-001 merged and Mia released RM1-002.
- 2026-06-20: Added compiled `network_e2e_runner`, replaced the canonical CTest command, and kept the old PowerShell script as an optional manual helper.
- 2026-06-20: Focused, preset, full debug, negative CLI, and MSVC runtime-dialog checks passed. Final merge waited for Vera's RM1-003 merge window to close.
- 2026-06-20: Rebasing onto `a8b932d Merge RM1-003 cross-asset validation` was clean; focused, network, e2e, full debug, and negative CLI checks passed again.

## Verification Results

- After rebasing onto `a8b932d`, `cmake --build --preset msvc-debug --target network_e2e_runner` passed.
- `ctest --test-dir build/msvc-debug -R "network_e2e" --output-on-failure` passed: 1/1.
- `ctest --preset msvc-debug-network --output-on-failure` passed: 4/4.
- `ctest --preset msvc-debug-e2e --output-on-failure` passed: 1/1.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug --output-on-failure` passed: 26/26.
- `network_e2e_runner --client-count 1 --result-file ... --headless` returned exit 1 and wrote a structured `process_error` diagnostic.
- `network_e2e_runner --unknown-option --result-file ... --headless` returned exit 1 and wrote a structured `process_error` diagnostic.
- Generated `build/msvc-debug/CTestTestfile.cmake` registers `network_e2e` as a direct `network_e2e_runner.exe` command and no longer invokes PowerShell for that test.
- MSVC runtime/assertion dialog check found no matching visible windows.

## Final Commits

- `d6d2a20` Add compiled network E2E runner
