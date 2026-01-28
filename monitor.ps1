# PowerShell Server Monitoring Script
# Usage: .\deployment\monitor.ps1 [-Follow]

param(
    [string]$RemoteHost = "gameserver",
    [switch]$Follow
)

$ErrorActionPreference = "Stop"

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "   Game Server Monitor           " -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

# Check SSH connection
Write-Host "Connecting to $RemoteHost..." -ForegroundColor Yellow
ssh -o ConnectTimeout=5 $RemoteHost "echo 'Connected'" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Cannot connect to $RemoteHost" -ForegroundColor Red
    exit 1
}

# Get server status
$statusScript = @'
#!/bin/bash

echo "=== SYSTEM INFO ==="
echo "Hostname: $(hostname)"
echo "Uptime: $(uptime -p)"
echo "Load: $(uptime | awk -F'load average:' '{print $2}')"
echo ""

echo "=== MEMORY USAGE ==="
free -h | grep -E 'Mem|Swap'
echo ""

echo "=== DISK USAGE ==="
df -h / | tail -n 1
echo ""

echo "=== SERVER STATUS ==="
if systemctl is-active --quiet gameserver 2>/dev/null; then
    echo "Status: RUNNING ✓"
    echo "Service: systemd (gameserver.service)"
    echo ""
    echo "Details:"
    systemctl status gameserver --no-pager | grep -E 'Active:|Main PID:|Memory:|CPU:'
else
    echo "Status: CHECKING PROCESS..."
    if pgrep -x Server >/dev/null; then
        echo "Status: RUNNING (manual) ✓"
        echo "PID: $(pgrep -x Server)"
        echo "Memory: $(ps aux | grep Server | grep -v grep | awk '{print $4}')%"
        echo "CPU: $(ps aux | grep Server | grep -v grep | awk '{print $3}')%"
    else
        echo "Status: NOT RUNNING ✗"
    fi
fi
echo ""

echo "=== NETWORK INFO ==="
echo "Local IP: $(hostname -I | awk '{print $1}')"
echo "Listening Ports:"
sudo netstat -tulpn 2>/dev/null | grep Server || echo "No Server process found"
echo ""

echo "=== RECENT CRASHES ==="
if systemctl is-enabled --quiet gameserver 2>/dev/null; then
    crashes=$(systemctl show gameserver -p NRestarts --value)
    echo "Service restarts: $crashes"
fi
'@

Write-Host "Fetching server status..." -ForegroundColor Yellow
Write-Host ""

ssh $RemoteHost "bash -s" <<< $statusScript

if ($Follow) {
    Write-Host ""
    Write-Host "=== LIVE LOGS (Ctrl+C to exit) ===" -ForegroundColor Cyan
    Write-Host ""
    ssh $RemoteHost "sudo journalctl -u gameserver -f --no-pager"
}

Write-Host ""
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""
