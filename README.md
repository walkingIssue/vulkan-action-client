# Vulkan Action Client

Prototype repo for a C++/Vulkan action-combat client.

## Local Toolchain

Detected and prepared:

- Git 2.53.0
- CMake 3.25.2
- Visual Studio 2022 Build Tools / MSVC 19.44
- Ninja 1.13.2
- Vulkan SDK 1.4.350.0 at `C:\VulkanSDK\1.4.350.0`
- Shader tools: `glslc` and `dxc`
- Vulkan runtime working on Intel Arc A770
- vcpkg at `C:\Users\Bartek\Documents\Playground\tools\vcpkg`
- Git LFS 3.7.1
- LLVM 22.1.8 for `clang-format` and `clang-tidy`

Git signing is disabled locally for this repository:

```powershell
git config commit.gpgsign
git config tag.gpgsign
```

Both should print `false`.

## Dev Shell

From this directory, run:

```powershell
.\tools\dev-shell.ps1
```

That activates the MSVC x64 build environment and adds the Vulkan SDK and WinGet tool links to the current PowerShell process.

It also exposes local workspace tools such as `vcpkg`, `git-lfs`, and portable RenderDoc when available.

## Build And Test

The canonical build and test interface is CMake presets plus CTest. Use the dev shell or a Visual Studio developer shell so MSVC and Ninja are visible, then run:

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

Useful test slices:

```powershell
ctest --preset msvc-debug-unit
ctest --preset msvc-debug-network
ctest --preset msvc-debug-e2e
ctest --preset msvc-debug-combat
ctest --preset msvc-debug-content
ctest --preset msvc-debug-characterization
ctest --preset msvc-debug-viewer
```

The project uses vcpkg manifest mode through `CMakePresets.json`; CMake runs vcpkg install during configure. The PowerShell scripts in `tools/` are convenience wrappers around the same preset path.

## Host Automation

Hosts and tools are moving toward a shared automation contract. Supported executables reject unknown options, and tools that opt in can write structured JSON results:

```powershell
.\build\msvc-debug\scene_probe.exe --scene config/scenes/bootstrap.scene.json --result-file build/msvc-debug/test-artifacts/scene_probe_result.json
.\build\msvc-debug\vulkan_scene_viewer.exe --frames 3 --result-file build/msvc-debug/test-artifacts/viewer_result.json
```

The common parser recognizes future-facing flags such as `--ticks`, `--offline`, `--headless`, `--hidden-window`, `--input-script`, `--command-script`, `--seed`, and `--log-file`; each executable still rejects common flags it does not support yet.

## Bootstrap Scene

The first scene uses a procedural floor plus character instances because no environment scene models are present yet.

```powershell
.\tools\prepare-assets.ps1
cmake --build --preset msvc-debug
.\build\msvc-debug\scene_probe.exe
```

## Scene Viewer

Build and run the Vulkan scene viewer:

```powershell
.\tools\run-scene-viewer.ps1
```

The viewer keeps running until the window is closed. Press Escape to close it from the keyboard. By default the camera is anchored behind the player model; pass `--orbit-camera` to use the orbiting inspection camera, or `--frames 3` for a short smoke test.

Prototype controls:

- `WASD` or XInput left stick moves the player.
- Tab or Caps Lock toggles between cursor mode and camera steering mode.
- Mouse wheel changes the player-follow camera distance from first-person to 3x the default range.
- In cursor mode, the mouse pointer is visible. Left click the projected ground square to rotate the player toward that point and move there.
- In camera steering mode, mouse movement rotates the player-follow camera and player movement/facing locks to the camera direction.
- Hold Shift to sprint at the configured sprint speed scale.
- Arrow keys move the sparring partner.
- Escape closes the viewer.

Movement currently runs through a small fixed-tick combat simulation layer, while presentation interpolates combat transforms and renders cached model geometry. Cursor mode uses character-facing movement and click-to-move world targets. Camera steering mode uses camera-relative movement and facing. Diagonal movement input is normalized so `A+W` and `D+W` do not move faster than straight movement. Backpedal moves at one-third of normal run speed and does not sprint.

Movement tuning and key bindings live in `config/controls/player_control_profile.json`. The viewer hot-reloads that profile while running, so run speed, sprint scale, backpedal scale, arrival radius, arena inset, and keyboard bindings can be tuned without rebuilding.

## UDP Relay Prototype

Run the relay server and two optimistic clients:

```powershell
.\tools\start-network-clients.ps1
```

Pass `-Clients 5` to launch five viewer processes. Each viewer owns one local actor and dynamically creates remote actors as peers connect. Add `-RandomIds` to assign unique random client ids for independent connection testing. The relay caches the latest actor snapshot per connected client so late joiners receive the current peer state instead of starting from only placeholder spawns.

Or run the pieces manually:

```powershell
.\tools\build.ps1
.\build\msvc-debug\udp_state_server.exe --bind 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 1 --net-server 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 2 --net-server 127.0.0.1:40000
```

Clients explicitly connect before snapshots are accepted and disconnect on shutdown. The server accepts an arbitrary number of client sessions and fans out validated packets to connected peers. The current viewer maps client `1` to the first character and client `2` to the second character.

Run the regression suite:

```powershell
ctest --preset msvc-debug
```

## Architecture Notes

See `docs/simulation-presentation-split.md` for the intended split between authoritative gameplay simulation and visual presentation.
See `docs/input-pipeline.md` for the intended path from platform input to fixed-tick combat commands.
See `docs/control-profile.md` for hot-reloaded movement tuning and key binding data.
See `docs/network-architecture.md` for the current UDP relay protocol and client/server split.
