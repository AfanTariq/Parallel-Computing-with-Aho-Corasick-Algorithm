[CmdletBinding()]
param([switch]$InstallDependencies, [switch]$Full, [int]$Repeats = 3)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Missing '$Name'. Install MinGW-w64 GCC and Python 3, or rerun with -InstallDependencies."
    }
}

if ($InstallDependencies) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "winget is required for automatic Windows dependency installation."
    }
    winget install --id Python.Python.3.12 --exact --accept-package-agreements --accept-source-agreements
    winget install --id MSYS2.MSYS2 --exact --accept-package-agreements --accept-source-agreements
    $env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:Path"
    & C:\msys64\usr\bin\bash.exe -lc "pacman --noconfirm -Syuu; pacman --noconfirm -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python"
}

Require-Command python
Require-Command gcc
& powershell -ExecutionPolicy Bypass -File "$Root\scripts\build_windows.ps1"
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$campaignArgs = @("scripts\run_campaign.py", "--repeats", $Repeats)
if ($Full) { $campaignArgs += "--full" }
& python @campaignArgs
if ($LASTEXITCODE -ne 0) { throw "Campaign failed; preserve the partial results directory." }
