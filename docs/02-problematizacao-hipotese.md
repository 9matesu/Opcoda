# 2. Problematização e formulação da hipótese

## Contexto

Abrir um .exe como áudio bruto em um editor como o Audacity gera som, mas um som difícil de usar em música. Na prática, quatro problemas aparecem sempre:

1. Offset DC extremo. Bytes de programa não têm média zero. Opcodes comuns concentram valores em faixas específicas, e quando isso vira amostra de áudio o sinal fica deslocado. O resultado é perda de volume útil, estalos e risco para monitores.

2. Transientes espúrios. Cabeçalhos DOS, COFF, tabelas de seção e trechos de preenchimento com zeros criam saltos bruscos de amplitude. Soam como cliques sem relação musical.

3. Aliasing. Ler byte a byte como amostra cria variações muito rápidas, acima do limite de Nyquist. Ao mudar afinação ou posição de leitura, essas frequências dobram para dentro do espectro audível de forma descontrolada.

4. Risco de instabilidade. Um parser que confia nos campos do cabeçalho, como SizeOfRawData ou NumberOfSections, pode ler fora dos limites do arquivo se o binário estiver corrompido ou forjado. No pior caso, derruba o DAW inteiro com falha de segmentação. Nenhuma página do arquivo pode ser mapeada como executável.

## Pergunta de pesquisa

Como construir um instrumento em tempo real que leia binários PE arbitrários com segurança e os converta em material sonoro controlável, por síntese granular guiada por entropia, sem travar ou atrasar o host de áudio?

## Hipóteses

H1: um pipeline com parser isolado, fila lock-free e motor granular com janelamento e filtro DC-blocker reduz o offset DC em pelo menos 40 dB. Ele mantém o processamento por bloco abaixo de 50% do tempo disponível com 8 vozes e registra zero travamentos sob fuzzing com arquivos corrompidos.

H0 (nula): o tratamento granular e estatístico não traz diferença relevante em relação à leitura bruta direta.
