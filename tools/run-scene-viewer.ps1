$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "dev-shell.ps1")
& (Join-Path $PSScriptRoot "prepare-assets.ps1")

cmake --preset msvc-debug
cmake --build --preset msvc-debug --target vulkan_scene_viewer

$viewer = Resolve-Path (Join-Path $PSScriptRoot "..\build\msvc-debug\vulkan_scene_viewer.exe")
& $viewer @args
