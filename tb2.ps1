param([string]$Path)
Add-Type -Assembly System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
Write-Output "img $($bmp.Width)x$($bmp.Height)"
for ($y = 5; $y -le 80; $y += 12) {
    $line = ""
    for ($x = 0; $x -lt 720; $x += 30) {
        $c = $bmp.GetPixel($x, $y)
        $sum = $c.R + $c.G + $c.B
        if ($sum -gt 300) { $line += "W" } elseif ($sum -gt 120) { $line += "g" } else { $line += "." }
    }
    Write-Output ("y={0,3} {1}" -f $y, $line)
}
$bmp.Dispose(); $img.Dispose()
