# Renderiza docs/atlas/diagrams/*.mmd para SVG e PNG.
#
# O SVG e' o registo visual estavel que vai para o repositorio e que o GitHub
# mostra. O PNG serve para a inspeccao visual, a que se le com a ferramenta de
# leitura antes de fechar o diagrama.
#
# Nao ha Node nem Java. O motor e' o mermaid.min.js fixado e o navegador e' o
# Edge ou o Chrome em modo headless.
#
# O mermaid.min.js nao vai para o repositorio: e' de 2,5 MB e a skill do
# Catlas mantem-no de fora de proposito. A versao e' fixada aqui e o SHA-256
# fica escrito ao lado do ficheiro, para o build ser reproduzivel.
#
# Uso: powershell -File tools/render-mermaid.ps1 -Atlas docs/atlas
param(
  [string]$Atlas = "docs/atlas",
  [string]$MermaidJs = ".opencode/skills/catlas/third_party/mermaid.min.js",
  [string]$MermaidVersion = "11.4.1",
  [string]$MermaidSha256 = "a43bc1afd446f9c4cc66ac5dd45d02e8d65e26fc5344ec0ef787f88d6ddb6f9e",
  [string]$Browser = ""
)

$ErrorActionPreference = "Stop"

function Find-Browser {
  $candidates = @(
    "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    "C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "C:\Program Files\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
    "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe"
  )
  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) { return $candidate }
  }
  # O EdgeCore fica em-versao no perfil do utilizador. O glob e' a unica forma
  # de o apanhar, porque o numero de versao muda a cada actualizacao.
  $core = Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\EdgeCore" -Filter "msedge.exe" `
    -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($core) { return $core.FullName }
  return ""
}

function Get-MermaidHash {
  param([string]$Path)
  if (Test-Path "$Path.sha256") {
    return (Get-Content "$Path.sha256" -Raw).Trim().Split(" ")[0]
  }
  return ""
}

function Install-Mermaid {
  param([string]$Path, [string]$Version, [string]$Expected)
  $url = "https://cdn.jsdelivr.net/npm/mermaid@$Version/dist/mermaid.min.js"
  $target = Split-Path -Parent $Path
  New-Item -ItemType Directory -Force -Path $target | Out-Null
  Write-Host "A obter mermaid $Version de $url" -ForegroundColor Yellow
  Invoke-WebRequest -Uri $url -OutFile $Path -UseBasicParsing -TimeoutSec 120
  $actual = (Get-FileHash -Path $Path -Algorithm SHA256).Hash.ToLower()
  if ($Expected -and $actual -ne $Expected.ToLower()) {
    throw "SHA-256 do mermaid fixado nao bate: esperado $Expected, obtido $actual"
  }
  "$actual  mermaid.min.js (mermaid $Version)" | Out-File -Encoding ascii "$Path.sha256"
}

$diagDir = Join-Path $Atlas "diagrams"
$outDir = Join-Path $Atlas "assets/render"
if (-not (Test-Path $diagDir)) { throw "pasta de diagramas inexistente: $diagDir" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$files = Get-ChildItem -Path $diagDir -Filter *.mmd -ErrorAction Stop
if ($files.Count -eq 0) { throw "nenhum .mmd em $diagDir" }

if (-not (Test-Path $MermaidJs)) {
  Write-Warning "mermaid.min.js ausente em $MermaidJs. A obter a versao fixada $MermaidVersion."
  Install-Mermaid -Path $MermaidJs -Version $MermaidVersion -Expected $MermaidSha256
}
$recorded = Get-MermaidHash -Path $MermaidJs
if ($recorded) { Write-Host "mermaid fixado: $recorded" }

# O caminho do script tem de ser absoluto: um file:///.opencode/... aponta
# para a raiz da drive e o navegador nao o encontra.
$mermaidPath = (Resolve-Path $MermaidJs).Path
$mermaidUri = "file:///" + ($mermaidPath -replace '\\', '/')

if (-not $Browser) { $Browser = Find-Browser }
if (-not $Browser) {
  throw "nenhum Edge ou Chrome encontrado. Instale um dos dois ou passe -Browser."
}
Write-Host "Navegador: $Browser"

# O navegador escreve no canal de erro mesmo quando sai bem, e o PowerShell
# com ErrorActionPreference Stop transforma isso num erro terminante. Durante
# a chamada a preferencia fica em Continue, e o codigo de saida e' que decide.
# Cada corrida usa um perfil proprio. Sem isto, o Chrome encontra uma sessao
# ja aberta e passa-lhe os argumentos em vez de correr em headless, e o
# resultado e' uma pagina em branco repetida em todos os diagramas.
$profileDir = Join-Path ([System.IO.Path]::GetTempPath()) ("opcoda-atlas-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $profileDir | Out-Null

function Invoke-Browser {
  param([string[]]$Arguments)
  $previous = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  # O perfil tem de vir antes do URL: o Chrome deixa de ler opcoes depois do
  # primeiro argumento que nao seja opcao, e o perfil ficava de fora.
  $all = @("--user-data-dir=$profileDir") + $Arguments
  $output = & $Browser @all 2>&1 | Out-String
  $code = $LASTEXITCODE
  $ErrorActionPreference = $previous
  return @{ Output = $output; Code = $code }
}

foreach ($f in $files) {
  $src = Get-Content $f.FullName -Raw
  $esc = $src -replace '</', '<\/'
  # O codigo vai num script de tipo texto: fica fora do DOM ja desenhado, e
  # as tags HTML nao sao interpretadas pelo Mermaid.
  #
  # O tema e' o 'base' com a paleta do atlas: caixas amarelas com contorno
  # preto, texto preto a negrito e sem serifa. O GitHub desenha o .mmd com o
  # tema dele, e por isso o SVG versionado e' o registo visual fiel.
  $html = @"
<!doctype html><meta charset="utf-8"><body style="margin:0;background:#ffffff">
<style>
  /* O Mermaid nao passa o peso da letra, e o titulo do grupo sai com a cor
     do tema. A folha de estilo poem tudo a preto, a negrito e em sem
     serifa, que e' o que o TikZ faz do outro lado. */
  #diagrama svg text {
    font-family: Arial, Helvetica, sans-serif !important;
    font-weight: 700 !important;
    fill: #000000 !important;
  }
  #diagrama svg .edgeLabel, #diagrama svg .label, #diagrama svg .label text {
    fill: #000000 !important;
  }
</style>
<script type="text/plain" id="fonte">$esc</script>
<div id="diagrama"></div>
<script src="$mermaidUri"></script>
<script>
(async function () {
  mermaid.initialize({
    theme: 'base',
    themeVariables: {
      primaryColor: '#FFD400',
      primaryTextColor: '#000000',
      primaryBorderColor: '#000000',
      lineColor: '#000000',
      secondaryColor: '#FFB000',
      tertiaryColor: '#FFB000',
      clusterBkg: '#FFB000',
      clusterBorder: '#000000',
      edgeLabelBackground: '#ffffff',
      fontFamily: 'Arial, Helvetica, sans-serif',
      fontSize: '15px'
    },
    flowchart: { useMaxWidth: false, htmlLabels: false, curve: 'linear' },
    sequence: { useMaxWidth: false, fontSize: '15px' }
  });
  const fonte = document.getElementById('fonte').textContent;
  try {
    const { svg } = await mermaid.render('g', fonte);
    document.getElementById('diagrama').innerHTML = svg;
    document.title = 'pronto';
  } catch (erro) {
    document.getElementById('diagrama').textContent = 'ERRO: ' + erro.message;
    document.title = 'erro';
  }
})();
</script>
"@
  $tmp = Join-Path $outDir ($f.BaseName + ".html")
  $html | Out-File -Encoding utf8 $tmp
  # O URL tem de ser absoluto. Um file:///docs/... aponta para a raiz da
  # drive e o navegador da ERR_FILE_NOT_FOUND.
  $pageUri = "file:///" + ((Resolve-Path $tmp).Path -replace '\\', '/')

  # O SVG sai do DOM depois de o Mermaid desenhar. Com --dump-dom o navegador
  # imprime o DOM ja com o <svg> no lugar do vazio. Os scripts sao removidos
  # antes da busca, senao o texto minificado da biblioteca entra na captura.
  $dump = Invoke-Browser @("--headless", "--disable-gpu", "--no-sandbox",
    "--virtual-time-budget=8000", "--dump-dom", $pageUri)
  $clean = [regex]::Replace($dump.Output, '(?s)<script\b.*?</script>', '')
  $svg = [regex]::Match($clean, '(?s)<svg\b.*?</svg>')
  $svgPath = Join-Path $outDir ($f.BaseName + ".svg")
  $width = 1600
  $height = 1000
  if ($svg.Success -and $svg.Value.Length -gt 500) {
    # O SVG e' guardado sozinho, sem a folha de estilo da pagina. A tipografia
    # tem de entrar no ficheiro, senao o SVG versionado sai com o texto
    # normal e com o titulo do grupo na cor do tema.
    $style = '<style>text{font-family:Arial,Helvetica,sans-serif !important;font-weight:700 !important;fill:#000000 !important}.edgeLabel rect{fill:#ffffff;stroke:none}</style>'
    $markup = $svg.Value
    $cut = $markup.IndexOf('>')
    if ($cut -gt 0) {
      $markup = $markup.Substring(0, $cut + 1) + $style + $markup.Substring($cut + 1)
    }
    $markup | Out-File -Encoding utf8 $svgPath
    # A janela do screenshot segue a dimensao do desenho, senao o PNG fica
    # com meia pagina em branco por baixo.
    $viewBox = [regex]::Match($svg.Value, 'viewBox="([\d.\-]+)\s+([\d.\-]+)\s+([\d.]+)\s+([\d.]+)"')
    if ($viewBox.Success) {
      $width = [Math]::Ceiling([double]$viewBox.Groups[3].Value) + 32
      $height = [Math]::Ceiling([double]$viewBox.Groups[4].Value) + 32
    }
  } else {
    Write-Warning "$($f.Name): o navegador nao devolveu SVG utilizavel. O PNG ainda sai."
  }

  # O PNG e' para a inspeccao visual. O tamanho da janela e' fixo para que
  # duas execucoes produzam imagens comparaveis. O caminho vai com barras
  # normais: o Chrome nao escreve num caminho com barras invertidas.
  $png = Join-Path $outDir ($f.BaseName + ".png")
  $pngUri = (Resolve-Path (Split-Path -Parent $png)).Path + "/" + (Split-Path -Leaf $png)
  $pngUri = $pngUri -replace '\\', '/'
  Invoke-Browser @("--headless", "--disable-gpu", "--no-sandbox",
    "--virtual-time-budget=8000", "--screenshot=$pngUri", "--window-size=$width,$height",
    $pageUri) | Out-Null

  if (-not (Test-Path $png)) {
    throw "o navegador nao escreveu $pngUri"
  }
  Write-Host "$($f.Name) -> $(Split-Path -Leaf $svgPath), $(Split-Path -Leaf $png)"
}

Write-Host "pronto: $($files.Count) diagramas em $outDir"
Remove-Item -Recurse -Force $profileDir -ErrorAction SilentlyContinue
