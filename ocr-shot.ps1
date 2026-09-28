param([string]$Path, [string]$Lang = "")
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Media.Ocr.OcrEngine,Windows.Media.Ocr,ContentType=WindowsRuntime]
$null = [Windows.Graphics.Imaging.BitmapDecoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime]
$null = [Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime]

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() |
    Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
                   $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    $netTask.Result
}

if ($Lang -ne "") {
    $langs = [Windows.Media.Ocr.OcrEngine]::AvailableRecognizerLanguages
    $chosen = $langs | Where-Object { $_.LanguageTag -like "$Lang*" } | Select-Object -First 1
    if ($null -eq $chosen) {
        Write-Output "AVAILABLE_LANGS: $($langs.LanguageTag -join ', ')"
        exit 1
    }
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage($chosen)
} else {
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
}
if ($null -eq $engine) { Write-Output "OCR_ENGINE_NULL"; exit 1 }

$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($Path)) ([Windows.Storage.StorageFile])
$stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
$decoder = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
$bitmap = Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
if ($null -eq $bitmap) { Write-Output "BITMAP_NULL"; exit 1 }
if ($bitmap.BitmapPixelFormat -ne [Windows.Graphics.Imaging.BitmapPixelFormat]::Bgra8) {
    $bitmap = [Windows.Graphics.Imaging.SoftwareBitmap]::Convert($bitmap, [Windows.Graphics.Imaging.BitmapPixelFormat]::Bgra8)
}
Write-Output "IMG_SIZE $($decoder.PixelWidth)x$($decoder.PixelHeight) LANG $($engine.RecognizerLanguage.LanguageTag)"
$result = Await ($engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult])
Write-Output "---LINES---"
foreach ($line in $result.Lines) {
    $words = @($line.Words | ForEach-Object { $_.Text })
    $txt = $words -join " "
    $rects = @()
    foreach ($w in @($line.Words)) {
        $br = $w.BoundingRect
        $rects += ("(" + [math]::Round($br.X) + "," + [math]::Round($br.Y) + " " + [math]::Round($br.Width) + "x" + [math]::Round($br.Height) + ")")
    }
    Write-Output ($txt + "  @@  " + ($rects -join " "))
}
Write-Output "---END---"
