# Gera os icones da aplicacao a partir de uma geometria unica.
#
# O desenho e' o material do instrumento: quadrado preto (palette::display) de
# cantos arredondados e a silhueta de onda espelhada em laranja (palette::accent)
# -- a onda que os bytes fazem, sem nenhuma outra marca. A forma da onda e' uma
# tabela fixa de 32 amplitudes que imita o que o display mostra num .exe real:
# inicio baixo, o pico do cabecalho PE, corpo denso e cauda a morrer.
#
# O mesmo array vive em src/opcoda_plugin/source/standalone/standalone_window.cpp
# (makeWindowIcon desenha a mesma geometria em JUCE para o icone da janela).
# Se mexer numa tabela, mexa na outra.
#
# Uso: powershell -File tools/make-icon.ps1

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repoRoot "resources\icons"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Add-Type -AssemblyName System.Drawing

# 32 amplitudes, 0..1. pico do cabecalho na quinta mostra, corpo de codigo denso,
# cauda de dados a morrer.
$amp = @(0.06, 0.10, 0.08, 0.14, 0.96, 0.30, 0.18, 0.24,
         0.38, 0.28, 0.44, 0.34, 0.22, 0.30, 0.52, 0.36,
         0.26, 0.42, 0.30, 0.20, 0.34, 0.24, 0.16, 0.28,
         0.18, 0.12, 0.20, 0.10, 0.16, 0.08, 0.12, 0.05)

$bg = [System.Drawing.Color]::FromArgb(255, 0x0D, 0x0C, 0x0A)
$wave = [System.Drawing.Color]::FromArgb(255, 0xD8, 0x79, 0x2F)

function Draw-Icon {
    param([int]$Size, [double]$MinAmp, [string]$Path)

    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    # chapa preta com cantos arredondados; fora dela fica transparente.
    $radius = $Size * 0.19
    $bgPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $bgPath.AddArc(0, 0, $d, $d, 180, 90)
    $bgPath.AddArc($Size - $d - 1, 0, $d, $d, 270, 90)
    $bgPath.AddArc($Size - $d - 1, $Size - $d - 1, $d, $d, 0, 90)
    $bgPath.AddArc(0, $Size - $d - 1, $d, $d, 90, 90)
    $bgPath.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush($bg)), $bgPath)

    # silhueta espelhada: borda de cima da esquerda para a direita, borda de
    # baixo de volta. amplitudes curtas sao levantadas para a onda nunca
    # desaparecer nos tamanhos pequenos.
    $marginX = $Size * 0.13
    $halfH = $Size * 0.30
    $cy = $Size * 0.5
    $step = ($Size - 2.0 * $marginX) / $amp.Count

    $points = New-Object 'System.Drawing.PointF[]' (2 * $amp.Count)
    for ($i = 0; $i -lt $amp.Count; ++$i) {
        $a = [Math]::Max($amp[$i], $MinAmp)
        $x = [float]($marginX + ($i + 0.5) * $step)
        $points[$i] = New-Object System.Drawing.PointF($x, [float]($cy - $a * $halfH))
        $points[2 * $amp.Count - 1 - $i] = New-Object System.Drawing.PointF(
            $x, [float]($cy + $a * $halfH))
    }

    $g.FillPolygon((New-Object System.Drawing.SolidBrush($wave)), $points)

    $g.Dispose()
    $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    "ok: $Path"
}

# o grande alimenta o .ico (juceaide reamostra 16/32/48/256 a partir dele) e o
# icone da janela vai por BinaryData.
Draw-Icon -Size 512 -MinAmp 0.05 -Path (Join-Path $outDir "opcoda-icon-512.png")
Draw-Icon -Size 256 -MinAmp 0.05 -Path (Join-Path $outDir "opcoda-icon-256.png")
# o pequeno e' desenhado directo: reamostrar 256 para 32 apagava o corpo baixo
# da onda. aqui o minimo sobe para a silhueta aguentar o tamanho do explorer.
Draw-Icon -Size 32 -MinAmp 0.16 -Path (Join-Path $outDir "opcoda-icon-32.png")
