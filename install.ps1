# FasthonJIT PowerShell Installer for Windows
[CmdletBinding()]
param (
    [string]$Version = $env:FASTHON_VERSION
)

$ErrorActionPreference = 'Stop'
$repo = "sunandar3221/fasthonjit"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "       FasthonJIT Windows Installer       " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "  [3] FasthonJIT v3.0.0 (Prerelease terbaru) [Default]"
    Write-Host "  [2] FasthonJIT v2.0.0"
    Write-Host "  [1] Fasthon v1.0.0 (Legacy)"
    $choice = Read-Host "Pilih versi [3/2/1] (default: 3)"
    if ([string]::IsNullOrWhiteSpace($choice)) { $choice = "3" }
    $Version = $choice
}

# Normalize version
switch -Regex ($Version) {
    "^(1|v1|v1\.0\.0|legacy)$" {
        $tag = "v1.0.0"
        $display = "v1.0.0 (Fasthon Legacy)"
    }
    "^(2|v2|v2\.0\.0|2\.0\.0)$" {
        $tag = "v2.0.0"
        $display = "v2.0.0 (FasthonJIT Native Engine)"
    }
    "^(3|v3|v3\.0\.0|3\.0\.0|latest)$" {
        $tag = "v3.0.0"
        $display = "v3.0.0 (FasthonJIT prerelease terbaru)"
    }
    default {
        throw "Versi tidak dikenal: $Version. Pilih 3, 2, atau 1."
    }
}

Write-Host "Versi Dipilih  : $display" -ForegroundColor Yellow
$installDir = "$HOME\AppData\Local\Microsoft\WindowsApps"
if (-not (Test-Path -Path $installDir)) {
    $installDir = "$HOME\bin"
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$installDir*") {
        [Environment]::SetEnvironmentVariable("Path", "$userPath;$installDir", "User")
        $env:Path += ";$installDir"
    }
}

Write-Host "Target Lokasi  : $installDir" -ForegroundColor Yellow

$assetCandidates = @(
    "fasthonjit.exe",
    "fasthonjit-windows-x64.exe",
    "fasthon.exe",
    "fasthon-windows-x64.exe"
)

$tempExe = [System.IO.Path]::GetTempFileName() + ".exe"
$downloaded = $false

foreach ($asset in $assetCandidates) {
    $urls = @(
        "https://github.com/$repo/releases/download/$tag/$asset",
        "https://github.com/sunandar3221/Fasthon/releases/download/$tag/$asset"
    )

    foreach ($url in $urls) {
        try {
            Write-Host "Mengunduh dari $url..." -ForegroundColor DarkGray
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $url -OutFile $tempExe -UseBasicParsing -TimeoutSec 30 -ErrorAction Stop
            if ((Get-Item $tempExe).Length -gt 100000) {
                $downloaded = $true
                break
            }
        } catch {
            # Try next URL
        }
    }
    if ($downloaded) { break }
}

if (-not $downloaded) {
    Write-Host "Error: Gagal mengunduh binary FasthonJIT dari GitHub Releases." -ForegroundColor Red
    Write-Host "Silakan unduh manual dari: https://github.com/$repo/releases" -ForegroundColor Red
    if (Test-Path $tempExe) { Remove-Item $tempExe -Force }
    exit 1
}

# Install binaries and aliases
$targetMain = Join-Path $installDir "fasthonjit.exe"
$targetAlias1 = Join-Path $installDir "fasthon.exe"
$targetAlias2 = Join-Path $installDir "eas.exe"

Copy-Item -Path $tempExe -Destination $targetMain -Force
Copy-Item -Path $tempExe -Destination $targetAlias1 -Force
Move-Item -Path $tempExe -Destination $targetAlias2 -Force

Write-Host "==========================================" -ForegroundColor Green
Write-Host " FasthonJIT berhasil terpasang di Windows!" -ForegroundColor Green
Write-Host " Lokasi binary: $targetMain" -ForegroundColor Green
Write-Host " Jalankan     : fasthonjit" -ForegroundColor White
Write-Host " Cek JIT info : fasthonjit --jit-info" -ForegroundColor White
Write-Host " Alias aktif  : 'fasthon' dan 'eas'" -ForegroundColor White
Write-Host "==========================================" -ForegroundColor Green
