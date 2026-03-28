# GRUDA Node — Re-generate all C image headers from source assets
# Run this whenever you update an image file in D:\GRUDA-Node\
# Usage: pwsh -File scripts\convert_images.ps1

$root = "$PSScriptRoot\.."
$tool = "$root\tools\img_to_c.py"
$include = "$root\include"

# All images get dithering + color enhancement for ILI9341 RGB565 panels.
# Dithering eliminates banding in gradients on 16-bit color.
# Contrast/saturation boost compensates for TN panel wash-out.
$baseOpts = @("--dither", "--contrast", "1.10", "--saturation", "1.15", "--sharpness", "1.08")

$assets = @(
    @{ src = "assets\logo.png"; name = "grudge_logo"; w = 120; h = 120; gif = $false; opts = $baseOpts + @("--fit", "contain") },
    @{ src = "assets\Grudge.png"; name = "grudge_header"; w = 240; h = 80; gif = $false; opts = $baseOpts + @("--fit", "cover") },
    @{ src = "assets\account.png"; name = "account_btn"; w = 40; h = 40; gif = $false; opts = $baseOpts + @("--fit", "contain") },
    @{ src = "assets\walletintro.gif"; name = "wallet_intro"; w = 240; h = 320; gif = $true; frames = 8; opts = $baseOpts + @("--fit", "cover", "--brightness", "1.05") }
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
