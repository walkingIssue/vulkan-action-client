# SP1-002 Result: Common Host CLI and Result Files

Branch: `sprint01/sp1-002-host-cli-results`

## Outcome

SP1-002 added a small `host_core` library for shared executable automation:

- Common argv parsing.
- Typed common host options.
- Unsupported common-option rejection per executable.
- Structured JSON result-file writing.
- Unit coverage for parser and result-file behavior.

The shared layer is integrated into:

- `scene_probe`
- `vulkan_scene_viewer`

## Supported In This Ticket

- `--scene`
- `--frames`
- `--result-file`

The common parser also recognizes these future-facing flags, but integrated executables reject them until they are explicitly supported:

- `--ticks`
- `--offline`
- `--headless`
- `--hidden-window`
- `--input-script`
- `--command-script`
- `--seed`
- `--log-file`

## Verification

Passed:

```text
. .\tools\dev-shell.ps1; cmake --build --preset msvc-debug
. .\tools\dev-shell.ps1; ctest --preset msvc-debug
.\build\msvc-debug\scene_probe.exe --scene config/scenes/bootstrap.scene.json --result-file build/msvc-debug/test-artifacts/scene_probe_result.json
.\build\msvc-debug\vulkan_scene_viewer.exe --frames 3 --result-file build/msvc-debug/test-artifacts/viewer_result.json
.\build\msvc-debug\scene_probe.exe --bogus
.\build\msvc-debug\vulkan_scene_viewer.exe --bogus
```

The two invalid-option commands failed with nonzero exit as expected.

## Residual Risk

- `--result-file` is now available on `scene_probe` and `vulkan_scene_viewer`, not every executable.
- `--hidden-window` is parsed by the common layer but not implemented by the viewer yet.
- Result files are diagnostic artifacts; process exit codes remain the authoritative pass/fail signal.
- Native Vulkan smoke still opens a real window for now.

## Next Useful Step

SP1-003 should use this result-file foundation while capturing characterization tests for current scene, movement, interpolation, packet, relay, and viewer smoke behavior.
