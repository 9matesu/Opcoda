# 4. Objetivos

## Objetivo geral

Entregar até 03/11 o Opcoda como aplicativo standalone e plugin VST3 x64 (Windows) que abre ou recebe por arrasto arquivos .exe e PE, com parâmetros granulares controláveis por MIDI, redução de DC de ao menos 40 dB e playback sem xruns. CLAP e monografia completa seguem após a entrega.

## Objetivos específicos

### Engenharia de software

- OE1. Parser PE próprio que valida MZ, PE, número de seções e limites de cada seção, e converte bytes em float normalizado.
- OE2. Ingestão somente-leitura com abrir e arrastar (file dialog + drag-and-drop do SO), cópia defensiva e códigos de erro claros para arquivos truncados, sem propagar exceção para o host.
- OE3. Fila lock-free entre interface e DSP, com troca de sample por double-buffer.

### DSP

- OE4. Motor granular polifônico de 8 vozes, com tamanho de grão de 1 a 100 ms, densidade, posição, jitter, afinação e panorama, com janelas Hann e Gaussiana.
- OE5. Cálculo de entropia de Shannon por janela de 2048 bytes e por seção, usado para controlar dispersão e aleatoriedade dos grãos. Fórmula: H = - soma de p(xi) log2 p(xi).
- OE6. Cadeia de condicionamento com centralização DC, filtro DC-blocker de primeira ordem com R = 0,9983 e limitador de saída.

### Interface e IHC

- OE7. Interface simples com mapa de seções do PE, forma de onda navegável e controles automatizáveis no DAW. Curva de entropia em versão enxuta.
- OE8. Isolamento entre interface e DSP, sem acesso direto ao estado do áudio, com verificação por thread-sanitizer.
- OE9. Mapeamento MIDI nativo (MIDI learn, CC, pitch e mod wheel) nos parâmetros granulares, no standalone e no VST3.
