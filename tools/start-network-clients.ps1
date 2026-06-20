param(
    [ValidateSet("msvc-debug", "msvc-release")]
    [string]$Preset = "msvc-debug",

    [int]$Port = 40000,

    [ValidateRange(1, 255)]
    [int]$Clients = 2,

    [int]$Frames = 0,

    [switch]$RandomIds,

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
$logDir = Join-Path $buildDir "run-logs\network-clients"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not $NoServer) {
    $existingServer = Get-CimInstance Win32_Process -Filter "name = 'udp_state_server.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -like "*--bind $serverEndpoint*" } |
        Select-Object -First 1

    if ($existingServer) {
        Write-Host "Reusing relay pid=$($existingServer.ProcessId) on $serverEndpoint"
    } else {
        $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $serverOut = Join-Path $logDir "server-$stamp.out.log"
        $serverErr = Join-Path $logDir "server-$stamp.err.log"
        $server = Start-Process -FilePath $serverExe `
            -ArgumentList @("--bind", $serverEndpoint, "--dump-packets", "64") `
            -WorkingDirectory $buildDir `
            -RedirectStandardOutput $serverOut `
            -RedirectStandardError $serverErr `
            -PassThru `
            -WindowStyle Hidden
        Start-Sleep -Milliseconds 350
        $server.Refresh()
        if ($server.HasExited) {
            Write-Host "Relay stdout:"
            if (Test-Path $serverOut) { Get-Content $serverOut }
            Write-Host "Relay stderr:"
            if (Test-Path $serverErr) { Get-Content $serverErr }
            throw "Relay exited immediately with code $($server.ExitCode)."
        }
        Write-Host "Started relay pid=$($server.Id) on $serverEndpoint"
    }
}

function New-ClientIds {
    param(
        [Parameter(Mandatory = $true)][int]$Count,
        [Parameter(Mandatory = $true)][bool]$Random
    )

    if (-not $Random) {
        return @(1..$Count)
    }

    $ids = [System.Collections.Generic.HashSet[int]]::new()
    while ($ids.Count -lt $Count) {
        [void]$ids.Add((Get-Random -Minimum 1 -Maximum 256))
    }
    return @($ids | Sort-Object)
}

$clientIds = New-ClientIds -Count $Clients -Random ([bool]$RandomIds)
Write-Host "Client ids: $($clientIds -join ', ')"

$started = @()
foreach ($clientId in $clientIds) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $clientOut = Join-Path $logDir "client$clientId-$stamp.out.log"
    $clientErr = Join-Path $logDir "client$clientId-$stamp.err.log"
    $clientArgs = @("--net-client-id", $clientId.ToString(), "--net-server", $serverEndpoint)
    if ($Frames -gt 0) {
        $clientArgs += @("--frames", $Frames.ToString())
    }

    $client = Start-Process -FilePath $viewerExe `
        -ArgumentList $clientArgs `
        -WorkingDirectory $buildDir `
        -RedirectStandardOutput $clientOut `
        -RedirectStandardError $clientErr `
        -PassThru

    $started += $client
    Write-Host "Started client $clientId pid=$($client.Id)"
    Start-Sleep -Milliseconds 150
}

Start-Sleep -Milliseconds 500
for ($index = 0; $index -lt $started.Count; ++$index) {
    $client = $started[$index]
    $clientId = $clientIds[$index]
    $client.Refresh()
    if ($client.HasExited) {
        Write-Host "Client $clientId exited immediately with code $($client.ExitCode). Logs: $logDir"
        throw "Client $clientId failed to stay running."
    }
}

Write-Host "Logs: $logDir"
