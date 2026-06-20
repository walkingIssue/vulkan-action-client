# SP2-005: Network Compatibility Snapshot v1

Status: planned late/stretch
Branch: sprint02/sp2-005-network-compatibility-snapshot
Start commit: TBD by dispatched owner
Source plan: `docs/sprint-02-implementation-plan.md`
Source result docs:
- `docs/tickets/RM1-002-compiled-network-e2e_result.md`
- `docs/tickets/SP1-003-characterization_result.md`

## Goal

Capture and test the current snapshot relay/protocol/result-file contract as a narrow compatibility snapshot, without defining future authoritative combat networking too early.

## Non-Goals

- Do not add authoritative combat commands, combat events, rollback, reconciliation, prediction, reliable channels, command replication, matchmaking, or production session management.
- Do not serialize `SP2-001` damage/reaction semantics over the network in this ticket.
- Do not change the current relay architecture unless needed for diagnostics or compatibility tests.
- Do not make network compatibility a blocker for the core Sprint 02 combat path unless Mia explicitly redispatches it.

## Current Baseline

- `state_protocol` defines packet magic, version, packet kinds, and encoding/decoding behavior.
- `network_protocol_tests` and characterization fixtures cover representative packets.
- `network_e2e_runner` exercises an in-process UDP relay loop with structured result JSON.
- The network path currently carries optimistic actor transforms and connect/disconnect/snapshot fanout, not authoritative combat.

## Data Flow

```text
protocol encode/decode fixtures
  -> snapshot relay/client tests
  -> compiled network_e2e_runner result JSON
  -> documented compatibility snapshot
```

Ownership boundaries:

- Network protocol owns bytes, packet kinds, version constants, relay/client behavior, and result JSON fields.
- Combat tickets own damage/reaction semantics and should not be pulled into this snapshot.

## Implementation Plan

1. Document current protocol magic, version, packet kinds, representative encoded packet bytes, and network result JSON fields.
2. Add or refresh protocol fixture checks if current tests do not explicitly pin enough bytes.
3. Add an unsupported-version or bad-magic negative test if it is missing.
4. Consider adding `protocolVersion` or `networkContractVersion` to the compiled E2E result JSON.
5. Verify `snapshot_relay_tests`, `network_protocol_tests`, and `network_e2e_runner` still pass.
6. Record that authoritative combat networking remains a later sprint.

## Test Plan

- Unit:
  - `network_protocol_tests` for packet byte compatibility and negative parse/version behavior.
  - `snapshot_relay_tests` for current relay fanout and disconnect semantics.
- Process/CTest:
  - `network_e2e_runner` focused process test.
  - `ctest --preset msvc-debug-network --output-on-failure`.
  - `ctest --preset msvc-debug-e2e --output-on-failure`.
  - `cmake --build --preset msvc-debug`.
  - `ctest --preset msvc-debug --output-on-failure`.
  - MSVC runtime/assertion dialog check.

## Acceptance Criteria

- The current packet/version/result contract is documented and covered by tests.
- Negative parse or unsupported-version behavior is tested.
- Network E2E result JSON includes enough contract metadata to compare future artifacts.
- No authoritative combat networking surface is introduced.
- This ticket can be skipped or moved to a later sprint without blocking `SP2-001` through `SP2-004`.

## Risks and Watchpoints

- Freezing protocol details too early can lock in the wrong future surface.
- Keep this ticket late or stretch until combat and character semantics settle.
- Avoid sharing hot files with active combat/scenario tickets.

## Progress Log

- 2026-06-20: Planned as late/stretch by `SP2-PLAN`. Implementation not yet dispatched.

## Verification Results

Fill this before merging.

## Final Commits

Fill this after implementation.
