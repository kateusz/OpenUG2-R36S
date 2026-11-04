#Requires -Version 5.1
<#
.SYNOPSIS
    Deploys the built OpenUG2 port payload and game data to the ArkOS SD card.

.DESCRIPTION
    Copies dist/OpenUG2-r36s/* to F:\ports\OpenUG2\, the two launchers to
    F:\ports\, and the six required game data directories to
    F:\ports\OpenUG2\data\.

    Nothing is ever deleted: robocopy runs in additive mode (/E, no /MIR), so
    settings, saves and logs already on the card survive a redeploy.

.PARAMETER Destination
    EASYROMS root. Defaults to F:\.

.PARAMETER DataRoot
    Retail NFSU2 installation. Defaults to D:\Need for Speed Underground 2.

.PARAMETER SkipData
    Update only the binary and port files; leave the game data untouched.

.PARAMETER DataOnly
    Copy only the game data.

.EXAMPLE
    .\deploy.ps1
    .\deploy.ps1 -SkipData
#>
[CmdletBinding()]
param(
    [string]$Destination = 'F:\',
    [string]$DataRoot = 'D:\Need for Speed Underground 2',
    [switch]$SkipData,
    [switch]$DataOnly
)

$ErrorActionPreference = 'Stop'

$here   = $PSScriptRoot
$repo   = (Resolve-Path (Join-Path $here '..\..')).Path
$outDir = Join-Path $repo 'dist\OpenUG2-r36s'
$ports  = Join-Path $Destination 'ports'
$port   = Join-Path $ports 'OpenUG2'
$data   = Join-Path $port 'data'

$dataDirs = @('CARS', 'TRACKS', 'GLOBAL', 'FRONTEND', 'LANGUAGES', 'SOUND')

# Robocopy: 0-7 are success (0 = nothing to do, 1 = files copied, 3 = extras).
function Invoke-Robocopy {
    param([string]$Source, [string]$Target, [string]$Label)
    Write-Host ("== {0} ==" -f $Label)
    $rcArgs = @($Source, $Target, '/E', '/R:2', '/W:2', '/NFL', '/NDL', '/NP', '/NJH', '/NJS')
    & robocopy @rcArgs | ForEach-Object { if ($_ -match '\S') { Write-Host ("   {0}" -f $_.Trim()) } }
    $code = $LASTEXITCODE
    if ($code -ge 8) { throw ("robocopy failed for {0} (exit {1})" -f $Label, $code) }
    return $code
}

if (-not (Test-Path -LiteralPath $outDir)) {
    throw "No build output at $outDir. Run tools\arm64\build.ps1 first."
}
if (-not (Test-Path -LiteralPath $ports)) {
    throw "$ports does not exist. Is the ArkOS card mounted at $Destination?"
}

if (-not $DataOnly) {
    New-Item -ItemType Directory -Force -Path $port | Out-Null
    $null = Invoke-Robocopy -Source $outDir -Target $port -Label "port files -> $port"

    # EmulationStation lists top-level /roms/ports/*.sh entries. Two names for
    # the same game: "OpenUG2" can be confused with the OpenUG2 folder in the
    # Ports list, "NFSU2" cannot.
    foreach ($name in @('OpenUG2.sh', 'NFSU2.sh', 'OpenUG2 Diagnostics.sh', 'OpenUG2 Performance.sh')) {
        $src = Join-Path $outDir $name
        if (-not (Test-Path -LiteralPath $src)) { throw "Missing launcher $src" }
        Copy-Item -LiteralPath $src -Destination (Join-Path $ports $name) -Force
        Write-Host ("   {0}" -f (Join-Path $ports $name))
    }
}

if (-not $SkipData) {
    if (-not (Test-Path -LiteralPath $DataRoot)) {
        throw "Game data not found at $DataRoot. Pass -DataRoot <path>."
    }

    # Rough size of the subset: ~1.2 GB. Keep a safety margin.
    $need = 1.5GB
    $free = (Get-PSDrive -Name ($Destination.Substring(0, 1))).Free
    if ($free -lt $need) {
        throw ("Not enough space on {0}: {1:N2} GB free, need about {2:N2} GB." -f $Destination, ($free / 1GB), ($need / 1GB))
    }

    New-Item -ItemType Directory -Force -Path $data | Out-Null
    foreach ($dir in $dataDirs) {
        $src = Join-Path $DataRoot $dir
        if (-not (Test-Path -LiteralPath $src)) { throw "Missing game data directory $src" }
        $null = Invoke-Robocopy -Source $src -Target (Join-Path $data $dir) -Label "$dir -> $data\$dir"
    }
}

$binary = Join-Path $port 'nfsu2'
if (Test-Path -LiteralPath $binary) {
    $size = (Get-Item -LiteralPath $binary).Length
    Write-Host ("== Binary on card: {0} ({1:N1} MB) ==" -f $binary, ($size / 1MB))
}
$free = (Get-PSDrive -Name ($Destination.Substring(0, 1))).Free
Write-Host ("== Free space on {0}: {1:N2} GB ==" -f $Destination, ($free / 1GB))
Write-Host 'Done. Eject the card safely, then start "OpenUG2" from the Ports menu.'
