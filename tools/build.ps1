param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug",

    [string]$Target = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "dev-shell.ps1")

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Target) {
    cmake --build --preset $Preset --target $Target
} else {
    cmake --build --preset $Preset
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
