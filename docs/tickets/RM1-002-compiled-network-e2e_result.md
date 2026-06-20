# RM1-002 Result: Compiled Network E2E Runner

Status: ready for merge
Branch: `mitigation01/rm1-002-compiled-network-e2e`
Start commit: `9f06853`
Implementation commit after RM1-003 rebase: `d6d2a20`

## What Changed

- Added `network_e2e_runner`, a compiled C++ process test harness for the UDP snapshot relay path.
- Replaced the canonical `network_e2e` CTest command so it invokes `network_e2e_runner.exe` directly instead of `powershell` and `tools/test-network-e2e.ps1`.
- Kept `network_e2e_client` and the PowerShell script available as optional manual/process-boundary helpers.
- Added structured result JSON for successful runs and parse/failure paths.
- Added README guidance for the compiled runner and the focused network/e2e presets.

## Behavior Now Covered

- The canonical E2E test starts an in-process UDP relay loop around `SnapshotRelay`.
- Client slots use real `SnapshotClient` instances and loopback UDP sockets.
- The runner verifies connect fanout, snapshot fanout, relay-emitted disconnect fanout, and final relay drain to zero connected clients.
- Invalid client-count and unknown-option paths return nonzero and write structured diagnostics when `--result-file` is supplied.
- CTest labels remain `network;e2e`, so existing `msvc-debug-network` and `msvc-debug-e2e` presets continue to select the test.

## Result Artifact Shape

The successful result file includes:

```json
{
  "host": "network_e2e_runner",
  "status": "ok",
  "clientCount": 2,
  "snapshotsPerClient": 96,
  "relay": {
    "acceptedConnects": 2,
    "acceptedDisconnects": 2,
    "acceptedSnapshots": 192,
    "finalClientCount": 0
  },
  "clients": [
    {"clientId": 1, "remoteSnapshots": 94},
    {"clientId": 2, "remoteSnapshots": 96}
  ]
}
```

The exact per-client remote snapshot counts can vary slightly with loopback timing, but the runner requires each client to observe at least the configured minimum and requires relay cleanup to complete.

## Verification

- Rebased cleanly onto `a8b932d Merge RM1-003 cross-asset validation`.
- `cmake --build --preset msvc-debug --target network_e2e_runner` passed after the rebase.
- `ctest --test-dir build/msvc-debug -R "network_e2e" --output-on-failure` passed: 1/1.
- `ctest --preset msvc-debug-network --output-on-failure` passed: 4/4.
- `ctest --preset msvc-debug-e2e --output-on-failure` passed: 1/1.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug --output-on-failure` passed: 26/26.
- `network_e2e_runner --client-count 1 --result-file build/msvc-debug/test-artifacts/network_e2e_invalid_client_count.json --headless` returned exit 1 and wrote `process_error`.
- `network_e2e_runner --unknown-option --result-file build/msvc-debug/test-artifacts/network_e2e_unknown_option.json --headless` returned exit 1 and wrote `process_error`.
- `build/msvc-debug/CTestTestfile.cmake` shows `network_e2e` invoking `network_e2e_runner.exe` directly; no PowerShell command remains in the canonical test entry.
- MSVC runtime/assertion dialog check found no matching visible dialogs.

## Residual Risks

- This removes PowerShell from the canonical CTest path, but it does not prove multi-process server/client launch behavior. The retained PowerShell/manual tools still cover ad hoc process-boundary testing.
- The runner uses a fixed default loopback port. It supports `--port` for collision debugging, but a future harness could allocate an OS port dynamically if `UdpSocket` exposes the bound endpoint.
- The client orchestration is intentionally deterministic rather than a full concurrent soak test. Client-observed disconnect events remain in the result JSON for debugging, but the hard gate uses relay-emitted disconnect events plus final relay drain because individual client-side receipt can race teardown. Future network tickets should add longer-running compatibility/version tests when protocol stability becomes a product requirement.

## Merge State

Vera's RM1-003 merge landed on `main` at `a8b932d`. RM1-002 has been rebased and reverified on top of that merge and is ready for a serialized merge request.
