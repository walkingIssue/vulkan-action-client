param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build\msvc-debug"),

    [int]$Port = 0,

    [int]$Snapshots = 96
)

$ErrorActionPreference = "Stop"

if ($Port -eq 0) {
    $Port = Get-Random -Minimum 41000 -Maximum 49000
}

$buildDirPath = (Resolve-Path $BuildDir).Path
$serverExe = Join-Path $buildDirPath "udp_state_server.exe"
$clientExe = Join-Path $buildDirPath "network_e2e_client.exe"
if (-not (Test-Path $serverExe)) {
    throw "Missing $serverExe. Build udp_state_server first."
}
if (-not (Test-Path $clientExe)) {
    throw "Missing $clientExe. Build network_e2e_client first."
}

$logDir = Join-Path $buildDirPath "test-logs\network-e2e"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$serverOut = Join-Path $logDir "server-$stamp.out.log"
$serverErr = Join-Path $logDir "server-$stamp.err.log"
$client1Out = Join-Path $logDir "client1-$stamp.out.log"
$client1Err = Join-Path $logDir "client1-$stamp.err.log"
$client2Out = Join-Path $logDir "client2-$stamp.out.log"
$client2Err = Join-Path $logDir "client2-$stamp.err.log"

$endpoint = "127.0.0.1:$Port"
$server = $null
$clients = @()

function Assert-ClientLog {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ErrorPath,
        [Parameter(Mandatory = $true)][int]$ClientId
    )

    $text = ""
    if (Test-Path $Path) { $text = Get-Content -Raw $Path }
    if ($text -notmatch "client=$ClientId remoteSnapshots=(\d+)") {
        $errorText = ""
        if (Test-Path $ErrorPath) { $errorText = Get-Content -Raw $ErrorPath }
        throw "Client $ClientId did not report a successful remote snapshot exchange. stdout='$text' stderr='$errorText'"
    }

    $remoteSnapshots = [int]$Matches[1]
    if ($remoteSnapshots -lt 1) {
        throw "Client $ClientId reported $remoteSnapshots remote snapshots."
    }
}

try {
    $server = Start-Process -FilePath $serverExe `
        -ArgumentList @("--bind", $endpoint, "--dump-packets", "0") `
        -RedirectStandardOutput $serverOut `
        -RedirectStandardError $serverErr `
        -PassThru `
        -WindowStyle Hidden

    Start-Sleep -Milliseconds 400

    $clients += Start-Process -FilePath $clientExe `
        -ArgumentList @("--server", $endpoint, "--client-id", "1", "--snapshots", "$Snapshots", "--expect-remote-snapshots", "1") `
        -RedirectStandardOutput $client1Out `
        -RedirectStandardError $client1Err `
        -PassThru `
        -WindowStyle Hidden
    $clients += Start-Process -FilePath $clientExe `
        -ArgumentList @("--server", $endpoint, "--client-id", "2", "--snapshots", "$Snapshots", "--expect-remote-snapshots", "1") `
        -RedirectStandardOutput $client2Out `
        -RedirectStandardError $client2Err `
        -PassThru `
        -WindowStyle Hidden

    $deadline = (Get-Date).AddSeconds(20)
    while (($clients | Where-Object { -not $_.HasExited }).Count -gt 0) {
        if ((Get-Date) -gt $deadline) {
            throw "Timed out waiting for e2e clients."
        }
        Start-Sleep -Milliseconds 100
        foreach ($client in $clients) {
            $client.Refresh()
        }
    }

    foreach ($client in $clients) {
        $client.Refresh()
        $client.WaitForExit()
        if ($null -ne $client.ExitCode -and $client.ExitCode -ne 0) {
            throw "network_e2e_client pid=$($client.Id) exited with $($client.ExitCode). See $logDir."
        }
    }

    Start-Sleep -Milliseconds 250
} finally {
    foreach ($client in $clients) {
        if ($client -and -not $client.HasExited) {
            Stop-Process -Id $client.Id -Force
        }
    }

    if ($server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}

$serverText = ""
if (Test-Path $serverOut) { $serverText += Get-Content -Raw $serverOut }
if (Test-Path $serverErr) { $serverText += Get-Content -Raw $serverErr }

Assert-ClientLog -Path $client1Out -ErrorPath $client1Err -ClientId 1
Assert-ClientLog -Path $client2Out -ErrorPath $client2Err -ClientId 2

foreach ($pattern in @("client 1 connected", "client 2 connected", "client 1 disconnected", "client 2 disconnected")) {
    if ($serverText -notmatch [regex]::Escape($pattern)) {
        throw "Server log did not contain '$pattern'. See $logDir."
    }
}

Write-Host "Network e2e passed on $endpoint"
Write-Host "Logs: $logDir"
