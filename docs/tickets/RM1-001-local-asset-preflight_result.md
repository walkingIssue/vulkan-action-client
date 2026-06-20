# RM1-001 Result: Local Asset Preflight and Fixture Guidance

Status: complete
Branch: `mitigation01/rm1-001-local-asset-preflight`
Start commit: `48d2f74`
Implementation commit: `3f014d0`

## What Changed

- Added `asset_fixture_check`, a small host executable that checks the ignored local paladin OBJ, MTL, and texture directory before legacy viewer/characterization tests run.
- Reused `host_core` common option handling and structured result-file output.
- Added a `--root` override so the missing-asset path is covered without moving local assets.
- Added CTest coverage for both the present-assets path and the intentional missing-root failure path.
- Added README guidance with the direct preflight command and clarified that `visual_lab_smoke` does not require imported paladin assets.

## Behavior Now Covered

- Agents can run a focused local preflight before full viewer/characterization suites.
- Missing local asset fixtures produce nonzero process exit behavior plus structured diagnostics.
- Result JSON includes `status`, `host`, `message`, and present/missing asset diagnostics.
- The missing-root CTest proves the failure path without requiring destructive local asset changes.

## Verification

- `cmake --build --preset msvc-debug --target asset_fixture_check` passed after rebasing onto `origin/main` at `cab1da0`.
- `ctest --test-dir build/msvc-debug -R "asset_fixture_check" --output-on-failure` passed: 2/2 tests.
- `cmake --build --preset msvc-debug` passed.
- `ctest --preset msvc-debug-viewer --output-on-failure` passed: 4/4 tests.
- `ctest --preset msvc-debug --output-on-failure` passed: 26/26 tests.

The successful preflight artifact reported:

```json
{
  "host": "asset_fixture_check",
  "message": "All local asset fixtures are present",
  "status": "ok"
}
```

The negative preflight artifact reported:

```json
{
  "host": "asset_fixture_check",
  "message": "Missing local asset fixtures required by legacy viewer/characterization tests",
  "status": "error"
}
```

## Residual Risks

- This ticket makes the local asset dependency explicit and fast to diagnose, but it does not make full debug CTest asset-independent from a clean checkout.
- The broader mitigation-plan goal of replacing automated viewer/characterization dependencies with tracked miniature fixtures still needs a follow-up slice if that remains the desired gate.
- The remediation note names the current local machine copy source; this is useful for this multi-agent setup but should not become a long-term distribution mechanism.

## Next Recommended Ticket

Continue the coordinator-dispatched mitigation lanes. RM1-003 can proceed in parallel, while a future asset-fixture replacement slice should address the remaining clean-checkout requirement.
