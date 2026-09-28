param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# menu at logical (163,25)-(399,187); window client origin ~(3,3) physical -> menu physical ~(329,53)-(801,377)
foreach ($p in @(@(500,100),@(500,200),@(600,150),@(700,300))) {
    $x=$p[0]; $y=$p[1]
    if ($x -lt $bmp.Width -and $y -lt $bmp.Height) {
        $c = $bmp.GetPixel($x, $y)
        Write-Output ("($x,$y) R={0} G={1} B={2}" -f $c.R, $c.G, $c.B)
    }
}
$bmp.Dispose(); $img.Dispose()
