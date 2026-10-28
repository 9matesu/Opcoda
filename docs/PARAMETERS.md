# Parâmetros

Fonte: `PluginProcessor::createParameterLayout` em
`src/opcoda_plugin/source/plugin_processor.cpp`. Valores na unidade exibida pela
interface. O host automatiza todos os 66 parâmetros: 41 de som, a largura
estéreo e as 24 células da matriz.

## Grão

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `grain` | SIZE | ms | 1 a 2000 | 40 | Duração da janela do grão. O knob é linear em milissegundos com centro em 100 ms, e a interface mostra segundos a partir de 1000. Numa região curta com pitch alto, o grão encurta para metade do material (e o envelope acompanha) para a posição não ficar morta. |
| `density` | DENSITY | grãos/s | 1 a 200 | 20 | Taxa de disparo. |
| `position` | POSITION | fração | 0 a 1 | 0,5 | Ponto de leitura no material. |
| `spray` | SPRAY | fração | 0 a 1 | 0 | Desvio uniforme da posição de cada grão em ±spray. |
| `pitch` | PITCH | semitons | -48 a +48 | 0 | Transposição. Quatro oitavas para cada lado. |
| `volume` | VOLUME | dB | -60 a 0 | 0 | Nível de saída. |
| `window` | WINDOW | escolha | Hann, Gaussian, Hamming, Blackman | Hann | Envelope do grão. |
| `pan` | PAN | -1 a +1 | -1 a +1 | 0 | Posição estéreo. |
| `width` | STEREO | fator | 0 a 2 | 1 | Largura estéreo na saída: mid/side até 1, e acima disso o canal direito atrasa até 12 ms (Haas), o spread que material mono não tem. 0 = mono, 1 = material, 2 = side dobrado com atraso máximo. |
| `grainlevel` | LEVEL | linear | 0 a 1 | 1 | Ganho por grão. |
| `pitchrand` | P.RAND | semitons | 0 a 12 | 0 | Detune aleatório por grão. |
| `scanspeed` | SCAN | fator | 0,25 a 4 | 1 | Velocidade da varredura no transporte. |

## Nuvem

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `cloudsize` | CLOUD W | fração | 0 a 1 | 0 | Atraso uniforme de 0 a 500 ms no início de cada grão. |
| `pitchspread` | P.SPREAD | semitons | 0 a 24 | 0 | Desvio de afinação por grão, simétrico. |
| `grainjit` | JITTER | fração | 0 a 1 | 0 | Variação de duração entre 0,5x e 1,5x, limitada a 1–2000 ms. |

## Envelope

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `attack` | ATTACK | s | 0,001 a 2 | 0,005 | Tempo de subida. |
| `decay` | DECAY | s | 0,001 a 2 | 0,1 | Tempo até o sustain. |
| `sustain` | SUSTAIN | linear | 0 a 1 | 1 | Nível mantido. |
| `release` | RELEASE | s | 0,001 a 2 | 0,05 | Tempo de descida. |
| `atkcurve` | ATK CURVE | -1 a +1 | -1 a +1 | 0 | Curva do ataque; -1 exponencial, +1 logarítmica. |
| `deccurve` | DEC CURVE | -1 a +1 | -1 a +1 | 0 | Curva do decay. |
| `relcurve` | REL CURVE | -1 a +1 | -1 a +1 | 0 | Curva do release. |

## Filtros

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `f1type` | FILTER 1 TYPE | escolha | Low-pass, High-pass, Band-pass, Notch | Low-pass | Tipo do primeiro filtro. |
| `f1cutoff` | FILTER 1 CUTOFF | Hz | 20 a 20000 | 20000 | Corte com escala logarítmica. |
| `f1q` | FILTER 1 Q | fator | 0,5 a 12 | 0,7071 | Ressonância. |
| `f1slope` | FILTER 1 SLOPE | escolha | 12 dB, 24 dB | 12 dB | Inclinação. |
| `f1drive` | FILTER 1 DRIVE | dB | 0 a 24 | 0 | Saturação antes do filtro 1; acima de 6 dB o clip do barramento fecha o teto de 1,0 até 0,40. |
| `f1keytrack` | FILTER 1 KEYTRACK | 0 a 1 | 0 a 1 | 0 | Move o corte com a nota; 1 = uma oitava por oitava. |
| `f1env` | FILTER 1 ENV | -1 a +1 | -1 a +1 | 0 | Move o corte com o envelope, até ±5 oitavas. |
| `f2type` | FILTER 2 TYPE | escolha | Low-pass, High-pass, Band-pass, Notch | Low-pass | Tipo do segundo filtro. |
| `f2cutoff` | FILTER 2 CUTOFF | Hz | 20 a 20000 | 20000 | Corte. |
| `f2q` | FILTER 2 Q | fator | 0,5 a 12 | 0,7071 | Ressonância. |

## LFO

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `lforate` | LFO RATE | Hz | 0 a 20 | 1 | Taxa no modo Free. |
| `lfodepth` | LFO DEPTH | 0 a 1 | 0 a 1 | 0 | Profundidade. |
| `lfotarget` | LFO TARGET | escolha | Pitch, Density, Cutoff, Position | Pitch | Destino. |
| `lfowave` | LFO WAVE | escolha | Sine, Tri, Saw, Square, S&H | Sine | Forma. |
| `lfosync` | LFO SYNC | escolha | Free, Sync | Free | Sincronismo com o transporte. |
| `lfodiv` | LFO DIV | escolha | 1/1 a 1/64, pontuados e tercinas | 1/4 | Divisão no modo Sync. |
| `lfosteps` | LFO STEPS | inteiro | 1 a 16 | 16 | Quantização em degraus; 16 = sem quantização. |

## Teclado e escala

| ID | Controle | Unidade | Faixa | Padrão | Comportamento |
| --- | --- | --- | --- | --- | --- |
| `scale` | SCALE | escolha | 12 escalas | Chromatic | Quantiza a nota. |
| `root` | ROOT | escolha | C a B | C | Tônica da escala. |
| `glide` | GLIDE | s | 0 a 2 | 0 | Portamento entre notas. |

## Matriz de modulação

Oito rotas, cada uma em três parâmetros automatizáveis e editáveis na vista
MATRIX do display (tecla 4): `m1src`, `m1dst` e `m1amt` até `m8src`, `m8dst`,
`m8amt`. Fontes: Off, LFO, envelope, velocidade, keytrack, mod wheel, CC74,
CC71 e entropia. Destinos: Off, pitch, density, position, spray, grain size,
cutoff 1, Q 1, cutoff 2, Q 2, pan, grain level, drive e stereo width. O amount
é bipolar, de -1 a +1, e o audio lê os 24 valores crus por bloco.

## Estado legado

Projetos gravados antes de a matriz virar parâmetro traziam um nó `MODMATRIX`
no estado; o processador copia as rotas para os parâmetros novos e remove o nó
ao carregar.
