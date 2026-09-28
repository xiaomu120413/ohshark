param([string]$Path)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Path)
$bmp = New-Object System.Drawing.Bitmap($img)
# scan title bar right side for button glyphs (light pixels in dark bar)
for ($y = 10; $y -le 70; $y += 15) {
    $line = ""
    for ($x = 500; $x -le 700; $x += 20) {
        $c = $bmp.GetPixel($x, $y)
        $sum = $c.R + $c.G + $c.B
        if ($sum -gt 300) { $line += "B" } else { $line += "." }
    }
    Write-Output "y=$y $line"
}
$bmp.Dispose(); $img.Dispose()
