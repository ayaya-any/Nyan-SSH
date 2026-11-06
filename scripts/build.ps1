param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',
    [string]$QtRoot = 'D:\Qt\6.12.0\msvc2022_64',
    [string]$OpenSslRoot = $env:OPENSSL_ROOT_DIR,
    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$SkipDeploy
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\$Configuration"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere not found; install Visual Studio 2022/2026 with the C++ workload."
}
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "No Visual Studio installation with C++ tools found."
}
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (-not (Test-Path $cmake)) { $cmake = 'cmake' }
if (-not (Test-Path $ninja)) { $ninja = 'ninja' }

Write-Host "==> Visual Studio: $vsPath" -ForegroundColor Cyan
Write-Host "==> Qt: $QtRoot" -ForegroundColor Cyan
if ($OpenSslRoot) { Write-Host "==> OpenSSL: $OpenSslRoot" -ForegroundColor Cyan }
Write-Host "==> Build: $buildDir ($Configuration)" -ForegroundColor Cyan

cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}
$env:VSLANG = '1033'

$configureArgs = @(
    '-S', $root,
    '-B', $buildDir,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DQT_ROOT=$QtRoot",
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    '-DNYAN_BUILD_TESTS=ON'
)
if ($OpenSslRoot) { $configureArgs += "-DOPENSSL_ROOT_DIR=$OpenSslRoot" }
if ($Clean) { $configureArgs += '--fresh' }

& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

& $cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

$exe = Join-Path $buildDir 'NyanSSH.exe'
if (-not $SkipDeploy -and (Test-Path $exe)) {
    Write-Host "==> Deploying Qt runtime" -ForegroundColor Cyan
    $windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
    if (Test-Path $windeployqt) {
        & $windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw --no-quick-import --compiler-runtime $exe | Out-Null
    } else {
        Write-Warning "windeployqt not found at $windeployqt"
    }
    Copy-Item (Join-Path $root 'README.md') $buildDir -Force
    Copy-Item (Join-Path $root 'LICENSE') $buildDir -Force
    Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') $buildDir -Force
    Write-Host "==> Ready: $exe" -ForegroundColor Green
}

if (-not $SkipTests -and (Test-Path (Join-Path $buildDir 'nyan_tests.exe'))) {
    Write-Host "==> Running unit tests" -ForegroundColor Cyan
    $env:PATH = "$QtRoot\bin;$env:PATH"
    if ($OpenSslRoot) { $env:PATH = "$OpenSslRoot\bin;$env:PATH" }
    $env:QT_PLUGIN_PATH = "$QtRoot\plugins"
    & (Join-Path $buildDir 'nyan_tests.exe') -txt
    if ($LASTEXITCODE -ne 0) { throw "Unit tests failed ($LASTEXITCODE)" }
}
