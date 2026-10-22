# 020 · Caráter analógico, curvas do envelope e matriz de modulação

**Feature Branch**: `020-carater-analogico-e-matriz`
**Criada**: 2026-10-19
**Status**: Implementado (DSP e estado); falta a página de matriz na UI, que é F023
**Objetivos atendidos**: OE4 (filtros e modulação)
**Ensaios**: portões B, C e E

**Alvo**: F020, do mapa da emenda 1.2.0

## Contexto

Três lacunas com a mesma origem — o instrumento soa digital no sentido estreito:
o filtro é um biquad de livro, o envelope é linear em todos os estágios, e não há
forma de ligar uma fonte de modulação a mais de um destino sem acrescentar um
parâmetro por combinação.

O filtro já existe (F013), e o ADSR também (F012). O que esta feature acrescenta
é o que os sintetizadores modernos chamam de *caráter*: inclinação selecionável,
saturação antes do filtro, corte que segue a nota, profundidade de envelope, e
curvas por estágio.

A matriz de 8 rotas é a peça que a emenda 1.2.0 declarou **fora da tabela de
parâmetros**: vive no estado do APVTS, viaja no preset, e não é automatizável
pelo host. A consequência está escrita na constitution e em `docs/19`, e o teste
tem de a provar — nomeadamente que nenhuma rota aparece no painel de parâmetros.

## Cenários

### História 1 — O filtro responde à nota (P1)

Como músico, quero que o corte acompanhe a nota tocada, para que as notas agudas
não fiquem abafadas.

**Por que P1**: é a diferença mais audível entre um filtro estático e um filtro
musical, e é o que faz um patch de baixo funcionar em duas oitavas.

**Teste independente**: `Filter.KeytrackMovesCutoffOneOctavePerOctave` confirma
que, com `keytrack = 1`, MIDI 72 produz o dobro do corte de MIDI 60.

### História 2 — Inclinação e drive (P2)

Como produtor, quero 12 e 24 dB e um drive antes do filtro, para passar de um
filtro limpo a um filtro com corpo.

**Teste independente**: `Filter.SlopeTwentyFourAttenuatesTwiceInDbPerOctave`
compara a resposta a 12 e a 24 dB; `Filter.DriveAddsHarmonicsWithoutClippingOutput`
verifica que o drive aumenta o conteúdo harmónico sem passar do teto.

### História 3 — Curvas do envelope (P2)

Como produtor, quero que o ataque seja exponencial e o release logarítmico, para
que o envelope soe a analógico e não a rampa.

**Teste independente**: `Envelope.CurveMinusOneIsExponential` e
`Envelope.CurvePlusOneIsLogarithmic` comparam o valor a 25 % e a 75 % do estágio
contra a rampa linear.

### História 4 — Uma fonte, vários destinos (P1)

Como produtor, quero ligar o LFO ao corte e à densidade ao mesmo tempo, sem
duplicar o LFO.

**Por que P1**: é o que a matriz existe para resolver.

**Teste independente**: `Matrix.TwoRoutesFromOneSourceSumAtDestination` confirma
que duas rotas com a mesma fonte somam no destino e que o resultado é limitado à
faixa do parâmetro.

### História 5 — A matriz não é parâmetro (P1)

Como host, não quero que as rotas apareçam no painel de automação.

**Teste independente**: o layout fecha em 38 parâmetros nesta fatia, e o número
é verificado por `jassert` na construção do processador, ao lado do nó
`MODMATRIX` que guarda as 8 rotas. Não há teste no binário de núcleo para isto
porque o layout é JUCE e os ensaios de núcleo não linkam JUCE; a prova de que as
rotas viajam no estado é `Matrix.RoutesSurviveStateRoundTrip`, que exercita a
mesma conversão que o APVTS usa.

## Requisitos

### Filtro

1. `f1slope` em `{12, 24}`: 24 dB acrescenta um segundo estágio idêntico em série
   ao biquad do filtro 1.
2. `f1drive` de 0 a 24 dB: ganho antes do filtro, saturação `tanh`, compensação
   de ganho na saída para não subir o nível. A compensação é `1/g` e não
   `1/tanh(g)`: a segunda normaliza para escala cheia e levanta 12 dB o material
   baixo, a primeira garante `|tanh(g·x)/g| ≤ |x|` para qualquer entrada, e o
   pico nunca sobe. Em `drive = 0` o caminho é desligado, não saturado.
   **Sem oversampling**, e isso é declarado: o aliasing do drive é audível em
   material rico em agudos, e a alternativa custaria 4× o CPU.
3. `f1keytrack` de 0 a 1: `cutoff *= 2^((note - 60) * keytrack / 12)`.
4. `f1env` de −1 a +1: `cutoff *= 2^(env * depth * 5)`, com o envelope normalizado.
5. Coeficientes recalculados quando qualquer entrada muda, no máximo uma vez por
   bloco, nunca por amostra.

### Envelope

6. `atkcurve`, `deccurve`, `relcurve` de −1 a +1: a fase do estágio sobe ao
   expoente `2^(3 * curve)`, o que dá linear em 0, **rápido no início e lento no
   fim em −1** (expoente 1/8) e **lento no início e rápido no fim em +1**
   (expoente 8). **O sinal estava trocado na primeira versão:** a frase dizia
   isto e a fórmula escrita ao lado dizia o contrário. Vale a frase, que é a
   convenção do Serum declarada na `docs/19`; a fórmula passou a `2^(+3*curva)`.
7. O cálculo é `pow`, uma vez por amostra apenas quando a curva não é 0; com
   curva 0 usa-se a rampa linear, que é o caminho atual, bit a bit — os ensaios
   do ADSR comparam com `EXPECT_FLOAT_EQ` e trocar a aritmética mudaria os
   últimos bits de todos eles.

### Matriz

8. Oito rotas de `{source, destination, amount}`, em arrays fixos.
9. Fontes: LFO, envelope, velocity, keytrack, mod wheel, CC 74, CC 71, entropia.
10. Destinos: pitch, density, position, spray, grain size, cutoff 1, Q 1, cutoff 2,
    Q 2, pan, grain level, drive.
11. Avaliação por bloco, com o resultado somado ao valor do parâmetro e limitado à
    faixa declarada. `amount` de −1 a +1. A escala de cada destino: pitch ±12
    semitons, densidade e tamanho de grão em oitavas (×4), posição ±0,5 de
    fração, spray ±1, cortes ±5 oitavas, Q ×4, pan ±1, nível de grão ±1, drive
    ±24 dB.
12. Persistência: as rotas são propriedades da árvore do APVTS, guardadas no
    estado do plugin e no `.opcoda`. Rota inválida no estado é ignorada, não
    provoca exceção.
13. O motor recebe a matriz numa cópia dos parâmetros por bloco, e não acumula:
    a soma das rotas aplica-se ao valor declarado desse bloco.
14. A publicação entre threads são 24 inteiros atómicos (fonte, destino, amount
    em milésimos) mais um contador de geração. Quem lê vê a matriz antiga
    inteira ou a nova inteira, nunca metade de cada, e não há alocação nem
    bloqueio no caminho de áudio.

## Critérios de aceite

- [x] `keytrack = 1`: MIDI 72 tem o dobro do corte de MIDI 60;
- [x] `slope = 24` atenua ~24 dB/oitava acima do corte; `12`, ~12;
- [x] drive de 24 dB não aumenta o pico de saída acima de 0,1 dB;
- [x] curva −1, 0 e +1 produzem três formas distintas e mensuráveis a 25 % e 75 %;
- [x] duas rotas da mesma fonte somam;
- [x] o layout fecha em 38 parâmetros nesta fatia (41 com a F021), sem nenhuma
      rota na tabela;
- [x] as 8 rotas sobrevivem à conversão estado ⇄ array que o APVTS usa;
- [x] rota com destino inválido é ignorada sem exceção;
- [x] guard de alocação verde com a matriz ativa (o caminho de áudio só copia
      inteiros);
- [ ] **por ouvir**: drive, 24 dB e as três curvas num host real (sessão T4).
      O binário de ensaio mede as formas; ninguém as ouviu ainda.

## Não-objetivos

- **Oversampling do drive.** Declarado acima.
- **Paridade do filtro 2.** `f2slope`, `f2drive`, `f2keytrack` e `f2env` custariam
  4 parâmetros e a tabela iria a 45; o desenho está em `docs/19`.
- **Rotas automatizáveis.** Custariam 24 parâmetros; ficam no estado.
- **Envelope por nota.** O instrumento é mono e o envelope é global.
- **Curvas visualizadas no editor antes da F023.** A F020 entrega o DSP; a curva
  desenhada é a F023.

## Testes

| Teste | O que prova |
| --- | --- |
| `Filter.KeytrackMovesCutoffOneOctavePerOctave` | keytrack, também a meia profundidade |
| `Filter.SlopeTwentyFourAttenuatesTwiceInDbPerOctave` | inclinação, por RMS a duas oitavas do corte |
| `Filter.DriveAddsHarmonicsWithoutClippingOutput` | drive: pico não sobe e a terceira harmónica cresce |
| `Envelope.CurveMinusOneIsExponential` | curva −1, a 25 % e a 75 % |
| `Envelope.CurvePlusOneIsLogarithmic` | curva +1, a 25 % e a 75 % |
| `Envelope.CurvesReachTheEngine` | as curvas chegam ao envelope do motor, não só à função |
| `Matrix.TwoRoutesFromOneSourceSumAtDestination` | soma de rotas |
| `Matrix.TwoSourcesSum` | duas fontes no mesmo destino |
| `Matrix.OffsetIsClamped` | soma saturada em ±1 |
| `Matrix.InvalidRouteIsIgnored` | fonte/destino vazios e amount NaN |
| `Matrix.RoutesSurviveStateRoundTrip` | persistência (os 24 inteiros) |
| `Matrix.CorruptStateOpensEmpty` | robustez do estado |
| `Matrix.EngineAppliesRouteToFilterCutoff` | a rota chega ao motor e não deixa rasto |
