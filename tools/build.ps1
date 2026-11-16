<#
    Compila e testa o Opcoda. O cl.exe depende do vcvars64, que e' carregado aqui.

    .\tools\build.ps1                 # debug + ASan + testes
    .\tools\build.ps1 -Preset release # release, sem sanitizers
#>
param(
    [ValidateSet('dev', 'release')]
    [string]$Preset = 'dev',
    [switch]$Release
)

$ErrorActionPreference = 'Stop'

# A lista fixa so' cobria BuildTools. Uma maquina com o Professional (ou Community)
# instalado tem o mesmo vcvars64 e ficava de fora -- o build dizia "nao encontrado"
# com o compilador ali ao lado. O vswhere resolve isso sem adivinhar: devolve o
# installPath de cada instancia, e o vcvars mora sempre em VC\Auxiliary\Build.
$vcvarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
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
$cmakeCandidates = @(
    'C:\Program Files\CMake\bin\cmake.exe',
    'C:\Program Files (x86)\CMake\bin\cmake.exe'
) + (Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
$cmake = $cmakeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cmake) { throw "cmake.exe nao encontrado" }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'

if ($Release) { $Preset = 'release' }

if ($Preset -eq 'dev') { $Config = 'Debug' } else { $Config = 'Release' }

foreach ($exe in @($vcvars, $cmake, $ctest)) {
    if (-not (Test-Path $exe)) { throw "nao encontrado: $exe" }
}

& cmd.exe /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

& $cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { throw "configure falhou" }

& $cmake --build --preset $Preset --config $Config
if ($LASTEXITCODE -ne 0) { throw "build falhou" }

if ($Preset -eq 'release') {
    # Um plugin que linka contra o CRT de debug so carrega em maquina com o
    # Visual Studio instalado. No Ableton de outra maquina o LoadLibrary falha
    # com ERROR_MOD_NOT_FOUND, e o sintoma e o pior possivel: o host le o
    # moduleinfo.json do disco, lista o plugin, e nao consegue abrir. Foi
    # exatamente o que aconteceu com o preset release, que nao tinha
    # CMAKE_BUILD_TYPE e caia no Debug do MSVC.
    $bundle = @(Get-ChildItem -Path 'build-release\src\opcoda_plugin\Opcoda_artefacts' `
        -Filter 'Opcoda.vst3' -Directory -Recurse)
    if ($bundle.Count -eq 0) { throw "bundle do VST3 nao encontrado" }

    $binary = Join-Path $bundle[0].FullName 'Contents\x86_64-win\Opcoda.vst3'
    $debugRuntime = & dumpbin.exe /dependents $binary |
        Select-String -Pattern '\.dll' |
        ForEach-Object { $_.Line.Trim() } |
        Where-Object { $_ -match 'D\.dll$|^ucrtbased\.dll$' }

    if ($debugRuntime) {
        throw ("o VST3 linka contra o CRT de debug e nao vai carregar em um host comum:`n  " +
               ($debugRuntime -join "`n  "))
    }
    "guarda ok: o VST3 nao depende de runtime de debug"
}

if ($Preset -eq 'dev') {
    & $ctest --preset $Preset -C Debug
    if ($LASTEXITCODE -ne 0) { throw "testes falharam" }
}
