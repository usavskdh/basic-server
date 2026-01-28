# PowerShell Server Logs Viewer
# Usage: .\deployment\logs.ps1 [-Lines 50] [-Follow]

param(
    [string]$RemoteHost = "gameserver",
    [int]$Lines = 100,
    [switch]$Follow
)

$ErrorActionPreference = "Stop"

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "   Game Server Logs              " -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""

# Check SSH connection
ssh -o ConnectTimeout=5 $RemoteHost "echo 'Connected'" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Cannot connect to $RemoteHost" -ForegroundColor Red
    exit 1
}

if ($Follow) {
    Write-Host "Following live logs (Ctrl+C to exit)..." -ForegroundColor Yellow
    Write-Host ""
    ssh $RemoteHost "sudo journalctl -u gameserver -f --no-pager"
} else {
    Write-Host "Last $Lines log lines:" -ForegroundColor Yellow
    Write-Host ""
    ssh $RemoteHost "sudo journalctl -u gameserver -n $Lines --no-pager"
}

Write-Host ""
Write-Host "==================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tip: Use -Follow to watch live logs" -ForegroundColor Gray
Write-Host "     .\deployment\logs.ps1 -Follow" -ForegroundColor Gray
