# Captura a janela do Standalone do Opcoda numa vista.
# Uso: powershell -File tools/capture-ui.ps1 -ShotName 02-hex
# Requer: build-release pronto para a vista desejada. Nao commitar as
# capturas (vao para I:/temp/).
#
# **Sem input sintetico.** Cliques e teclas via user32 nao chegam a janela
# nesta maquina (PLAY nao mudou de estado, abas nao trocaram — provado em 4
# rodadas). Cada vista e' capturada com o seu proprio processo, com o modo
# inicial trocado temporariamente em byte_display.h e revertido depois.
param(
    [string]$ShotName = "shot",
    [string]$OutDir = "I:/temp/opencode/ui-shots",
    [string]$Binary = "",
    [int]$Width = 0,
    [int]$Height = 0,
    [int]$PosX = 60,
    [int]$PosY = 60,
    [int]$SettleMs = 1200
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
  public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

if ($Binary -eq "") {
    $cands = Get-ChildItem "build-release" -Recurse -Filter "Opcoda.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "Standalone" } | Select-Object -First 1
    if ($null -eq $cands) { throw "Opcoda.exe do Standalone nao encontrado; rode build.ps1 -Release" }
    $Binary = $cands.FullName
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$p = Start-Process -FilePath $Binary -PassThru
# Espera o handle da janela do processo lancado, nao de qualquer Opcoda:
# um zumbi do taskkill anterior ainda aparece no Get-Process com rect
# velho, e a captura saia de uma janela de fundo (um PDF, uma vez).
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40 -and $h -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 500
    $proc = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    if ($null -ne $proc) { $h = $proc.MainWindowHandle }
}
if ($h -eq [IntPtr]::Zero) { throw "janela nao abriu" }

# O handle aparece antes do primeiro paint do editor. Sem esta espera a captura
# de um arranque sem material apanha o fundo cinzento por pintar e os filhos ja'
# desenhados; com material o restauro atrasa o primeiro frame e o defeito nao
# aparecia.
Start-Sleep -Milliseconds $SettleMs

if ($Width -gt 0 -and $Height -gt 0) {
    [Win32]::MoveWindow($h, $PosX, $PosY, $Width, $Height, $true) | Out-Null
    Start-Sleep -Milliseconds 900
}

$r = New-Object Win32+RECT
[Win32]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
# PrintWindow com PW_RENDERFULLCONTENT: pinta a janela directamente no bitmap,
# esteja ela em frente ou atras. O CopyFromScreen fotografava o que estivesse
# por cima (o IDE, o Chrome) porque SetForegroundWindow e' vetado quando outro
# processo tem o foco. O flag 2 e' o que obriga janelas aceleradas (GL/DX) a
# repintar no bitmap.
$bmp = New-Object System.Drawing.Bitmap($w, $hh)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $gfx.GetHdc()
$captured = [Win32]::PrintWindow($h, $hdc, 2)
$gfx.ReleaseHdc($hdc)
if (-not $captured) {
    $gfx.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
}
$path = Join-Path $OutDir ($ShotName + ".png")
$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose(); $bmp.Dispose()
Write-Output "ok: $path (${w}x${hh}) printwindow=$captured"

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
# Stop-Process nem sempre mata o Standalone (segura o device de audio);
# taskkill e' o que fecha de verdade. Sem isto o proximo link falha com o
# .exe trancado (U1077 0xffffffff). O 2>&1 engole o "nao encontrado" quando o
# Stop-Process ja fez o trabalho; sem ele, $ErrorActionPreference = Stop
# transformava o exit code do taskkill em erro fatal do script.
Start-Sleep -Milliseconds 500
$prevErr = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& taskkill /F /PID $p.Id 2>&1 | Out-Null
$ErrorActionPreference = $prevErr
Write-Output "fim"
