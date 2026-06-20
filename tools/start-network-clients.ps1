param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug",

    [int]$Port = 40000,

    [switch]$NoServer
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "dev-shell.ps1")
& (Join-Path $PSScriptRoot "prepare-assets.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $Preset --target udp_state_server vulkan_scene_viewer
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildDir = Resolve-Path (Join-Path $PSScriptRoot "..\build\$Preset")
$serverExe = Join-Path $buildDir "udp_state_server.exe"
$viewerExe = Join-Path $buildDir "vulkan_scene_viewer.exe"
$serverEndpoint = "127.0.0.1:$Port"

if (-not $NoServer) {
    $server = Start-Process -FilePath $serverExe `
        -ArgumentList @("--bind", $serverEndpoint, "--dump-packets", "64") `
        -PassThru `
        -WindowStyle Hidden
    Start-Sleep -Milliseconds 250
    Write-Host "Started relay pid=$($server.Id) on $serverEndpoint"
}

$client1 = Start-Process -FilePath $viewerExe `
    -ArgumentList @("--net-client-id", "1", "--net-server", $serverEndpoint) `
    -PassThru
$client2 = Start-Process -FilePath $viewerExe `
    -ArgumentList @("--net-client-id", "2", "--net-server", $serverEndpoint) `
    -PassThru

Write-Host "Started client 1 pid=$($client1.Id)"
Write-Host "Started client 2 pid=$($client2.Id)"
