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
    [string]$Target = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ArtefactsRoot)) {
    $ArtefactsRoot = Join-Path $PSScriptRoot '..\build-release\src\opcoda_plugin\Opcoda_artefacts'
}

# O preset release gera em <config>/VST3, e o config vem do CMAKE_BUILD_TYPE.
# Descobrir por glob evita o caminho fixo errar de novo quando o build type
# mudar, que ja foi a causa do plugin nao carregar.
$bundles = @(Get-ChildItem -Path $ArtefactsRoot -Filter 'Opcoda.vst3' -Directory -Recurse)
if ($bundles.Count -eq 0) {
    throw "nenhum Opcoda.vst3 em $ArtefactsRoot. Rode antes: .\tools\build.ps1 -Release"
}
if ($bundles.Count -gt 1) {
    throw "mais de um bundle em $ArtefactsRoot : $($bundles.FullName -join ', ')"
}

$source = $bundles[0].FullName
$binary = Join-Path $source 'Contents\x86_64-win\Opcoda.vst3'
if (-not (Test-Path -LiteralPath $binary)) {
    throw "bundle invalido, faltando o binario: $binary"
}

$destination = Join-Path $Target 'Opcoda.vst3'

if ($PSCmdlet.ShouldProcess($destination, 'Instalar Opcoda.vst3')) {
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force

    # O .ilk e residuo do link incremental e chega a 89 MB. Nao faz parte do
    # bundle e nao tem funcao nenhuma no host.
    Get-ChildItem -Path $destination -Include '*.ilk', '*.pdb' -File -Recurse |
        Remove-Item -Force
}

$report = Get-ChildItem -Recurse -LiteralPath $destination |
    ForEach-Object { '{0,10:N0}  {1}' -f $_.Length, $_.FullName.Replace($destination, '') }
$report
"instalado em $destination"