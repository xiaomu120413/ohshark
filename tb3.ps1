param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
Write-Output "img $($bmp.Width)x$($bmp.Height)"
for ($y = 0; $y -lt 80; $y += 8) {
    $line = ""
    for ($x = 0; $x -lt 420; $x += 15) {
        $c = $bmp.GetPixel($x, $y)
        $sum = $c.R + $c.G + $c.B
        if ($sum -gt 600) { $line += "W" } elseif ($sum -gt 250) { $line += "g" } else { $line += "." }
    }
    Write-Output ("y={0,3} {1}" -f $y, $line)
}
$bmp.Dispose(); $img.Dispose()
