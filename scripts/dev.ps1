# GRUDA Node — Developer quick-launch script
# Usage:
#   pwsh -File scripts\dev.ps1 build
#   pwsh -File scripts\dev.ps1 upload
#   pwsh -File scripts\dev.ps1 monitor
#   pwsh -File scripts\dev.ps1 flash    (upload + monitor)
#   pwsh -File scripts\dev.ps1 clean

param([string]$cmd = "build")

$root = "$PSScriptRoot\.."
$env:PATH += ";C:\Users\nugye\AppData\Local\Programs\Python\Python313\Scripts"

switch ($cmd) {
    "build"   { pio run -d $root }
    "upload"  { pio run -d $root --target upload }
    "monitor" { pio device monitor -d $root --baud 115200 --port COM5 }
    "flash"   {
        pio run -d $root --target upload
        if ($LASTEXITCODE -eq 0) {
            pio device monitor -d $root --baud 115200 --port COM5
        }
    }
    "clean"   { pio run -d $root --target clean }
    default   { Write-Error "Unknown command: $cmd. Use: build | upload | monitor | flash | clean" }
}
