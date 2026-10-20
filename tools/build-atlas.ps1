# Pipeline do atlas: uma fonte, duas saidas.
#
# O .mmd em docs/atlas/diagrams e' a fonte da verdade. O pipeline produz
# tres coisas a partir dele:
#
#   1. o lint de texto da skill do Catlas, que barra diagramas ilegiveis;
#   2. o render SVG e PNG com Edge ou Chrome, para o GitHub e para a
#      inspeccao visual;
#   3. o TikZ, para o documento LaTeX.
#
# O passo 3 e' a parte que o Catlas nao traz: a mesma figura em Mermaid para
# o repositorio e em TikZ para o PDF, geradas do mesmo ficheiro, para nao
# haver duas verdadees.
#
# Uso:
#   powershell -File tools/build-atlas.ps1
#   powershell -File tools/build-atlas.ps1 -SkipRender
#   powershell -File tools/build-atlas.ps1 -SkipLatex
param(
  [string]$Atlas = "docs/atlas",
  [string]$Python = "python",
  [switch]$SkipRender,
  [switch]$SkipLatex,
  [switch]$SkipLint
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$diagDir = Join-Path $Atlas "diagrams"
$figDir = Join-Path $Atlas "figuras"
$texFile = Join-Path $Atlas "arquitetura.tex"

function Invoke-Step {
  param([string]$Name, [scriptblock]$Action)
  Write-Host ""
  Write-Host "==> $Name" -ForegroundColor Cyan
  & $Action
  if ($LASTEXITCODE -ne 0) {
    throw "$Name falhou com codigo $LASTEXITCODE"
  }
}

# 1. Lint de texto. Baralha a primeira, e e' o portao mais barato.
if (-not $SkipLint) {
  Invoke-Step "Lint dos diagramas" {
    & $Python ".opencode/skills/catlas/scripts/mermaid-lint.py" $diagDir
  }
}

# 2. TikZ. Depende so do .mmd, por isso corre mesmo sem render.
Invoke-Step "Mermaid para TikZ" {
  & $Python "tools/mmd2tikz.py" --in $diagDir --out $figDir
}

# 3. Render SVG e PNG. Precisa de Edge ou Chrome e do mermaid.min.js fixado.
if (-not $SkipRender) {
  Invoke-Step "Render dos diagramas" {
    & powershell -File "tools/render-mermaid.ps1" -Atlas $Atlas
  }
}

# 4. Documento LaTeX. Sao duas passagens do pdflatex, e nao latexmk: o
#    latexmk do MiKTeX e' um script em Perl, e nem toda a maquina de build
#    tem Perl. O documento nao tem referencias cruzadas, por isso a segunda
#    passagem e' so por causa do indice e das paginas.
if (-not $SkipLatex) {
  Invoke-Step "Documento LaTeX" {
    Push-Location $Atlas
    try {
      $previous = $ErrorActionPreference
      $ErrorActionPreference = "Continue"
      # Aqui o directorio corrente ja e' a pasta do atlas, por isso o
      # ficheiro entra pelo nome e nao pelo caminho relativo.
      $name = Split-Path -Leaf $texFile
      foreach ($pass in 1..2) {
        & pdflatex -interaction=nonstopmode -halt-on-error $name 2>&1 |
          Select-String -Pattern "^!|Output written" | ForEach-Object { Write-Host $_.Line }
        if ($LASTEXITCODE -ne 0) {
          $ErrorActionPreference = $previous
          throw "pdflatex falhou na passagem $pass"
        }
      }
      $ErrorActionPreference = $previous
    } finally {
      Pop-Location
    }
  }
}

Write-Host ""
Write-Host "Atlas pronto em $Atlas" -ForegroundColor Green
