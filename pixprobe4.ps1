param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# Count dark pixels in each menu item row (screen coords, menu x 330-780)
for ($sy = 55; $sy -le 360; $sy += 12) {
    $dark = 0; $total = 0
    for ($sx = 340; $sx -le 760; $sx += 8) {
        $cx = $sx - 300; $cy = $sy - 30
        if ($cx -ge 0 -and $cx -lt $bmp.Width -and $cy -ge 0 -and $cy -lt $bmp.Height) {
            $c = $bmp.GetPixel($cx, $cy)
            $total++
            if (($c.R + $c.G + $c.B) -lt 250) { $dark++ }
        }
    }
    Write-Output ("y=$sy dark=$dark/$total")
}
$bmp.Dispose(); $img.Dispose()
