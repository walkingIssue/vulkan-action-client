param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug"
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildDir = Resolve-Path (Join-Path $PSScriptRoot "..\build\$Preset")
ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
