#Requires -Version 5.1
<#
.SYNOPSIS
    Builds the OpenUG2 aarch64 + OpenGL ES port payload for ArkOS/dArkOS handhelds.

.DESCRIPTION
    Builds the arm64 Ubuntu 20.04 image in tools/arm64/Dockerfile, runs
    `make gles` inside it, copies the resulting ELF to dist/OpenUG2-r36s/nfsu2,
    then stages the launcher scripts and config next to it.

    The container runs under Docker Desktop's QEMU emulation, so the first build
    is slow (tens of minutes). The image layer is cached; later builds only
    recompile.

.PARAMETER Image
    Docker image tag to build/use.

.PARAMETER NoCache
    Rebuild the image from scratch.

.PARAMETER SkipImage
    Use the existing image without rebuilding it.

.PARAMETER StageOnly
    Skip Docker entirely and only refresh the launcher/config files in
    dist/OpenUG2-r36s from tools/arm64/port.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -NoCache
    .\build.ps1 -StageOnly
#>
[CmdletBinding()]
param(
    [string]$Image = 'openug2-arm64-builder',
    [switch]$NoCache,
    [switch]$SkipImage,
    [switch]$StageOnly
)

$ErrorActionPreference = 'Stop'

$here    = $PSScriptRoot
$repo    = (Resolve-Path (Join-Path $here '..\..')).Path
$outDir  = Join-Path $repo 'dist\OpenUG2-r36s'
$logFile = Join-Path $repo 'dist\build-arm64.log'

New-Item -ItemType Directory -Force -Path $outDir, (Split-Path $logFile) | Out-Null

# Native tools (docker) write progress to stderr. Windows PowerShell 5.1 turns
# those lines into ErrorRecords, and with ErrorActionPreference=Stop that would
# abort a healthy build. Capture stderr, and keep Tee-Object's pass-through out
# of this function's return value (only the exit code may be emitted).
function Invoke-Native {
    param([string[]]$Arguments, [string]$LogPath)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        if ($LogPath) {
            & docker @Arguments 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        } else {
            & docker @Arguments 2>&1 | Out-Host
        }
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
}

if (-not $StageOnly) {
    Write-Host '== Docker engine =='
    docker version --format '{{.Server.Version}}'
    if ($LASTEXITCODE -ne 0) { throw 'Docker engine is not reachable. Start Docker Desktop first.' }

    if (-not $SkipImage) {
        Write-Host '== Building arm64 build image =='
        $dockerArgs = @('build', '--platform', 'linux/arm64', '-t', $Image, '-f', (Join-Path $here 'Dockerfile'))
        if ($NoCache) { $dockerArgs += '--no-cache' }
        $dockerArgs += $here
        $code = Invoke-Native -Arguments $dockerArgs
        if ($code -ne 0) { throw 'docker build failed.' }
    }

    # Docker Desktop wants forward slashes in bind-mount sources.
    $repoMount = $repo -replace '\\', '/'
    $outMount  = $outDir -replace '\\', '/'

    # The helper is mounted read-only; strip CR and run a copy so a Windows
    # checkout can never break the shebang.
    $inner = "sed 's/\r`$//' /src/tools/arm64/build-in-container.sh > /tmp/build.sh && sh /tmp/build.sh"

    Write-Host '== Compiling (arm64, QEMU) =='
    $runArgs = @(
        'run', '--rm', '--platform', 'linux/arm64',
        '-v', "${repoMount}:/src:ro",
        '-v', "${outMount}:/out",
        '-e', 'SRC=/src', '-e', 'WORK=/work', '-e', 'OUT=/out',
        $Image, 'sh', '-c', $inner
    )
    $code = Invoke-Native -Arguments $runArgs -LogPath $logFile
    if ($code -ne 0) { throw "Build failed; see $logFile" }

    $binary = Join-Path $outDir 'nfsu2'
    if (-not (Test-Path $binary)) { throw "Build produced no binary at $binary" }
}

# Stage the ArkOS payload. Shell scripts must reach the SD card with LF
# endings and no BOM; config.txt is sourced by bash, so it gets the same
# treatment.
Write-Host '== Staging port payload =='
$portSrc = Join-Path $here 'port'
$utf8 = New-Object System.Text.UTF8Encoding($false)
Get-ChildItem -Path $portSrc -File | Sort-Object Name | ForEach-Object {
    $text = [System.IO.File]::ReadAllText($_.FullName)
    $text = $text -replace "`r`n", "`n" -replace "`r", "`n"
    [System.IO.File]::WriteAllText((Join-Path $outDir $_.Name), $text, $utf8)
    Write-Host ("  {0}" -f $_.Name)
}

$binary = Join-Path $outDir 'nfsu2'
if (Test-Path $binary) {
    $size = (Get-Item $binary).Length
    Write-Host ("== Binary: {0} ({1:N1} MB) ==" -f $binary, ($size / 1MB))
    Get-FileHash $binary -Algorithm SHA256 | Format-List
} else {
    Write-Host "== No binary in $outDir yet =="
}
