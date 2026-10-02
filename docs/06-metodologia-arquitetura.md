# 6. Metodologia e arquitetura de software proposta

## Pipeline de dados

```text
[1. INGESTÃO / PARSER - UI Thread, não-RT, PROT_READ-only]
  Arquivo .exe/.bin
    -> open/mmap READONLY
    -> valida MZ + e_lfanew bounds-checked
    -> valida PE + NumberOfSections
    -> para cada seção: valida PointerToRawData + SizeOfRawData < fileSize
    -> copia defensiva p/ heap owned (float normalizado)
    -> calcula entropia H por janela 2048B + metadados por seção
    -> munmap/close
    -> publica descritor imutável via SPSC Queue
         |
         v
[2. BUFFER LOCK-FREE - Fronteira RT]
  SPSC Ring-Buffer + Atomic Params
  Double-buffer A/B com flip atômico
         |
         v
[3. MOTOR GRANULAR DSP - Audio Thread, RT-safe, pré-alocado]
  Pool fixo 8 vozes (MVP):
    Scheduler -> Seletor de posição -> Leitor interpolado
    -> Janela Hann/Gauss -> Ganho + Pan -> Mix bus
  -> DC-Blocker estéreo (R=0,9983, corte ~12 Hz)
  -> Soft-Clipper + Limiter -1 dBFS
         |
         v
[4. SAÍDA DE ÁUDIO]
  VST3/CLAP process() -> DAW Mix Bus
  + Telemetria lock-free reversa p/ GUI
```

Regra central: nada passa do parser para o DSP sem cruzar a fila, e o DSP nunca chama o sistema operacional.

## Tecnologias

- Linguagem: C++20. Alternativa documentada: Rust com nih-plug.
- Framework: JUCE 8 ou iPlug2, com alvos Standalone e VST3 x64 (Windows). CLAP após a entrega.
- Entrada: file dialog + drag-and-drop do SO, que entrega o caminho do arquivo ao parser na UI thread.
- Parser: próprio, header-only, sem dependência externa.
- FFT para análise: pffft ou KissFFT.
- Testes: GoogleTest, com fuzzing por mutação convertido em testes unitários determinísticos, e conformidade do bundle conferida no `moduleinfo.json`.
- DAW de validação: Ableton Live 12.3.1. O plano previa REAPER 7 e Bitwig Studio; a justificativa da troca está em `docs/07-plano-testes.md`.
- MIDI: CC mapeável nos parâmetros do framework. O MIDI chega ao DSP como parâmetro atômico, pela mesma via lock-free.
- Build: CMake + NMake, só MSVC. O plano previa Ninja com Clang e MSVC; a toolchain ficou num compilador só, com o que ele entrega nativamente.

## Mitigação de falhas

1. Toda leitura verifica se offset mais tamanho cabe no arquivo.
2. NumberOfSections limitado a faixa válida.
3. SizeOfRawData truncado para o tamanho real.
4. Ponteiros do arquivo mapeado não sobrevivem ao fechamento. DSP usa apenas a cópia.
5. Posição de grão sempre limitada ao buffer, com amostras de guarda.
6. Entradas inválidas retornam erro tipado, sem exceção cruzando a ABI.
7. Teto de grão em 100 ms com interpolação linear até o playback estabilizar sem xruns.
