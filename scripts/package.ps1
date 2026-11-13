param(
    [string]$Configuration = 'Release',
    [string]$QtRoot = 'D:\Qt\6.12.0\msvc2022_64',
    [string]$OpenSslRoot = $env:OPENSSL_ROOT_DIR,
    [switch]$AllowQt610,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\$Configuration"
$distRoot = Join-Path $root 'dist'
$cmakeProject = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeProject -notmatch 'project\(NyanSSH VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "Unable to read project version from CMakeLists.txt"
}
$version = $Matches[1]
$appDir = Join-Path $distRoot "NyanSSH-$version-win64"
$archive = Join-Path $distRoot "NyanSSH-$version-win64.zip"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -QtRoot $QtRoot `
        -OpenSslRoot $OpenSslRoot -AllowQt610:$AllowQt610 -Clean -SkipTests
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = 'cmake' }

if (Test-Path $appDir) { Remove-Item -Recurse -Force $appDir }
New-Item -ItemType Directory -Force $appDir | Out-Null

Write-Host "==> Installing to $appDir" -ForegroundColor Cyan
& $cmake --install $buildDir --prefix $appDir
if ($LASTEXITCODE -ne 0) { throw "Install failed ($LASTEXITCODE)" }

$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
if (Test-Path $windeployqt) {
    Write-Host "==> Ensuring MSVC runtime is bundled" -ForegroundColor Cyan
    & $windeployqt --no-translations --no-system-d3d-compiler --compiler-runtime `
        (Join-Path $appDir 'NyanSSH.exe') | Out-Null
}

# windeployqt may skip the CRT when it is already installed system-wide.
# Copy the release CRT explicitly so the ZIP is self-contained.
$crtDir = Get-ChildItem (Join-Path $vsPath 'VC\Redist\MSVC') -Recurse -Directory `
    -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' -and $_.FullName -notmatch 'debug|onecore' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($crtDir) {
    Copy-Item (Join-Path $crtDir.FullName '*.dll') $appDir -Force
}

if (Test-Path $archive) { Remove-Item -Force $archive }
Compress-Archive -Path (Join-Path $appDir '*') -DestinationPath $archive -CompressionLevel Optimal
Write-Host "==> Package: $archive" -ForegroundColor Green
