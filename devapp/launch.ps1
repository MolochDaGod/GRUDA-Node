# GRUDA Node DevApp — Launch Script
# Double-click or: pwsh -File D:\GRUDA-Node\devapp\launch.ps1

$appDir = $PSScriptRoot

# Install deps if needed
if (-not (Test-Path "$appDir\node_modules")) {
    Write-Host "[SETUP] Installing dependencies..." -ForegroundColor Yellow
    node --version | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Node.js is not installed. Download from https://nodejs.org"
        Read-Host "Press Enter to exit"
        exit 1
    }
    Push-Location $appDir
    npm install
    Pop-Location
}

# Kill any existing instance on port 3000
$existing = Get-NetTCPConnection -LocalPort 3000 -ErrorAction SilentlyContinue
if ($existing) {
    $pid_ = ($existing | Select-Object -First 1).OwningProcess
    Stop-Process -Id $pid_ -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

Write-Host "`n  GRUDA Node DevApp" -ForegroundColor Red
Write-Host "  Starting server on http://localhost:3000`n" -ForegroundColor Gray

# Start the server
$srv = Start-Process -FilePath "node" -ArgumentList "server.js" -WorkingDirectory $appDir -PassThru -WindowStyle Hidden

# Wait for server to be ready
$ready = $false
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 300
    try {
        $null = Invoke-WebRequest "http://localhost:3000" -UseBasicParsing -TimeoutSec 1
        $ready = $true
        break
    } catch {}
}

if ($ready) {
    Write-Host "  Server ready! Opening browser..." -ForegroundColor Green
    Start-Process "http://localhost:3000"
} else {
    Write-Host "  Server may still be starting. Open http://localhost:3000 manually." -ForegroundColor Yellow
    Start-Process "http://localhost:3000"
}

Write-Host "`n  Server PID: $($srv.Id)  — Close this window to stop." -ForegroundColor Gray
Write-Host "  Press Ctrl+C or close this window to shut down.`n"

# Keep script alive so the server stays up
try {
    Wait-Process -Id $srv.Id
} catch {
    # Server stopped
}
