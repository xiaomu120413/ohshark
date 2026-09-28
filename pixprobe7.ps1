param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# expected blue at screen (422,346); menu image blue at (100,300), menu at (322,46)
foreach ($p in @(@(422,346),@(430,350),@(410,340),@(500,300),@(400,100),@(500,150))) {
    $sx=$p[0]; $sy=$p[1]
    $cx = $sx - 300; $cy = $sy - 30
    if ($cx -ge 0 -and $cx -lt $bmp.Width -and $cy -ge 0 -and $cy -lt $bmp.Height) {
        $c = $bmp.GetPixel($cx, $cy)
        Write-Output ("screen($sx,$sy) R={0} G={1} B={2}" -f $c.R, $c.G, $c.B)
    }
}
$bmp.Dispose(); $img.Dispose()
