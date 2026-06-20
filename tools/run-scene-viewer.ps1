$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "dev-shell.ps1")
& (Join-Path $PSScriptRoot "prepare-assets.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --preset msvc-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset msvc-debug --target vulkan_scene_viewer
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$viewer = Resolve-Path (Join-Path $PSScriptRoot "..\build\msvc-debug\vulkan_scene_viewer.exe")
& $viewer @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
