param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# screen coords: menu at (322,46)-(794,370); crop starts (300,30) -> crop=(screen-300, screen-30)
# probe a vertical line at screen x=500 (crop x=200) down the menu
for ($sy = 50; $sy -le 380; $sy += 20) {
    $cx = 500 - 300; $cy = $sy - 30
    if ($cx -ge 0 -and $cx -lt $bmp.Width -and $cy -ge 0 -and $cy -lt $bmp.Height) {
        $c = $bmp.GetPixel($cx, $cy)
        Write-Output ("screen(500,$sy) R={0} G={1} B={2}" -f $c.R, $c.G, $c.B)
    }
}
$bmp.Dispose(); $img.Dispose()
