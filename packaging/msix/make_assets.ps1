# Regenerates the MSIX tile and Store images in Assets/ from the app logo
# (sound-mind-studio/assets/ChooseAgainLarge.png). Run it by hand when the
# logo changes, then commit the PNGs:
#
#     powershell -ExecutionPolicy Bypass -File packaging/msix/make_assets.ps1
#
# Uses Windows' own System.Drawing, so it needs nothing installed.

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$logo = [System.Drawing.Image]::FromFile((Join-Path $root "..\..\sound-mind-studio\assets\ChooseAgainLarge.png"))
$assets = Join-Path $root "Assets"
New-Item -ItemType Directory -Force -Path $assets | Out-Null

# name, width, height, margin as a fraction of the shorter side
$sizes = @(
    @("Square44x44Logo.png", 44, 44, 0.0),
    @("Square150x150Logo.png", 150, 150, 0.12),
    @("Wide310x150Logo.png", 310, 150, 0.12),
    @("StoreLogo.png", 50, 50, 0.0)
)
foreach ($size in $sizes) {
    $name, $width, $height, $margin = $size
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $side = [Math]::Min($width, $height)
    $inner = [int]($side * (1.0 - 2.0 * $margin))
    $x = [int](($width - $inner) / 2)
    $y = [int](($height - $inner) / 2)
    $graphics.DrawImage($logo, $x, $y, $inner, $inner)
    $bitmap.Save((Join-Path $assets $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}
$logo.Dispose()
Write-Output "Wrote $($sizes.Count) images to $assets"
