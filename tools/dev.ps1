<#
    Ciclo de desenvolvimento do Opcoda. Compila so' o alvo que se esta' a mexer e,
    sem -NoLaunch, abre o standalone. Com -Watch, recompila e reabre a cada
    mudanca em src/ ou resources/.

    .\tools\dev.ps1                 # standalone, compila e abre
    .\tools\dev.ps1 -Watch          # recompila e reabre ao gravar
    .\tools\dev.ps1 -Vst3           # so' o VST3 (para usar no host)
    .\tools\dev.ps1 -NoLaunch       # so' compila
#>
param(
    [switch]$Vst3,
    [switch]$Watch,
    [switch]$NoLaunch
)

$ErrorActionPreference = 'Stop'

$vcvarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $discovered = @(& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null) |
        Where-Object { $_ } |
        ForEach-Object { Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat' }
    $vcvarsCandidates = @($discovered) + $vcvarsCandidates
}
$vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "vcvars64.bat nao encontrado" }

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { throw "cmake.exe nao encontrado" }

if (-not (Test-Path -LiteralPath 'build-release\CMakeCache.txt')) {
    & cmd.exe /c "`"$vcvars`" >nul 2>&1 && `"$cmake`" --preset release"
    if ($LASTEXITCODE -ne 0) { throw "configure falhou" }
}

$buildPreset = if ($Vst3) { 'vst3' } else { 'standalone' }

function Invoke-Build {
    & cmd.exe /c "`"$vcvars`" >nul 2>&1 && `"$cmake`" --build --preset $buildPreset"
    if ($LASTEXITCODE -ne 0) { throw "build falhou" }
}

Invoke-Build

if ($Vst3) {
    $bundle = @(Get-ChildItem -Path 'build-release\src\opcoda_plugin\Opcoda_artefacts' `
        -Filter 'Opcoda.vst3' -Directory -Recurse)
    if ($bundle.Count -eq 0) { throw "bundle do VST3 nao encontrado" }
    "VST3: $($bundle[0].FullName)"
    return
}

$exe = Join-Path $PSScriptRoot '..\build-release\src\opcoda_plugin\Opcoda_artefacts\Release\Standalone\Opcoda.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "standalone nao encontrado: $exe" }

if ($NoLaunch) {
    "standalone pronto: $exe"
    return
}

$proc = $null
function Start-App {
    if ($proc -and -not $proc.HasExited) {
        $proc.CloseMainWindow() | Out-Null
        if (-not $proc.WaitForExit(2000)) { Stop-Process -Id $proc.Id -Force }
    }
    $script:proc = Start-Process -FilePath $exe -PassThru
}

Start-App

if (-not $Watch) {
    return
}

$srcRoot = Join-Path $PSScriptRoot '..\src'
$resRoot = Join-Path $PSScriptRoot '..\resources'
$stamp = (Get-ChildItem -Path $srcRoot, $resRoot -Recurse -File -ErrorAction SilentlyContinue |
    Measure-Object LastWriteTime -Maximum).Maximum

"vigiando src/ e resources/. Ctrl+C para parar."
while ($true) {
    Start-Sleep -Milliseconds 800

    if ($proc -and $proc.HasExited) {
        $proc = $null
    }

    $now = (Get-ChildItem -Path $srcRoot, $resRoot -Recurse -File -ErrorAction SilentlyContinue |
        Measure-Object LastWriteTime -Maximum).Maximum
    if ($now -le $stamp) { continue }

    $stamp = $now
    try {
        Invoke-Build
        Start-App
        "recompilado e reaberto"
    }
    catch {
        Write-Warning $_.Exception.Message
    }
}