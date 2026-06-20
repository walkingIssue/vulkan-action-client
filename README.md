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
