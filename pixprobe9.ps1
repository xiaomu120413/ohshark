param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# menu at physical (322,46)-(794,370); crop starts (280,0) -> crop (42,46)-(514,370)
$dark = 0; $light = 0; $other = 0
for ($cy = 50; $cy -le 365; $cy += 5) {
    for ($cx = 50; $cx -le 500; $cx += 10) {
        if ($cx -lt $bmp.Width -and $cy -lt $bmp.Height) {
            $c = $bmp.GetPixel($cx, $cy)
            $sum = $c.R + $c.G + $c.B
            if ($sum -lt 250) { $dark++ } elseif ($sum -gt 750) { $light++ } else { $other++ }
        }
    }
}
Write-Output "menu area: dark=$dark light=$light other=$other"
foreach ($p in @(@(150,100),@(300,150),@(200,250))) {
    $c = $bmp.GetPixel($p[0], $p[1])
    Write-Output ("crop({0},{1}) R={2} G={3} B={4}" -f $p[0],$p[1],$c.R,$c.G,$c.B)
}
$bmp.Dispose(); $img.Dispose()
