param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
foreach ($p in @(@(500,100),@(500,200),@(700,150),@(350,300),@(100,300),@(700,400))) {
    $x=$p[0]; $y=$p[1]
    $c = $bmp.GetPixel($x, $y)
    Write-Output ("({0},{1}) R={2} G={3} B={4}" -f $x,$y,$c.R,$c.G,$c.B)
}
$bmp.Dispose(); $img.Dispose()
