# Empacota o release e compila o instalador wizard (Inno Setup 6).
#
#   powershell -File tools\build-installer.ps1            # build release + instalador
#   powershell -File tools\build-installer.ps1 -SkipBuild # so reempacota o release existente
#
# O script monta um payload limpo (sem residuos de link), calcula os pesos,
# gera a arte do wizard e chama o ISCC com versao e tamanhos como defines.

[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$Iscc
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Release
}

# ---------------------------------------------------------------------------
# ISCC: instalacao por utilizador ou de maquina.
# ---------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($Iscc)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
    )
    $Iscc = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($Iscc) -or -not (Test-Path -LiteralPath $Iscc)) {
    throw "ISCC.exe nao encontrado. Instale o Inno Setup 6 (winget install --id JRSoftware.InnoSetup -e)."
}

# ---------------------------------------------------------------------------
# Artefactos do release.
# ---------------------------------------------------------------------------
$artefacts = Join-Path $repoRoot 'build-release\src\opcoda_plugin\Opcoda_artefacts\Release'
$bundle = Join-Path $artefacts 'VST3\Opcoda.vst3'
$binary = Join-Path $bundle 'Contents\x86_64-win\Opcoda.vst3'
$exe = Join-Path $artefacts 'Standalone\Opcoda.exe'
if (-not (Test-Path -LiteralPath $binary)) {
    throw "bundle VST3 invalido (falta $binary). Rebuild com .\tools\build.ps1 -Release"
}
if (-not (Test-Path -LiteralPath $exe)) {
    throw "standalone em falta ($exe). Rebuild com .\tools\build.ps1 -Release"
}

# versao: a mesma do CMakeLists (fonte unica).
$versionMatch = Select-String -Path (Join-Path $repoRoot 'CMakeLists.txt') `
    -Pattern 'project\(Opcoda VERSION ([0-9]+(\.[0-9]+)+)'
if (-not $versionMatch) { throw 'versao nao encontrada no CMakeLists.txt' }
$version = $versionMatch.Matches[0].Groups[1].Value

# ---------------------------------------------------------------------------
# Payload limpo: copia o bundle e o standalone, sem residuos de link.
# ---------------------------------------------------------------------------
$installerDir = Join-Path $PSScriptRoot '..\installer'
$stage = Join-Path $installerDir 'stage'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'Docs') | Out-Null

Copy-Item -LiteralPath $bundle -Destination (Join-Path $stage 'Opcoda.vst3') -Recurse -Force
Get-ChildItem -LiteralPath (Join-Path $stage 'Opcoda.vst3') -Recurse -File |
    Where-Object { @('.ilk', '.pdb', '.exp', '.lib') -contains $_.Extension } |
    Remove-Item -Force
Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'Opcoda.exe')
$manual = Join-Path $repoRoot 'docs\manual\Opcoda-Manual.pdf'
if (Test-Path -LiteralPath $manual) {
    Copy-Item -LiteralPath $manual -Destination (Join-Path $stage 'Docs')
}

# ---------------------------------------------------------------------------
# Pesos: o que o utilizador ve no wizard.
# ---------------------------------------------------------------------------
$vst3Bytes = (Get-ChildItem -LiteralPath (Join-Path $stage 'Opcoda.vst3') -Recurse -File |
    Measure-Object -Property Length -Sum).Sum
$exeBytes = (Get-Item -LiteralPath (Join-Path $stage 'Opcoda.exe')).Length
$docsBytes = (Get-ChildItem -LiteralPath (Join-Path $stage 'Docs') -Recurse -File |
    Measure-Object -Property Length -Sum).Sum
$totalBytes = $vst3Bytes + $exeBytes + $docsBytes

$formatSize = {
    param([long]$bytes)
    if ($bytes -ge 1MB) { return ('{0:N1} MB' -f ($bytes / 1MB)) }
    return ('{0:N0} kB' -f ($bytes / 1KB))
}
$totalSize = & $formatSize $totalBytes
$vst3Size = & $formatSize $vst3Bytes
$standaloneSize = & $formatSize $exeBytes
$docsSize = & $formatSize $docsBytes

Write-Host ("versao   : {0}" -f $version)
Write-Host ("VST3     : {0}" -f $vst3Size)
Write-Host ("Standalone: {0}" -f $standaloneSize)
Write-Host ("Docs     : {0}" -f $docsSize)
Write-Host ("total    : {0}" -f $totalSize)

# ---------------------------------------------------------------------------
# Dependencia: Visual C++ 2015-2022 x64 (oferecido no wizard com consentimento;
# a deteccao no [Code] esconde a opcao quando o runtime ja existe).
# ---------------------------------------------------------------------------
$redistDir = Join-Path $installerDir 'redist'
$redist = Join-Path $redistDir 'vc_redist.x64.exe'
if (-not (Test-Path -LiteralPath $redist)) {
    New-Item -ItemType Directory -Force -Path $redistDir | Out-Null
    Write-Host 'a descarregar o VC++ 2015-2022 x64 redistribuivel...'
    Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile $redist -UseBasicParsing
}
Write-Host ("redist   : {0}" -f (& $formatSize (Get-Item -LiteralPath $redist).Length))

# ---------------------------------------------------------------------------
# ISCC.
# ---------------------------------------------------------------------------
$iss = Join-Path $installerDir 'opcoda.iss'
& $Iscc "/DMyAppVersion=$version" "/DTotalSize=$totalSize" "/DVst3Size=$vst3Size" `
    "/DStandaloneSize=$standaloneSize" "/DDocsSize=$docsSize" "/DPayloadDir=$stage" $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC falhou com o codigo $LASTEXITCODE" }

$out = Get-ChildItem -LiteralPath (Join-Path $installerDir 'Output') -Filter '*.exe' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Host ""
Write-Host ("instalador: {0} ({1})" -f $out.FullName, (& $formatSize $out.Length))