$ErrorActionPreference = "Stop"

function Add-PathEntry {
    param([Parameter(Mandatory = $true)][string]$PathEntry)

    if (-not (Test-Path $PathEntry)) {
        return
    }

    $parts = $env:Path -split ';' | Where-Object { $_ -ne '' }
    if ($parts -notcontains $PathEntry) {
        $env:Path = "$PathEntry;$env:Path"
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

$envLines = cmd /c "`"$vcvars`" >nul && set"
foreach ($line in $envLines) {
    $equals = $line.IndexOf("=")
    if ($equals -gt 0) {
        $name = $line.Substring(0, $equals)
        $value = $line.Substring($equals + 1)
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
}

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

Write-Host "MSVC, Ninja, and Vulkan SDK environment ready."
Write-Host "VULKAN_SDK=$env:VULKAN_SDK"
