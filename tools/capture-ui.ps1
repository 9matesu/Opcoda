# Captura a janela do Standalone do Opcoda numa vista e largura.
# Uso: powershell -File tools/capture-ui.ps1 -ShotName 02-hex-900 [-Width 480] [-Height 434] [-PosX 60] [-PosY 60]
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
    [int]$PosY = 60
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
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
Start-Sleep -Seconds 3

$shots = Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -match "Opcoda" }
if ($shots.Count -eq 0) { throw "janela nao abriu" }
$h = $shots[0].MainWindowHandle
if ($Width -gt 0 -and $Height -gt 0) {
    [Win32]::MoveWindow($h, $PosX, $PosY, $Width, $Height, $true) | Out-Null
    Start-Sleep -Milliseconds 900
}
[Win32]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 900

$r = New-Object Win32+RECT
[Win32]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap($w, $hh)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
$path = Join-Path $OutDir ($ShotName + ".png")
$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose(); $bmp.Dispose()
Write-Output "ok: $path (${w}x${hh})"

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Write-Output "fim"
