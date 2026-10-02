# Feature Specification: Ingestão de binários PE

**Feature Branch**: `001-ingestao-pe`
**Created**: 2026-10-01
**Status**: Implementado
**Objetivos atendidos**: OE1, OE2
**Ensaios atendidos**: T1 (parcial), T3 (casos de borda)

## Contexto

O Opcoda transforma arquivos executáveis em som. Esta feature é a fronteira
entre o arquivo no disco e o motor de síntese: abrir o binário, validar a
estrutura PE, converter os bytes em amostras e entregar o material ao motor
granular. Sem ela nada soa, porque o motor não lê disco.

Não inclui a interface gráfica (F008) nem o arrasto de arquivo, que é camada de
UI sobre o mesmo caminho. Inclui o caminho completo em código, testado sem JUCE.

## Cenários de usuário

### História 1 — O `.exe` vira som (P1)

Como músico, quero abrir um executável no Opcoda e ouvir o material, para poder
usar bytes de programa como fonte sonora.

**Por que P1**: é a entrega da S1 do cronograma, "áudio passando com 1 parâmetro".
Sem isso não há instrumento.

**Teste independente**: `EndToEnd.RealExecutableBecomesAudio` renderiza
`C:\Windows\System32\notepad.exe` pela cadeia completa e exige pico audível,
ausência de NaN e teto respeitado.

**Cenários de aceitação**:

1. **Dado** um PE válido, **quando** o arquivo é analisado, **então** a contagem
   de amostras é maior que 100.000 bytes
2. **Dado** um PE válido, **quando** a cadeia renderiza 1 segundo, **então** o
   pico fica entre 0,01 e 1,0 e não há NaN
3. **Dado** um PE válido, **quando** a saída é medida, **então** a média fica uma
   ordem de grandeza abaixo da média da leitura bruta

---

### História 2 — Binário corrompido é recusado com código (P1)

Como undesenvolvedor, quero que um binário inválido seja rejeitado com um código
legível, para que o host nunca segure e o usuário saiba o que houve.

**Por que P1**:OE2 exige códigos de erro claros e nenhuma exceção cruzando a ABI.
Um parser que aborta o DAW é pior que um que recusa o arquivo.

**Teste independente**: `EndToEnd.CorruptedBinaryIsRejectedWithTypedError` e os
dez casos de borda de `pe_parser_test`.

**Cenários de aceitação**:

1. **Dado** um arquivo com assinatura `MZ` apagada, **quando** é analisado,
   **então** retorna `E_BAD_MZ` e nenhuma amostra é produzida
2. **Dado** um arquivo com `e_lfanew` além do fim, **quando** é analisado,
   **então** retorna `E_BAD_E_LFANEW`
3. **Dado** um arquivo com `NumberOfSections` absurdo, **quando** é analisado,
   **então** retorna `E_TOO_MANY_SECTIONS` sem alocar tabela do tamanho pedido
4. **Dado** um arquivo com `SizeOfRawData` além do fim, **quando** é analisado,
   **então** retorna `E_OOB`
5. **Dado** um caminho inexistente, **quando** é analisado, **então** retorna
   `E_EMPTY` e não lança

---

### História 3 — A conversão byte→amostra é a mesma em todo lugar (P2)

Como mantenedor, quero uma única conversão de byte para amostra, para que o
plugin e o ensaio T1 não divirjam.

**Por que P2**: a duplicação existia e já causou a leitura de 44100 bytes de um
buffer de 768, detectada pelo AddressSanitizer.

**Teste independente**: `T1DcAttenuation.ToSamplesMapsByteRangeToUnitInterval`
e o critério de 40 dB do T1, que passa a chamar a mesma função do núcleo.

**Cenários de aceitação**:

1. **Dado** os bytes 0, 127, 128 e 255, **quando** são convertidos, **então** as
   amostras são −1, −0,0039, +0,0039 e +1
2. **Dado** um offset não nulo, **quando** a conversão roda, **então** o recorte
   começa no offset
3. **Dado** entrada nula ou contagem zero, **quando** a conversão roda, **então**
   devolve vetor vazio

---

### História 4 — O plugin carrega o binário (P2)

Como usuário do DAW, quero que o VST3 e o Standalone produzam som, para poder
usar o Opcoda em qualquer host.

**Por que P2**: é a forma do instrumento chegar ao usuário. A entrega da S1 é o
artefato compilando e processando, o que está verificado pelo build.

**Teste independente**: `cmake --build --preset vst3` e `--preset standalone`
geram os dois artefatos. A cadeia de áudio é a mesma do teste end-to-end, que
não usa JUCE.

**Cenários de aceitação**:

1. **Dado** o preset release, **quando** compila, **então** produz
   `Opcoda.vst3` e `Opcoda.exe` para x64
2. **Dado** uma imagem válida, **quando** `ingest` é chamada, **então** o motor
   recebe as amostras e `hasSample` fica verdadeiro
3. **Dado** uma imagem inválida, **quando** `ingest` é chamada, **então**
   `lastError` traz o código tipado e o material anterior é preservado

## Fora de escopo

- Arrasto e soltar e diálogo de arquivo: camada de UI, F008
- Editor gráfico: F008
- MIDI learn e CC: F009
- Mapeamento de curvas de entropia por seção: F004
- Telemetria para a GUI: F007

## Critérios de aceite da feature

| Critério | Verificado por |
| --- | --- |
| PE válido produz som audível | `EndToEnd.RealExecutableBecomesAudio` |
| Média de saída muito menor que a bruta | `EndToEnd.OutputHasNoDcOffset` |
| Toda entrada inválida recusa com código | `EndToEnd.CorruptedBinaryIsRejectedWithTypedError` e casos de borda |
| Truncamento nunca lê além do buffer | `EndToEnd.TruncatedRealBinaryIsRejected` |
| Conversão única byte→amostra | `T1DcAttenuation.ToSamples*` e o T1 de 40 dB |
| VST3 e Standalone compilam para x64 | build do preset release |
| Núcleo sem alocação no caminho de áudio | `opcoda_alloc_guard_test` |
| Cobertura de build e testes | `tools/build.ps1` e `-Release` |

## Decisões e armadilhas

**O `readStep` precisa ser normalizado.** A taxa de reprodução é a razão de
semitons, e o avanço por amostra tem de ser essa taxa dividida pelo
comprimento da fonte, porque `position` é normalizada em [0, 1]. Somar a taxa
crua fazia o grão varrer o arquivo inteiro em duas amostras e morrer, e a saída
ficava com pico de 5,6e-7. O ensaio T1 passava por acidente: com uma fonte de
4096 amostras o salto não ultrapassava 1,0 e produzia um pico residual
suficiente para a asserção frouxa.

**Um grão que atravessa o bloco recomeça em zero.** `writeIndex` é a posição
dentro do bloco corrente, não um contador global. Sem esse reset a voz ficava
travada: entrava no próximo bloco com índice maior que o tamanho do bloco e
nunca mais escrevia.

**O COFF header começa em `eLfanew + 4`, não em `eLfanew`.** A assinatura
`PE\0\0` ocupa quatro bytes. Ler `NumberOfSections` em `eLfanew + 2` faz o
parser recusar todo executável válido com `E_BAD_PE`. O erro estava no código
e não na especificação do formato, e só apareceu com um PE mínimo válido
construído em memória pelo `tests/pe_builder.h`.

**A posição do grão importa.** O DC-blocker anula corretamente um sinal
constante, então uma posição que caia em região de preenchimento produz silêncio
por desenho, e não por defeito.

**A dependência de JUCE é só no plugin.** O núcleo não inclui cabeçalho do
framework, e é por isso que o ensaio end-to-end roda sem JUCE e sem host.

**JUCE 8.0.14 compila contra o toolset v145.** A contingência do `docs/08`
previa trocar para iPlug2, e não foi necessária: o VST3 e o Standalone
compilam e linkam no Visual Studio 2026.
