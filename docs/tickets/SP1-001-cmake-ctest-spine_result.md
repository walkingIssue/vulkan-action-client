# SP1-001 Result: CMake and CTest Spine

Branch: `sprint01/sp1-001-cmake-ctest-spine`

## Outcome

SP1-001 made CMake presets and CTest presets the canonical build and test path for the project.

The ticket added or hardened:

- `CMakePresets.json` with `msvc-debug` and `msvc-release` configure/build presets.
- CTest presets for full debug, unit, network, e2e, combat, and release test runs.
- CTest labels and timeouts for existing tests.
- README instructions that put CMake/CTest first.
- PowerShell `tools/test.ps1` delegation to `ctest --preset`.
- Dev-shell PATH support for installed Git LFS and LLVM tools.

## Verification

Passed:

```text
cmake --list-presets
ctest --list-presets
. .\tools\dev-shell.ps1; cmake --preset msvc-debug
. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug
. .\tools\dev-shell.ps1; ctest --preset msvc-debug
. .\tools\dev-shell.ps1; ctest --preset msvc-debug-unit
. .\tools\dev-shell.ps1; ctest --preset msvc-debug-network
. .\tools\dev-shell.ps1; ctest --preset msvc-debug-e2e
. .\tools\dev-shell.ps1; ctest --preset msvc-debug-combat
.\tools\test.ps1 -Preset msvc-debug
```

All test runs passed during the ticket.

## Residual Risk

- The current Windows dev flow still benefits from `tools/dev-shell.ps1` to expose MSVC, Vulkan SDK, and local tools in the active shell.
- The network E2E test still invokes its existing PowerShell process runner; replacing that belongs in a later process-runner ticket.
- CMake emitted a known warning about `CMAKE_TOOLCHAIN_FILE` in the already-configured build tree, but configure/build/test completed successfully.

## Next Useful Step

SP1-002 should use the preset foundation to introduce shared host CLI/result-file behavior, so future scenario runners and smoke tests produce structured artifacts instead of relying on log scraping.
