$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$modelsRoot = Join-Path $repoRoot "assets\models"
$extractRoot = Join-Path $repoRoot "assets\extracted"
$paladinZip = Join-Path $modelsRoot "stylized_paladin_obj.zip"
$paladinOut = Join-Path $extractRoot "stylized_paladin_obj"
$paladinObj = Join-Path $paladinOut "Stylized_Paladin_Clean.obj"

if (-not (Test-Path $paladinZip)) {
    throw "Missing source asset bundle: $paladinZip"
}

if (-not (Test-Path $paladinObj)) {
    New-Item -ItemType Directory -Path $paladinOut -Force | Out-Null
    Write-Host "Extracting stylized paladin OBJ bundle..."
    Expand-Archive -LiteralPath $paladinZip -DestinationPath $paladinOut -Force
} else {
    Write-Host "Paladin OBJ bundle already extracted."
}

Write-Host "Prepared: $paladinObj"
$global:LASTEXITCODE = 0
