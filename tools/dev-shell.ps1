$ErrorActionPreference = "Stop"

function Get-UniquePathEntries {
    param([Parameter(Mandatory = $true)][string[]]$Entries)

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $unique = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in $Entries) {
        if ([string]::IsNullOrWhiteSpace($entry)) {
            continue
        }

        $trimmed = $entry.Trim()
        if ($seen.Add($trimmed)) {
            $unique.Add($trimmed)
        }
    }

    return @($unique)
}

function Set-PathEntries {
    param([Parameter(Mandatory = $true)][string[]]$Entries)

    $env:Path = (Get-UniquePathEntries $Entries) -join ';'
}

function Add-PathEntry {
    param([Parameter(Mandatory = $true)][string]$PathEntry)

    if (-not (Test-Path $PathEntry)) {
        return
    }

    $parts = Get-UniquePathEntries ($env:Path -split ';')
    if ($parts -notcontains $PathEntry) {
        Set-PathEntries (@($PathEntry) + $parts)
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Could not find vswhere.exe. Install Visual Studio Build Tools with the C++ workload."
}

$vsInstall = & $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $vsInstall) {
    throw "Could not find a Visual Studio install with MSVC x64 tools."
}

$vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "Could not find vcvars64.bat at $vcvars."
}

$originalPathEntries = Get-UniquePathEntries ($env:Path -split ';')
$cleanPathEntries = @(
    "$env:SystemRoot\System32",
    $env:SystemRoot,
    "$env:SystemRoot\System32\Wbem",
    "$env:SystemRoot\System32\WindowsPowerShell\v1.0"
)

$originalPath = $env:Path
Set-PathEntries $cleanPathEntries
$envLines = & cmd.exe /d /s /c "`"$vcvars`" >nul && set" 2>&1
$vcvarsExitCode = $LASTEXITCODE
$env:Path = $originalPath
if ($vcvarsExitCode -ne 0) {
    $details = ($envLines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    throw "vcvars64.bat failed with exit code $vcvarsExitCode. $details"
}

foreach ($line in $envLines) {
    $equals = $line.IndexOf("=")
    if ($equals -gt 0) {
        $name = $line.Substring(0, $equals)
        $value = $line.Substring($equals + 1)
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
}

Set-PathEntries (($env:Path -split ';') + $originalPathEntries)

$vulkanSdk = [Environment]::GetEnvironmentVariable("VULKAN_SDK", "Machine")
if (-not $vulkanSdk -and (Test-Path "C:\VulkanSDK\1.4.350.0")) {
    $vulkanSdk = "C:\VulkanSDK\1.4.350.0"
}

if ($vulkanSdk) {
    $env:VULKAN_SDK = $vulkanSdk
    Add-PathEntry (Join-Path $vulkanSdk "Bin")
}

Add-PathEntry (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links")
Add-PathEntry (Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps")

$workspaceTools = Resolve-Path (Join-Path $PSScriptRoot "..\..\tools") -ErrorAction SilentlyContinue
if ($workspaceTools) {
    $workspaceToolsPath = $workspaceTools.Path
    $vcpkgRoot = Join-Path $workspaceToolsPath "vcpkg"
    if (Test-Path $vcpkgRoot) {
        $env:VCPKG_ROOT = $vcpkgRoot
        Add-PathEntry $vcpkgRoot
    }

    $gitLfs = Get-ChildItem -Path $workspaceToolsPath -Directory -Filter "git-lfs-*" -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem -Path $_.FullName -Filter "git-lfs.exe" -Recurse -ErrorAction SilentlyContinue } |
        Select-Object -First 1
    if ($gitLfs) {
        Add-PathEntry $gitLfs.DirectoryName
    }

    $renderDoc = Get-ChildItem -Path $workspaceToolsPath -Filter "qrenderdoc.exe" -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($renderDoc) {
        Add-PathEntry $renderDoc.DirectoryName
    }
}

Write-Host "MSVC, Ninja, Vulkan SDK, vcpkg, and asset tools environment ready."
Write-Host "VULKAN_SDK=$env:VULKAN_SDK"
Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
$global:LASTEXITCODE = 0
