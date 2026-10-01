# Relatório de apoio: Opcoda, do databending ao instrumento em tempo real

Material de apoio para apresentação. Texto sucinto, com figuras de fontes oficiais e legenda no padrão ABNT. Medições e telas do Opcoda entram após o MVP de 03/11.

## 1. Databending e o problema

Databending é a leitura de um arquivo não sonoro como se fosse áudio. O Audacity oferece essa operação pelo diálogo de importação de dados brutos, em que o usuário define codificação, canais e taxa (Figura 1). O procedimento é offline e sem tratamento: bytes executáveis não têm média zero, cabeçalhos geram cliques e variações rápidas produzem aliasing. Um parser sem verificação de limites ainda pode ler fora do arquivo e derrubar a sessão.

![Figura 1: diálogo de importação de dados brutos do Audacity](https://manual.audacityteam.org/m/images/c/cd/importrawdata.png)

Fonte: Manual oficial do Audacity, página File Menu Import. Acesso em out. 2026.

## 2. Similares e diferencial

Granulator II, de Robert Henke, é a referência artística de síntese granular: fluxo constante de grãos com posição, tamanho e afinação moduláveis, além de filtros em série (Figura 2). Opera apenas sobre samples, dentro do Ableton Live com Max for Live.

![Figura 2: Granulator II, de Robert Henke](https://www.roberthenke.com/files_images/technology/granulator/granulatorII.jpeg)

Fonte: página oficial do autor. Acesso em out. 2026.

Quanta 2, da Audio Damage, é o granular comercial multiformato (VST3, AU, AAX e CLAP para desktop, com versão iOS), voltado a performance com samples tradicionais (Figura 3). Também não lê binários nem calcula entropia.

![Figura 3: catálogo de instrumentos da Audio Damage, com o Quanta 2](https://www.audiodamage.com/cdn/shop/collections/main_quanta.png?v=1661436891&width=800)

Fonte: site oficial da Audio Damage. Acesso em out. 2026.

O diferencial do Opcoda está na combinação ausente nos três: parser PE seguro com verificação de limites, entropia de Shannon como modulação e motor granular em tempo real rígido. A estrutura lida pelo parser segue a especificação PE/COFF da Microsoft (DOS Header, assinatura PE, COFF, Optional Header, tabela de seções e seções como .text, .data e .rsrc), documentada no Microsoft Learn.

## 3. Arquitetura

O pipeline tem quatro blocos: Parser isolado na UI thread, fila SPSC lock-free, motor granular de 8 vozes na thread de áudio e saída com DC-blocker e limiter. O diagrama BPMN do fluxo está em `docs/opcoda-fluxo.bpmn` (Figura 4, exportada pela equipe via bpmn.io).

## 4. Acessibilidade W3C

Auditoria WCAG 2.2 nível AA pelo método WCAG-EM. Nos docs, links genéricos foram corrigidos para texto descritivo (critério 2.4.4) e a tabela do cronograma usa cabeçalho (1.3.1). Na GUI, os critérios de aceite cobrem teclado total, foco visível, nomes acessíveis nos knobs, contraste de 4.5:1 e erro do parser em texto com código, nunca só por cor. O MIDI learn funciona como via acessível sem mouse. Reauditoria ao fim da etapa E4.

## 5. MVP e validação

Entrega em 03/11: standalone e VST3 x64, arrasto de .exe, 6 parâmetros, MIDI nativo e 8 vozes. Critérios: atenuação de DC de ao menos 40 dB, bloco abaixo de 50% do orçamento, zero segfaults e som em menos de 2 s após o arrasto. CLAP, bateria completa e monografia seguem após a entrega.
