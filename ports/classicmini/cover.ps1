# Name:        cover.ps1
# Description: makes the cover of the .clvg from a picture, in build/classicmini-cover (not in the repository:
#              the cover of a game is its publisher's)
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause
param([Parameter(Mandatory = $true)][string]$Source)
Add-Type -AssemblyName System.Drawing
$out = Join-Path $PSScriptRoot '..\..\build\classicmini-cover'
New-Item -ItemType Directory -Force $out | Out-Null
$image = [System.Drawing.Image]::FromFile($Source)
foreach ($size in @(@(204, 'CLV-G-ORUNR.png'), @(40, 'CLV-G-ORUNR_small.png'))) {
    $bmp = New-Object System.Drawing.Bitmap $size[0], $size[0]
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($image, 0, 0, $size[0], $size[0])
    $g.Dispose()
    $bmp.Save((Join-Path $out $size[1]), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
$image.Dispose()
