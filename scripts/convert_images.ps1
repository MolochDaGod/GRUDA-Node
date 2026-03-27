# GRUDA Node — Re-generate all C image headers from source assets
# Run this whenever you update an image file in D:\GRUDA-Node\
# Usage: pwsh -File scripts\convert_images.ps1

$root = "$PSScriptRoot\.."
$tool = "$root\tools\img_to_c.py"
$include = "$root\include"

$assets = @(
    @{ src = "logo.png"; name = "grudge_logo"; w = 120; h = 120; gif = $false },
    @{ src = "Grudge.png"; name = "grudge_header"; w = 240; h = 80; gif = $false },
    @{ src = "account.png"; name = "account_btn"; w = 40; h = 40; gif = $false },
    @{ src = "walletintro.gif"; name = "wallet_intro"; w = 240; h = 320; gif = $true; frames = 8; opts = @("--fit", "cover", "--dither", "--contrast", "1.08", "--saturation", "1.12", "--sharpness", "1.10") }
)

foreach ($a in $assets) {
    $src = "$root\$($a.src)"
    $out = "$include\img_$($a.name).h"
    if (-not (Test-Path $src)) {
        Write-Warning "Missing: $src — skipping"
        continue
    }
    $args_ = @($tool, $src, "--name", $a.name, "--width", $a.w, "--height", $a.h, "--out", $out)
    if ($a.gif) { $args_ += @("--gif", "--max-frames", $a.frames) }
    if ($a.ContainsKey("opts")) { $args_ += $a.opts }
    Write-Host "Converting $($a.src)..."
    py @args_
}

Write-Host "`nDone. Headers written to $include"
