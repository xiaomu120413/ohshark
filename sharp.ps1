param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# Check for 2x2 pixel duplication (half-res upscale) across a text row
$row = 300
$prev = $null; $dups = 0; $total = 0
for ($x = 200; $x -lt 800; $x += 2) {
    $c = $bmp.GetPixel($x, $row)
    if ($prev -ne $null) {
        $total++
        if ($c.R -eq $prev.R -and $c.G -eq $prev.G -and $c.B -eq $prev.B) { $dups++ }
    }
    $prev = $bmp.GetPixel($x + 1, $row)
}
Write-Output "pixel-pair duplication at y=$row`: $dups/$total (high = upscaled/blurry)"
$bmp.Dispose(); $img.Dispose()
