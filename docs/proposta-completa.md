# Opcoda: sintetizador granular em tempo real a partir de binários executáveis

Proposta de Trabalho de Graduação, Análise e Desenvolvimento de Sistemas, 5º semestre.

> Versão completa gerada a partir dos arquivos em `docs/01` a `docs/08`. Para edição por seção, use os arquivos separados.

## 1. Título e tema acadêmico

Opcoda: síntese granular em tempo real a partir da análise estática de binários no formato Portable Executable com processamento digital de sinais.

Tema: construção de um instrumento standalone e plugin VST3 x64 que abre ou recebe por arrasto arquivos .exe e binários arbitrários como matéria-prima sonora e os transforma em som utilizável por síntese granular.

Linha de pesquisa: Engenharia de Software, com foco em sistemas concorrentes, áudio em tempo real e multimídia. O trabalho toca ainda em segurança de sistemas, pois lida com arquivos executáveis.

Delimitação: o Opcoda não executa o binário, não desmonta código para análise de malware e não faz análise dinâmica. Ele trata o arquivo apenas como uma sequência de bytes para leitura.

## 2. Problematização e hipótese

Abrir um .exe como áudio bruto gera quatro problemas: offset DC extremo, transientes espúrios, aliasing acima de Nyquist e risco de instabilidade por leitura fora de limites.

Pergunta: como construir um instrumento em tempo real que leia binários PE arbitrários com segurança e os converta em material sonoro controlável, por síntese granular guiada por entropia, sem travar o host?

H1: pipeline com parser isolado, fila lock-free e motor granular com janelamento e DC-blocker reduz o offset DC em pelo menos 40 dB. Ele mantém o processamento abaixo de 50% do orçamento com 8 vozes e registra zero travamentos sob fuzzing.

H0: o tratamento não traz diferença relevante em relação à leitura bruta.

## 3. Justificativa

O valor para ADS está na arquitetura: separação UI vs DSP, comunicação lock-free SPSC, memória estática no loop de áudio e ingestão somente-leitura com validação MZ/PE. Supera Audacity raw import (offline), samplers comerciais (só WAV) e sonificação forense (offline em Python).

## 4. Objetivos

Geral: entregar até 03/11 o Opcoda standalone + VST3 x64, com arrasto de .exe, parâmetros por MIDI, redução de DC de ao menos 40 dB e playback sem xruns.

Específicos: (1) parser PE próprio com bounds checking; (2) ingestão PROT_READ com abrir e arrastar; (3) fila lock-free; (4) motor granular 8 vozes 1-100 ms com Hann/Gauss; (5) entropia de Shannon por janela 2048B; (6) DC-blocker R=0,995 + limiter; (7) GUI simples com mapa de seções; (8) thread-sanitizer limpo; (9) MIDI learn e CC.

Detalhamento dos nove objetivos específicos em `04-objetivos.md`.

## 5. Fundamentação

PE/COFF (.text, .data, .rsrc); Shannon 1948 aplicada a 256 símbolos; Roads 2001 Microsound e janelamento; VST3/CLAP real-time safe, Bencina, Pirkle, Zölzer. Os quatro pilares do referencial estão desenvolvidos em `05-fundamentacao.md`.

## 6. Metodologia e arquitetura

Pipeline: Ingestão/Parser (UI, PROT_READ) -> Buffer Lock-Free (SPSC + double-buffer) -> Motor Granular (audio thread, pool 8 vozes) -> Saída Standalone + VST3 x64. Regra: nada cruza sem fila, DSP nunca chama SO.

Stack: C++20, JUCE 8 ou iPlug2, parser próprio, pffft/KissFFT, GoogleTest/Catch2, libFuzzer/AFL++, Ableton Live 12.3.1, CMake + ASan/TSan/UBSan. Stack completa e mitigação de falhas em `06-metodologia-arquitetura.md`.

## 7. Testes

T1 FFT N=65536: atenuação DC >=40 dB, sub-20 Hz <-60 dBFS. T2: p99 <50% do budget a 8 vozes, 0 xruns em 5 min. T3: 50 mutações + 10 casos de borda, 0 segfaults, cobertura parser >=70%. T4: arrasto com som em <2 s e CC move o parâmetro. Protocolos T1 a T3 com critérios de aceite em `07-plano-testes.md`.

## 8. Cronograma

MVP até 03/11 com o calendário de 5 semanas. Tabela de etapas e portões de controle em `08-cronograma.md`.
