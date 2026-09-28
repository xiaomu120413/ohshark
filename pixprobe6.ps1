param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# menu screen rect (322,46)-(794,370); crop (300,30) -> crop (22,16)-(494,340)
for ($sy = 50; $sy -le 370; $sy += 25) {
    $dark = 0; $light = 0
    for ($sx = 340; $sx -le 760; $sx += 6) {
        $cx = $sx - 300; $cy = $sy - 30
        if ($cx -ge 0 -and $cx -lt $bmp.Width -and $cy -ge 0 -and $cy -lt $bmp.Height) {
            $c = $bmp.GetPixel($cx, $cy)
            $sum = $c.R + $c.G + $c.B
            if ($sum -lt 250) { $dark++ } elseif ($sum -gt 750) { $light++ }
        }
    }
    Write-Output ("y=$sy dark=$dark light=$light")
}
$bmp.Dispose(); $img.Dispose()
