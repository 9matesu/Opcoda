# 5. Fundamentação teórica / Revisão da literatura

Este capítulo organiza a base conceitual do Opcoda em quatro eixos: arquitetura de arquivos binários e engenharia reversa estática; teoria da informação e entropia; processamento digital de sinais e síntese granular; engenharia de software em tempo real rígido. O encadeamento entre os eixos corresponde ao pipeline do instrumento: o arquivo executável é lido com segurança, descrito por entropia, ressintetizado por granulação e entregue sob restrições de tempo real.

## 5.1 Arquitetura de arquivos binários e engenharia reversa estática

### 5.1.1 A especificação PE/COFF

O formato Portable Executable (PE) é o formato de arquivos executáveis, bibliotecas dinâmicas e drivers do sistema Windows em arquiteturas x86 e x64, derivado do Common Object File Format (COFF). Sua especificação é mantida pela Microsoft e descreve a organização binária que o carregador do sistema operacional utiliza para mapear o arquivo em memória (MICROSOFT, 2024).

A anatomia canônica do formato foi documentada por Pietrek em artigos de referência sobre a estrutura interna do PE, que descrevem cada campo dos cabeçalhos e sua função no carregamento (PIETREK, 1994; PIETREK, 2002). A sequência lógica do arquivo é: DOS Header (mantido por compatibilidade, com o campo `e_lfanew` que indica o deslocamento do cabeçalho PE), PE Signature (`PE\0\0`), COFF File Header (número de seções, características da imagem), Optional Header (ponto de entrada, endereço-base, tamanho de pilha e heap, diretórios de dados) e Section Table, seguida do corpo das seções (MICROSOFT, 2024; PIETREK, 2002).

### 5.1.2 Seções: .text, .data, .rdata e .rsrc

Cada entrada da Section Table declara nome, tamanho virtual, endereço virtual, tamanho dos dados brutos em disco (`SizeOfRawData`), ponteiro para os dados brutos (`PointerToRawData`) e flags de permissão (código, leitura, escrita). Essa dualidade entre tamanho em disco e tamanho em memória é central para a leitura segura, pois os dois valores podem divergir legitimamente ou por corrupção (PIETREK, 2002; MICROSOFT, 2024).

As seções convencionais possuem perfis distintos de conteúdo. A seção `.text` concentra código de máquina executável. As seções `.data` e `.rdata` concentram dados inicializados e constantes. A seção `.rsrc` concentra recursos como ícones, bitmaps, diálogos e cadeias de caracteres. Sikorski e Honig utilizam exatamente essa segmentação como ponto de partida da análise estática de binários, extraindo seções para inspeção sem executar o artefato (SIKORSKI; HONIG, 2012). No Opcoda, essa segmentação fundamenta a hipótese acústica do instrumento: cada seção constitui uma região de textura estatística própria, sonificada de forma parametrizável.

### 5.1.3 Leitura estática segura de arquivos não confiáveis

A análise estática, por definição, extrai informação do arquivo sem executá-lo (SIKORSKI; HONIG, 2012). Para arquivos potencialmente não confiáveis, três princípios se aplicam. Primeiro, o princípio do menor privilégio: o arquivo é aberto exclusivamente para leitura e nenhuma página de memória recebe permissão de execução (`PROT_EXEC`), de modo que a execução acidental é eliminada por construção. Segundo, toda leitura indexada por campos do cabeçalho (`e_lfanew`, `NumberOfSections`, `PointerToRawData`, `SizeOfRawData`) é precedida de verificação de limites contra o tamanho real do arquivo, o que previne leituras fora de limites e estouros de buffer. Terceiro, valores fora de faixas plausíveis implicam rejeição com erro tipado, sem propagação de exceção ao processo hospedeiro. Esses procedimentos materializam, no domínio de análise de binários, as práticas de software seguro para sistemas embarcados e de tempo real descritas por Kleidermacher e Kleidermacher (KLEIDERMACHER; KLEIDERMACHER, 2012).

## 5.2 Teoria da informação e entropia de dados arbitrários

### 5.2.1 A formulação de Shannon

Shannon definiu a entropia de uma fonte discreta como a quantidade média de informação por símbolo:

H(X) = - Σ p(xi) · log2 p(xi)

em que p(xi) é a probabilidade do símbolo xi no alfabeto da fonte (SHANNON, 1948). Aplicada a fluxos de bytes, o alfabeto contém 256 símbolos (valores 0 a 255) e a entropia máxima é 8 bits por byte, atingida quando todos os valores são equiprováveis.

### 5.2.2 Entropia por janela deslizante

O cálculo sobre o arquivo completo produz um único número, insuficiente para navegação paramétrica. A técnica adotada é a janela deslizante de tamanho fixo (neste trabalho, 2048 bytes, com a faixa de 256 a 1024 bytes documentada como referência comparativa): estima-se a distribuição de frequências dentro de cada janela e computa-se H local, produzindo a curva de entropia ao longo do arquivo. Lyda e Hamrock demonstraram que esse perfil entrópico distingue classes de conteúdo em binários, pois regiões distintas do arquivo exibem assinaturas estatísticas distintas (LYDA; HAMROCK, 2007).

### 5.2.3 Leitura física dos valores

A correlação entre H e a natureza do conteúdo é direta. Regiões com repetição massiva de bytes, como preenchimento com zeros ou áreas não inicializadas, apresentam entropia próxima de zero. Código compilado padrão apresenta entropia intermediária, refletindo a distribuição enviesada de opcodes e operandos. Blocos comprimidos, empacotados ou cifrados apresentam entropia próxima de 8 bits por byte, comportamento análogo ao ruído estocástico de banda larga (LYDA; HAMROCK, 2007; SHANNON, 1948).

### 5.2.4 Entropia como sinal de controle

No Opcoda, a curva de entropia opera como fonte de modulação paramétrica contínua (Entropy-to-CV): a entropia local pondera a dispersão temporal dos grãos, sua aleatoriedade de posição e a densidade da nuvem. Regiões de alta entropia geram texturas instáveis e ruidosas; regiões de baixa entropia geram texturas estáveis e periódicas. A justificativa teórica é que a entropia resume, em um escalar por janela, a mesma distinção entre ordem e ruído que a síntese granular manipula no domínio temporal.

## 5.3 Processamento digital de sinais e síntese granular

### 5.3.1 Microsom e síntese granular

Roads formalizou a síntese granular a partir do conceito de microsom: o som como agregado de microfragmentos de 1 a 100 ms, cada grão com duração, envelope, frequência, amplitude e posição próprios, organizados em estruturas meso-temporais como nuvens e fluxos (ROADS, 2001). Os parâmetros canônicos são tamanho de grão, densidade de disparo, dispersão temporal (spray), afinação e panorama. Essa teoria sustenta o motor do Opcoda, cujos grãos variam de 1 a 100 ms no MVP.

### 5.3.2 O problema do PCM bruto não senoidal

Interpretar bytes executáveis como amostras PCM produz um sinal sem as garantias do áudio convencional: média diferente de zero (offset DC extremo), descontinuidades abruptas nos limites de cabeçalhos e regiões de preenchimento (transientes espúrios) e variações amostra a amostra acima do limite de Nyquist, que se dobram para o espectro audível como aliasing (SMITH, 2007; ZÖLZER, 2011). O janelamento granular e a filtragem de condicionamento existem precisamente para converter esse material bruto em sinal musicalmente utilizável.

### 5.3.3 Janelamento: Hann, Hamming e Blackman

Multiplicar cada grão por uma função de janela força amplitude nula nas bordas, o que elimina cliques por descontinuidade de fase na sobreposição (overlap-add). As formulações adotadas, para janela de N amostras com n de 0 a N-1, são (SMITH, 2007):

Hann: w(n) = 0,5 · (1 - cos(2πn / (N-1)))

Hamming: w(n) = 0,54 - 0,46 · cos(2πn / (N-1))

Blackman: w(n) = 0,42 - 0,5 · cos(2πn / (N-1)) + 0,08 · cos(4πn / (N-1))

O instrumento adota Hann e Gaussiana no MVP. A escolha se fundamenta no compromisso entre suavidade temporal e seletividade espectral: janelas suaves reduzem espalhamento espectral dos transientes ao custo de resolução temporal, efeito desejável sobre material databending (SMITH, 2007; ROADS, 2001).

### 5.3.4 Filtros digitais: DC-blocker e biquad ressonante

O offset DC é removido por filtro IIR passa-alta de primeira ordem (DC-blocker) com polo próximo ao círculo unitário:

y[n] = x[n] - x[n-1] + R · y[n-1]

com R = 0,9983, o que posiciona a frequência de corte em aproximadamente 12 Hz a 44,1 kHz, dentro da faixa de 10 a 15 Hz que preserva o conteúdo audível (SMITH, 2007). O coeficiente é R = exp(−2·π·f_c/f_s); o valor 0,995 que constava em versões anteriores deste texto produziria corte em 35 Hz, fora da faixa pretendida. A coloração adicional utiliza biquad ressonante na Forma Direta:

y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] - a1·y[n-1] - a2·y[n-2]

com coeficientes calculados por frequência, Q e ganho segundo as fórmulas de projeto de biquads (SMITH, 2007; ZÖLZER, 2011). A saída passa ainda por limitador para contenção de picos residuais.

### 5.3.5 Estética do databending e glitch

Cascone fundamentou a estética da falha como material musical legítimo: erros, aliasing e artefatos digitais deixam de ser defeitos e passam a vocabulário composicional (CASCONE, 2000). O Opcoda inscreve-se nessa linhagem, com a diferença de que domestica o acidente por engenharia: o glitch permanece como fonte timbrística, mas sob controle paramétrico e dentro de garantias de tempo real.

## 5.4 Engenharia de software em tempo real rígido

### 5.4.1 Restrições da thread de áudio em VST3 e CLAP

Formatos profissionais de plugins impõem contrato de tempo real rígido ao callback de processamento (`process`/`processBlock`): o período por bloco é da ordem de milissegundos e o atraso é inadmissível. Três proibições decorrem desse contrato. Primeiro, nenhuma alocação dinâmica de memória (`malloc`, `free`, `new`) durante o processamento, pois o alocador tem tempo não determinístico. Segundo, nenhuma operação de entrada e saída de arquivos ou chamada de sistema bloqueante no laço prioritário. Terceiro, nenhuma primitiva de sincronização com espera (`mutex`, `lock`), que expõe o caminho de áudio à inversão de prioridade (STEINBERG, 2024; CLAP, 2024; PIRKLE, 2019).

### 5.4.2 Arquitetura concorrente desacoplada

A solução canônica separa dois domínios temporais. A interface gráfica (cerca de 60 quadros por segundo) admite alocação, I/O e parsing. O motor DSP (44,1 kHz a 96 kHz) opera exclusivamente sobre memória pré-alocada. A comunicação entre os domínios utiliza fila circular lock-free de produtor único e consumidor único (SPSC ring buffer), baseada em operações atômicas de hardware com ordenamento acquire-release, além de parâmetros atômicos com suavização no DSP (PIRKLE, 2019). Essa arquitetura, combinada às práticas de software seguro para sistemas com restrições temporais (KLEIDERMACHER; KLEIDERMACHER, 2012), sustenta os requisitos de robustez do instrumento.

## 5.5 Síntese dos eixos

Os quatro eixos convergem para a hipótese do trabalho: a leitura estática segura (5.1) fornece material bruto íntegro; a entropia (5.2) descreve esse material em sinal de controle; a granulação com janelamento e filtragem (5.3) converte o material em som utilizável; a arquitetura lock-free (5.4) garante a entrega em tempo real rígido. Cada eixo é testável por um ensaio do plano de validação: segurança por fuzzing, condicionamento por análise FFT, tempo real por medição de bloco e usabilidade por aceite com MIDI.

## Referências

CASCONE, K. The aesthetics of failure: post-digital tendencies in contemporary computer music. Computer Music Journal, Cambridge, v. 24, n. 4, p. 12-18, 2000.

CLAP. CLever Audio Plugin: specification. 2024. Disponível na documentação oficial do projeto CLAP.

KLEIDERMACHER, D.; KLEIDERMACHER, M. Embedded systems security: practical methods for safe and secure software and systems development. Oxford: Newnes/Elsevier, 2012.

LYDA, R.; HAMROCK, J. Using entropy analysis to find encrypted and packed malware. IEEE Security & Privacy, v. 5, n. 2, p. 40-45, 2007.

MICROSOFT. Microsoft Portable Executable and Common Object File Format specification. Redmond: Microsoft, 2024. Revisão vigente.

PIETREK, M. Peering inside the PE: a tour of the Win32 Portable Executable file format. Microsoft Systems Journal, 1994.

PIETREK, M. An in-depth look into the Win32 Portable Executable file format. MSDN Magazine, 2002.

PIRKLE, W. C. Designing software synthesizer plug-ins in C++: for RackAFX, VST3, and Audio Units. 2. ed. New York: Focal Press, 2019.

ROADS, C. Microsound. Cambridge: MIT Press, 2001.

SHANNON, C. E. A mathematical theory of communication. Bell System Technical Journal, v. 27, p. 379-423, 623-656, 1948.

SIKORSKI, M.; HONIG, A. Practical malware analysis: the hands-on guide to dissecting malicious software. San Francisco: No Starch Press, 2012.

SMITH, J. O. Introduction to digital filters: with audio applications. [S.l.]: W3K Publishing, 2007.

STEINBERG. VST 3 audio plug-ins SDK: documentation. Hamburg: Steinberg, 2024.

ZÖLZER, U. (Ed.). DAFX: digital audio effects. 2. ed. Chichester: Wiley, 2011.
