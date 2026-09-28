param([string]$In, [string]$Out, [int]$Scale = 3)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($In)
$w = $img.Width * $Scale; $h = $img.Height * $Scale
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.DrawImage($img, 0, 0, $w, $h)
$g.Dispose(); $img.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $Out ${w}x${h}"
