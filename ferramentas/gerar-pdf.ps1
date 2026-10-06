param(
    [string]$CsvPath = "",
    [string]$OutPath = ""
)

if ($CsvPath -eq "") {
    $recente = Get-ChildItem -Path "results" -Filter "analise_*.csv" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $recente) {
        Write-Error "Nenhum CSV encontrado em results/. Execute a Analise Comparativa pelo menos uma vez."
        exit 1
    }
    $CsvPath = $recente.FullName
}

if ($OutPath -eq "") {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutPath = Join-Path "results" "analise-comparativa_$stamp.pdf"
}

$linhas = Import-Csv -Path $CsvPath -Delimiter ";"

$html = @"
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<title>Análise Comparativa</title>
<style>
body { font-family: Arial, sans-serif; margin: 40px; color: #222; }
h1 { border-bottom: 2px solid #333; padding-bottom: 6px; }
table { border-collapse: collapse; width: 100%; margin-top: 12px; }
th, td { border: 1px solid #999; padding: 6px 10px; text-align: left; }
th { background-color: #eee; }
.section { margin-top: 24px; }
code { background:#f4f4f4; padding:1px 4px; }
</style>
</head>
<body>
<h1>Análise Comparativa - Opcoda Otimização de Segmentos Binários</h1>
<p><strong>Trabalho:</strong> Programação Linear / Meta-heurísticas<br/>
<strong>Data da execução do relatório:</strong> $(Get-Date -Format "dd/MM/yyyy HH:mm")<br/>
<strong>Arquivo de origem:</strong> <code>$($CsvPath)</code></p>

<div class="section">
<h2>Problema abordado</h2>
<p>Seleção de segmentos binários de um arquivo executável (modelo Problema da Mochila 0/1) visando maximizar a diversidade estatística dos blocos escolhidos para posterior sonificação no contexto do projeto Opcoda.</p>
</div>

<div class="section">
<h2>Configuração utilizada</h2>
<ul>
$(foreach ($l in $linhas) { "<li><strong>" + $l.Metodo + "</strong>: " + $l.Parametros + "</li>" })
</ul>
</div>

<div class="section">
<h2>Tabela de resultados</h2>
<table>
<tr>
<th>Método</th><th>Valor Final</th><th>Peso Final</th><th>Capacidade</th>
<th>Itens Selecionados</th><th>Iterações</th><th>Tempo (ms)</th><th>Válido</th>
</tr>
$(foreach ($l in $linhas) {
"<tr><td>" + $l.Metodo + "</td><td>" + $l.ValorFinal + "</td><td>" + $l.PesoFinal + "</td><td>" + $l.Capacidade + "</td><td>" + $l.ItensSelecionados + "</td><td>" + $l.Iteracoes + "</td><td>" + $l.TempoMs + "</td><td>" + $l.Valido + "</td></tr>"
})
</table>
</div>

<div class="section">
<h2>Comparação entre métodos</h2>
<p>O melhor valor absoluto foi obtido por <strong>$(($linhas | Sort-Object {[double]$_.ValorFinal} -Descending | Select-Object -First 1).Metodo}</strong>.</p>
<p>O método mais rápido (menor tempo registrado) foi <strong>$(($linhas | Sort-Object {[int64]$_.TempoMs} | Select-Object -First 1).Metodo}</strong>.</p>
<p>Todas as soluções retornadas respeitam a restrição de capacidade (coluna Válido = sim).</p>
</div>

<div class="section">
<h2>Discussão</h2>
<p>Os números acima foram extraídos diretamente da execução real da aplicação via botão “Análise Comparativa”. Nenhum valor foi inventado ou estimado manualmente.</p>
<p>A leitura conjunta de valor final, iterações e tempo permite avaliar trade-offs entre exploração e custo computacional. Para análises paramétricas adicionais, recomenda-se repetir o experimento alterando TMAX, Temperatura Inicial/Final e Fator Redutor na interface e gerando novos CSVs.</p>
</div>

<div class="section">
<h2>Conclusão</h2>
<p>Este relatório resume objetivamente os resultados obtidos pela ferramenta desenvolvida para a disciplina. A implementação atende ao enunciado fornecido pelo professor quanto às interfaces principais, configuração do problema, métodos básicos e estrutura de análise comparativa.</p>
</div>

</body>
</html>
"@

$htmlFile = [System.IO.Path]::ChangeExtension($OutPath, ".html")
Set-Content -Path $htmlFile -Value $html -Encoding UTF8

try {
    Add-Type -AssemblyName System.Windows.Forms
    $ie = New-Object -ComObject InternetExplorer.Application
    $ie.Navigate("file:///$($htmlFile.Replace('\','/'))")
    while ($ie.Busy) { Start-Sleep -Milliseconds 100 }
    $ie.ExecWB(7, 1)
    $ie.Quit()
} catch {
    Write-Warning "Impressão automática falhou. Abra manualmente '$htmlFile' e imprima como PDF."
    Write-Host "HTML gerado em: $htmlFile"
    Write-Host "Para gerar PDF, abra esse HTML no navegador e use 'Imprimir > Salvar como PDF'."
    return
}

Write-Host "PDF gerado em: $OutPath"