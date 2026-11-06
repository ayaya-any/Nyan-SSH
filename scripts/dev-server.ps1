param(
    [int]$Port = 2222
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$server = Join-Path $root 'tests\server\test_ssh_server.py'

python -c "import paramiko" 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Python paramiko is required: python -m pip install paramiko"
}

Write-Host "Starting Nyan SSH test server on 127.0.0.1:$Port" -ForegroundColor Cyan
Write-Host "Credentials: nyan / nyan123" -ForegroundColor Yellow
Write-Host "Client key: tests/server/client_rsa.key" -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop." -ForegroundColor DarkGray
python -u $server --port $Port
