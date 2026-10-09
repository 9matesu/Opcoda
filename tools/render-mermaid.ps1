# Renders docs/atlas/diagrams/*.mmd to SVG plus PNG using local Edge.
# No Node, no Java. Vendored mermaid.min.js plus headless Edge.
# Usage: powershell -File tools/render-mermaid.ps1 -Atlas docs/atlas
param(
  [string]$Atlas = "docs/atlas",
  [string]$MermaidJs = ".opencode/skills/catlas/third_party/mermaid.min.js",
  [string]$Edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
)
$ErrorActionPreference = "Stop"
$diagDir = Join-Path $Atlas "diagrams"
$outDir = Join-Path $Atlas "assets/render"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$files = Get-ChildItem -Path $diagDir -Filter *.mmd -ErrorAction Stop
if ($files.Count -eq 0) { throw "no .mmd files in $diagDir" }
if (!(Test-Path $MermaidJs)) {
  Write-Warning "vendored mermaid js missing at $MermaidJs. Place mermaid.min.js there and rerun for visual QA."
  Write-Warning "Text lint still runs: python .opencode/skills/catlas/scripts/mermaid-lint.py $diagDir"
  exit 2
}
if (!(Test-Path $Edge)) {
  $cand = Get-Command msedge -ErrorAction SilentlyContinue
  if ($cand) { $Edge = $cand.Source } else { throw "Edge not found. Install Edge or pass -Edge." }
}
foreach ($f in $files) {
  $src = Get-Content $f.FullName -Raw
  $esc = $src -replace '</', '<\/'
  $html = @"
<!doctype html><meta charset="utf-8"><body>
<pre class="mermaid">$esc</pre>
<script src="file:///$($MermaidJs -replace '\\','/')"></script>
<script>mermaid.initialize({startOnLoad:true, theme:'neutral'});</script>
"@
  $tmp = Join-Path $outDir ($f.BaseName + ".html")
  $html | Out-File -Encoding utf8 $tmp
  $png = Join-Path $outDir ($f.BaseName + ".png")
  & $Edge --headless --disable-gpu --screenshot="$png" --window-size=1600,1000 "file:///$($tmp -replace '\\','/')" | Out-Null
  Write-Output "$($f.Name) -> $png"
}
Write-Output "done: $($files.Count) renders in $outDir"
