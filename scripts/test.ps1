param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$QtRoot = 'D:\Qt\6.12.0\msvc2022_64',
    [string]$OpenSslRoot = $env:OPENSSL_ROOT_DIR,
    [int]$Port = 2222,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\$Configuration"
$serverScript = Join-Path $root 'tests\server\test_ssh_server.py'
$serverProcess = $null

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -QtRoot $QtRoot -OpenSslRoot $OpenSslRoot -SkipTests
}

$env:PATH = "$QtRoot\bin;$env:PATH"
if ($OpenSslRoot) { $env:PATH = "$OpenSslRoot\bin;$env:PATH" }
$env:QT_PLUGIN_PATH = "$QtRoot\plugins"
$env:QT_QPA_PLATFORM = ''

python -c "import paramiko" 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Python paramiko is required: python -m pip install paramiko"
}

$listening = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if (-not $listening) {
    $log = Join-Path $root 'tests\server\server.log'
    $errLog = Join-Path $root 'tests\server\server.err.log'
    $serverProcess = Start-Process -FilePath 'python' -ArgumentList '-u', $serverScript, '--port', $Port `
        -RedirectStandardOutput $log -RedirectStandardError $errLog -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) {
        if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)) {
        throw "Test SSH server failed to start; see tests/server/server.err.log"
    }
}

# Compute the SHA256 fingerprint of the generated test host key.
$fingerprint = python -c @"
import base64, hashlib, sys, paramiko
key = paramiko.ECDSAKey(filename=sys.argv[1])
print('SHA256:' + base64.b64encode(hashlib.sha256(key.asbytes()).digest()).decode().rstrip('='))
"@ (Join-Path $root 'tests\server\host_ecdsa.key')
$fingerprint = $fingerprint.Trim()
$keyPath = Join-Path $root 'tests\server\client_rsa.key'
if (-not (Test-Path $keyPath)) {
    throw "Client key missing; start the test server once to generate it."
}

$script:failed = 0
function Invoke-Test([string]$name, [string]$exe, [string[]]$arguments) {
    Write-Host "==> $name" -ForegroundColor Cyan
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $name ($LASTEXITCODE)" -ForegroundColor Red
        $script:failed++
    }
}

try {
    Invoke-Test 'unit tests' (Join-Path $buildDir 'nyan_tests.exe') @('-txt')
    Invoke-Test 'ssh backend smoke' (Join-Path $buildDir 'nyan_smoke.exe') @(
        '--host', '127.0.0.1', '--port', "$Port", '--user', 'nyan', '--password', 'nyan123',
        '--fingerprint', $fingerprint)
    Invoke-Test 'ssh public-key smoke' (Join-Path $buildDir 'nyan_smoke.exe') @(
        '--host', '127.0.0.1', '--port', "$Port", '--user', 'nyan', '--key', $keyPath,
        '--fingerprint', $fingerprint)
    Invoke-Test 'ssh worker' (Join-Path $buildDir 'nyan_ssh_worker_test.exe') @()
    Invoke-Test 'sftp worker' (Join-Path $buildDir 'nyan_sftp_worker_test.exe') @(
        '--fingerprint', $fingerprint)
    Invoke-Test 'file panel integration' (Join-Path $buildDir 'nyan_ui_integration.exe') @(
        '--fingerprint', $fingerprint)
} finally {
    if ($serverProcess -and -not $serverProcess.HasExited) {
        Stop-Process -Id $serverProcess.Id -Force
    }
}

if ($script:failed -gt 0) {
    throw "$($script:failed) test group(s) failed."
}
Write-Host "ALL TESTS PASSED" -ForegroundColor Green
