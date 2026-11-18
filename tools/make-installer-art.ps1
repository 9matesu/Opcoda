# Gera a arte do instalador (wizard do Inno Setup) a partir da marca.
#
#   powershell -File tools\make-installer-art.ps1
#
# Saida: installer\WizModernImage.bmp (164x314) e WizModernSmallImage.bmp (55x55).
# A area do wizard moderno e' 164x314; uma imagem maior seria esticada.

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$iconPath = Join-Path $root 'resources\icons\opcoda-icon-256.png'
$fontPath = Join-Path $root 'resources\fonts\Switzer-Semibold.ttf'
$outDir = Join-Path $PSScriptRoot '..\installer'
if (-not (Test-Path $iconPath)) { throw "icone em falta: $iconPath" }
if (-not (Test-Path $fontPath)) { throw "fonte em falta: $fontPath" }

$pfc = [System.Drawing.Text.PrivateFontCollection]::new()
$pfc.AddFontFile($fontPath)
$family = $pfc.Families[0]
$wordmark = [System.Drawing.Font]::new($family, 20.0, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$caption = [System.Drawing.Font]::new($family, 8.0, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$icon = [System.Drawing.Image]::FromFile($iconPath)

$w = 164
$h = 314
$bmp = [System.Drawing.Bitmap]::new($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

$rect = [System.Drawing.Rectangle]::new(0, 0, $w, $h)
$c1 = [System.Drawing.Color]::FromArgb(255, 20, 20, 20)
$c2 = [System.Drawing.Color]::FromArgb(255, 8, 8, 8)
$brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new($rect, $c1, $c2, 90.0)
$g.FillRectangle($brush, $rect)

$topLine = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(46, 242, 242, 242))
$g.FillRectangle($topLine, [System.Drawing.Rectangle]::new(24, 1, ($w - 48), 1))

$iconSize = 84
$iconX = [int](($w - $iconSize) / 2)
$g.DrawImage($icon, [System.Drawing.Rectangle]::new($iconX, 40, $iconSize, $iconSize))

$ink = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 242, 242, 242))
$wordSize = $g.MeasureString("Opcoda", $wordmark)
$g.DrawString("Opcoda", $wordmark, $ink, [single](($w - $wordSize.Width) / 2.0), [single]140.0)

$accentX = [int](($w - 28) / 2)
$g.FillRectangle($ink, [System.Drawing.Rectangle]::new($accentX, 172, 28, 2))

$dim = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(150, 242, 242, 242))
$cap1 = $g.MeasureString("GRANULAR SYNTHESIZER", $caption)
$g.DrawString("GRANULAR SYNTHESIZER", $caption, $dim, [single](($w - $cap1.Width) / 2.0), [single]182.0)
$dim2 = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(110, 242, 242, 242))
$cap2 = $g.MeasureString("WINDOWS x64", $caption)
$g.DrawString("WINDOWS x64", $caption, $dim2, [single](($w - $cap2.Width) / 2.0), [single]196.0)

$g.Flush()
$g.Dispose()
$bmp.Save((Join-Path $outDir 'WizModernImage.bmp'), [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp.Dispose()

$small = [System.Drawing.Bitmap]::new(55, 55)
$g2 = [System.Drawing.Graphics]::FromImage($small)
$g2.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g2.Clear([System.Drawing.Color]::FromArgb(255, 14, 14, 14))
$g2.DrawImage($icon, [System.Drawing.Rectangle]::new(3, 3, 49, 49))
$g2.Flush()
$g2.Dispose()
$small.Save((Join-Path $outDir 'WizModernSmallImage.bmp'), [System.Drawing.Imaging.ImageFormat]::Bmp)
$small.Dispose()
$icon.Dispose()

Write-Output "arte do instalador: WizModernImage.bmp (164x314), WizModernSmallImage.bmp (55x55)"