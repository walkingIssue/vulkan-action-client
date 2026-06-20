param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug"
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
