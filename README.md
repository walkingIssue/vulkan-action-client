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
- Git LFS 3.7.1 at `C:\Users\Bartek\Documents\Playground\tools\git-lfs-3.7.1`

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

## Dependencies

The project uses vcpkg manifest mode. After entering the dev shell:

```powershell
vcpkg install --triplet x64-windows
cmake --preset msvc-debug
```

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
.\build\msvc-debug\udp_state_server.exe --bind 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 1 --net-server 127.0.0.1:40000
.\build\msvc-debug\vulkan_scene_viewer.exe --net-client-id 2 --net-server 127.0.0.1:40000
```

The server only validates and fans out bitpacked actor snapshots. Client `1` owns the first character and client `2` owns the second character.

## Architecture Notes

See `docs/simulation-presentation-split.md` for the intended split between authoritative gameplay simulation and visual presentation.
See `docs/input-pipeline.md` for the intended path from platform input to fixed-tick combat commands.
See `docs/control-profile.md` for hot-reloaded movement tuning and key binding data.
See `docs/network-architecture.md` for the current UDP relay protocol and client/server split.
