param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# scan screen y 300-400, x 330-780 for dark rows
for ($sy = 300; $sy -le 400; $sy += 4) {
    $dark = 0
    for ($sx = 340; $sx -le 760; $sx += 6) {
        $cx = $sx - 300; $cy = $sy - 30
        if ($cx -ge 0 -and $cx -lt $bmp.Width -and $cy -ge 0 -and $cy -lt $bmp.Height) {
            $c = $bmp.GetPixel($cx, $cy)
            if (($c.R + $c.G + $c.B) -lt 250) { $dark++ }
        }
    }
    if ($dark -gt 0) { Write-Output ("y=$sy dark=$dark") }
}
$bmp.Dispose(); $img.Dispose()
