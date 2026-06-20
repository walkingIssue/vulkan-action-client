param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug",

    [int]$Port = 40000,

    [int]$DumpPackets = 128
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset -Target udp_state_server
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$server = Resolve-Path (Join-Path $PSScriptRoot "..\build\$Preset\udp_state_server.exe")
& $server --bind "127.0.0.1:$Port" --dump-packets $DumpPackets
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
