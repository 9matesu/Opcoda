# Instala o bundle do VST3 no caminho que o Ableton Live le no Windows.
# Precisa de uma sessao elevada: C:\Program Files\Common Files\VST3 nao e
# gravavel pelo usuario.
#
#   powershell -File tools\install-vst3.ps1          # pede UAC
#   powershell -File tools\install-vst3.ps1 -WhatIf  # so mostra o que faria
#
# O param e o CmdletBinding precisam ser as primeiras instrucoes do arquivo: uma
# sentenca antes do param e' erro de sintaxe, nao aviso.

[CmdletBinding(SupportsShouldProcess)]
param(
    # Default resolvido no corpo, nao aqui: em -File o $PSScriptRoot ainda nao
    # vale no momento em que os defaults sao avaliados.
    [string]$ArtefactsRoot,
    # Local onde o plugin sera copiado. Somente este caminho e' lido pelo Ableton
    # Live no Windows — caminhos por usuario (appdata) sao ignorados pelo host.
    [string]$Target = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Encontrar o bundle mais recente. O default de ArtefactsRoot e' relativo ao
# script e pode mudar conforme o tipo de build — resolver por glob em todo o
# repositorio evita essa fragilidade.
# ---------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($ArtefactsRoot)) {
    # Tentar primeiro o caminho classico do build release.
    $ArtefactsRoot = Join-Path $PSScriptRoot '..\build-release\src\opcoda_plugin\Opcoda_artefacts'
    $fallback = Join-Path $PSScriptRoot '..\..'
    $recent = @()

    $candidates = @()
    if (Test-Path -LiteralPath $ArtefactsRoot) {
        $candidates += Get-ChildItem -Path $ArtefactsRoot -Filter 'Opcoda.vst3' -Directory -Recurse -ErrorAction SilentlyContinue
    }
    # Se o path de artefactos nao existe ainda, procurar em qualquer lugar do
    # repositorio. Ordena por data de modificacao, o mais novo primeiro.
    if ($candidates.Count -eq 0) {
        $recent = (Get-ChildItem -Path $fallback -Filter 'Opcoda.vst3' -Directory -Recurse -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending)
    }
    else {
        $recent = $candidates | Sort-Object LastWriteTime -Descending
    }
}
else {
    $recent = @(Get-ChildItem -Path $ArtefactsRoot -Filter 'Opcoda.vst3' -Directory -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
}

$bundles = $recent
if ($bundles.Count -eq 0) {
    $hint = @()
    if ([string]::IsNullOrWhiteSpace($ArtefactsRoot)) {
        $hint += "Nenhum bundle encontrado no repositorio nem em $ArtefactsRoot."
    }
    else {
        $hint += "Nenhum Opcoda.vst3 encontrado em $ArtefactsRoot."
    }
    $hint += "Gera um antes: powershell -File .\tools\build.ps1 -Release"
    throw ($hint -join [System.Environment]::NewLine)
}
if ($bundles.Count -gt 1) {
    $hint = "Mais de um bundle encontrado. Usando o mais recente ({0}). Remova ou mova os outros." -f $bundles[0].FullName
    Write-Warning $hint
}

$source = $bundles[0].FullName
$binary = Join-Path $source 'Contents\x86_64-win\Opcoda.vst3'
if (-not (Test-Path -LiteralPath $binary)) {
    throw "bundle invalido, faltando o binario: $binary"
}

# Verifica se o bundle nao foi corrompido: o executavel interno deve ser um PE
# de 64 bits com cabecalho MZ valido.
$peBytes = [Byte[]]::New(2)
$handle = [System.IO.File]::Open($binary, 'Open', 'Read', 'Read')
$null = $handle.Read($peBytes, 0, 2)
$handle.Close()
if (($peBytes[0] -ne 0x4D) -or ($peBytes[1] -ne 0x5A)) {
    throw "O executavel do bundle nao e' um PE valido (cabecalho 'MZ' ausente): $binary`nEste bundle pode estar corrompido ou inacabado. Rebuild com: .\tools\build.ps1 -Release"
}

$destination = Join-Path $Target 'Opcoda.vst3'

if (-not ([System.Security.Principal.WindowsPrincipal]::New(
        [System.Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole(
        [System.Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Warning "esta sessao nao e administrativa. C:\Program Files\Common Files\VST3 precisa de UAC."
    Write-Warning "Reinicie este script com 'run as administrator' ou execute no PowerShell elevado."
    if (-not $PSCmdlet.ShouldProcess($destination, 'Instalar Opcoda.vst3')) {
        return
    }
}
else {
    if (-not $PSCmdlet.ShouldProcess($destination, 'Instalar Opcoda.vst3')) {
        return
    }
}

if (Test-Path -LiteralPath $destination) {
    Remove-Item -LiteralPath $destination -Recurse -Force
}
Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force

# O .ilk e residuo do link incremental e chega a 89 MB. Nao faz parte do
# bundle e nao tem funcao nenhuma no host.
Get-ChildItem -Path $destination -Include '*.ilk', '*.pdb' -File -Recurse |
    Remove-Item -Force

$report = Get-ChildItem -Recurse -LiteralPath $destination |
    ForEach-Object { '{0,10:N0}  {1}' -f $_.Length, $_.FullName.Replace($destination, '') }
$report
"instalado em $destination"

Write-Host "DICA: o Ableton so reconhece o VST3 instalado em C:\Program Files\Common Files\VST3." -ForegroundColor Cyan
Write-Host "Se o plugin aparecer com erro de 'event input bus', o problema e' de build (faltando IS_SYNTH + NEEDS_MIDI_INPUT) e' nao de instalacao." -ForegroundColor Cyan