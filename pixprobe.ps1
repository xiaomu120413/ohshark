param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
$points = @(
    @(100,300), @(100,1000), @(500,100), @(500,200),
    @(1500,800), @(2500,300), @(1550,1030), @(1560,1000)
)
foreach ($p in $points) {
    $x = $p[0]; $y = $p[1]
    if ($x -lt $bmp.Width -and $y -lt $bmp.Height) {
        $c = $bmp.GetPixel($x, $y)
        Write-Output ("({0},{1}) R={2} G={3} B={4}" -f $x, $y, $c.R, $c.G, $c.B)
    }
}
$bmp.Dispose(); $img.Dispose()
