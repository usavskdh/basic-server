# PowerShell Server Restart Script
# Usage: .\deployment\restart.ps1

param(
    [string]$RemoteHost = "gameserver"
)

$ErrorActionPreference = "Stop"

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "   Restarting Game Server        " -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

# Check SSH connection
Write-Host "Connecting to $RemoteHost..." -ForegroundColor Yellow
ssh -o ConnectTimeout=5 $RemoteHost "echo 'Connected'" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Cannot connect to $RemoteHost" -ForegroundColor Red
    exit 1
}

Write-Host "  ✓ Connected" -ForegroundColor Green

# Restart server
Write-Host "Restarting server..." -ForegroundColor Yellow

$restartScript = @'
#!/bin/bash
if systemctl is-active --quiet gameserver 2>/dev/null; then
    echo "  → Restarting via systemd..."
    sudo systemctl restart gameserver
    sleep 2
    if systemctl is-active --quiet gameserver; then
        echo "  ✓ Server restarted successfully"
    else
        echo "  ✗ Server failed to restart"
        exit 1
    fi
else
    echo "  → No systemd service found, restarting manually..."
    pkill -9 Server 2>/dev/null || true
    cd ~/intelligent_design/server_standalone/build
    nohup ./Server > server.log 2>&1 &
    echo "  ✓ Server started (PID: $!)"
fi
'@

ssh $RemoteHost "bash -s" <<< $restartScript

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "==================================" -ForegroundColor Cyan
    Write-Host "   Server Restarted! ✓           " -ForegroundColor Green
    Write-Host "==================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Check status with: .\deployment\monitor.ps1" -ForegroundColor Gray
} else {
    Write-Host ""
    Write-Host "ERROR: Restart failed" -ForegroundColor Red
    Write-Host "Check logs with: .\deployment\logs.ps1" -ForegroundColor Yellow
    exit 1
}
