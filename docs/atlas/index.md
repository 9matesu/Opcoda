# Indice do atlas

Raiz do repositorio `I:/TG_I/Opcoda`, commit `4cac7a5`, 19 de outubro de
2026. O codigo do produto e' o mesmo do commit `c6409bb`: o `4cac7a5` traz
so o pipeline do atlas.

As linhas citadas neste atlas são as do código versionado. A árvore de
trabalho tem trabalho por terminar na funcionalidade F018, e esse código
ainda não está versionado, por isso as suas linhas não valem aqui.

Pastas cobertas: `src/opcoda_core`, `src/opcoda_plugin`, `tests`, `tools`.

Pastas ignoradas: `.specify` e `.claude`, que sao guias de processo e nao
codigo do produto; `docs`, que e' o que este atlas mapeia; `specs`, cujas
especificacoes de funcionalidade ja estao escritas.

Renderizador: Mermaid `11.4.1`, fixado com SHA-256
`a43bc1afd446f9c4cc66ac5dd45d02e8d65e26fc5344ec0ef787f88d6ddb6f9e`, com
Chrome headless. A figura LaTeX e' gerada por `tools/mmd2tikz.py` a partir
do mesmo ficheiro `.mmd`.

A paleta e' a mesma nas duas saidas: caixa amarela com contorno preto,
texto preto a negrito, e um amarelo mais escuro nos grupos. A tipografia e'
sem serifa, e as caixas de uma mesma linha ficam com a mesma largura, para
a grelha fechar.

## Mapa

* `01-inventory.md`: os ficheiros por papel. Comece aqui quando precisar de
  saber onde uma coisa mora.
* `02-architecture.md`: contexto e contentores. Comece aqui quando precisar
  da forma.
* `03-dataflows.md`: o movimento de dados em nivel 0 e em nivel 1. Comece
  aqui quando precisar do que se mexe e para onde vai.
* `04-entries-exits.md`: entradas, saidas, erros e o caminho quente. Comece
  aqui quando depurar entrada ou saida.

O mesmo conteudo em LaTeX esta em `arquitetura.pdf`, gerado por
`tools/build-atlas.ps1`.

## Como se regenera

```powershell
powershell -File tools/build-atlas.ps1
```

O script passa pelo lint de texto da skill do Catlas, gera o TikZ a partir
dos `.mmd`, renderiza o SVG e o PNG com o navegador, e constroi o PDF. O
`.mmd` e' a fonte da verdade: o `.tex` e' gerado e nunca se edita a mao.

## Perguntas em aberto

* `src/opcoda_core/dsp/granular_engine.cpp:492`: a telemetria dos graos e'
  publicada no fim do bloco, e o editor le-a a 60 Hz. Um bloco pode ser
  substituido por outro entre a leitura do `count` e a leitura dos campos.
  O `count` resolve a consistencia, e o resto e' decorativo por desenho.
  Confirmado no comentario em `granular_engine.h:94`, sem ensaio, porque
  a corrida nao e' reproduzivel a pedido.
* `src/opcoda_plugin/source/plugin_processor.cpp:637`: o estado do host
  entrega um blob XML e o processor aceita o que reconhece e ignora o
  resto. Nao ha forma de distinguir um estado corrompido de um estado de
  outra versao.
* `docs/16-arquitetura-implementacao.md`: descreve a arquitetura em prosa.
  Onde o texto e o codigo divergem, o codigo manda, e este atlas da a
  linha exacta.
