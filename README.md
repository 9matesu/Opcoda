<!-- abre aqui: o que e, como rodar -->

# Opcoda

Sintetizador granular em tempo real para Windows x64. Lê arquivos Portable
Executable (`.exe`, `.dll`, `.bin`, `.sys`) como matéria-prima sonora e organiza
os bytes em grãos de 1 ms a 2 s. Existe como VST3 e *standalone*, na versão
0.1.0, ainda sem lançamento comercial.

![Interface do Opcoda com um binário carregado](docs/assets/screenshots/opcoda-ui.png)

A captura mostra o *standalone* com um binário carregado. Foi gerada com
`tools/capture-ui.ps1`.

<!-- o recorte -->

## O que é

O Opcoda lê a estrutura de um executável, converte os bytes em amostras
normalizadas e reproduz o resultado com um motor granular de oito vozes. O
usuário controla tamanho do grão, densidade, posição de leitura, dispersão,
afinação, filtros, envelope, LFO e uma matriz de modulação.

O recorte está na fonte. O material não vem de um sample comum; cada região do
arquivo carrega uma assinatura estatística própria, e o instrumento expõe essa
leitura em quatro vistas: forma de onda, grade de bytes com desmontagem, curva de
entropia e matriz de modulação.

<!-- so o que ja roda -->

## Recursos

- Ingestão de PE com validação de limites e erros tipados (`E_BAD_MZ`,
  `E_BAD_PE`, `E_TRUNCATED`, `E_OOB`, `E_TOO_MANY_SECTIONS`).
- Motor granular de oito vozes, janelas Hann, Gauss, Hamming e Blackman.
- 66 parâmetros automatizáveis: grão, largura estéreo, envelope ADSR com
  curvas, dois filtros biquad em série, LFO, teclado, nuvem de grãos e uma
  matriz de modulação de 8 rotas editável no display.
- Matriz de modulação com oito rotas e oito fontes.
- Presets de fábrica e arquivos `.opcoda` para carregar e salvar.
- Transporte sem MIDI, teclado virtual e controle por MIDI.
- Quatro vistas do material: forma de onda, grade hexadecimal com as colunas
  ASCII e ASM, entropia e matriz de modulação. A coluna ASM desmonta material
  64 bits com um decodificador próprio no núcleo, sem biblioteca externa, e
  escreve com cor por classe de símbolo numa fonte monoespaçada.
- Forma de onda de uma tinta só: o corpo e o contorno delimitam o envelope em
  qualquer zoom, e a rail de vistas é de teclas simples, só com o glifo.
- Curva de entropia em janelas de pelo menos 256 bytes, para que o eixo de 0 a 8
  bits continue verdadeiro em qualquer zoom.
- Ícone próprio: a onda que os bytes fazem, laranja sobre preto, dentro do
  executável e na barra de tarefas.
- Interface fixa de 1180×760, com o display como elemento dominante, o
  transporte no pé do display, medidor stereo de saída e *HOLD*.
- Visualizadores de envelope e de resposta dos filtros no painel dos módulos
  `ENV` e `FILTER`, com pegas que escrevem os mesmos parâmetros dos knobs.
- Desfazer por `Ctrl+Z` no Standalone, um passo por gesto.
- Núcleo C++20 sem dependência de JUCE, testado com AddressSanitizer.

<!-- ambiente de build -->

## Requisitos

- Windows x64.
- Visual Studio 2022 ou 2026 Build Tools com MSVC x64.
- CMake 3.28 ou superior.
- JUCE 8.0.14 em `I:\deps\JUCE` (ou outro caminho via `-DJUCE_DIR=`).
- GoogleTest 1.15.2 via `FetchContent` na primeira configuração.

<!-- build, sem atalho -->

## Compilação

```powershell
.\tools\build.ps1          # debug, AddressSanitizer e testes
.\tools\build.ps1 -Release # release, VST3 e standalone
```

O script carrega `vcvars64.bat`. Não chame `cmake --preset` diretamente.

Para compilar um formato depois da configuração Release:

```powershell
cmake --build --preset vst3
cmake --build --preset standalone
```

Testes:

```powershell
ctest --preset dev -C Debug
```

<!-- instalar o vst3 -->

## Instalação do VST3

```powershell
.\tools\install-vst3.ps1
```

O script exige elevação e instala em `C:\Program Files\Common Files\VST3`.
O Ableton Live ignora o caminho VST3 por usuário.

<!-- primeiro som -->

## Primeiro som

1. Abra o *standalone* ou carregue o VST3 em um *host*.
2. Arraste um `.exe`, `.dll`, `.bin` ou `.sys` para a janela, ou use o ícone de
   pasta.
3. Espere o material aparecer. O tempo de carga depende do tamanho do arquivo.
4. Pressione `Espaço` para percorrer a região sem MIDI, ou toque uma nota no
   teclado virtual.
5. Ajuste `SIZE`, `DENSITY` e `POSITION` para moldar o grão. `SPRAY` espalha a
   posição de leitura de cada grão.

Se o arquivo for recusado, a interface mostra o código do erro e mantém o
material anterior tocando.

No *standalone*, a placa escolhe-se em `SETTINGS`, na barra da janela: duas
páginas (`AUDIO` e `MIDI`), aplicadas ao escolher, sem botão de confirmar. O
instrumento continua a tocar por baixo do painel, e as teclas de nota ainda
chegam ao teclado virtual — dá para segurar uma nota e trocar de placa a ouvir.

<!-- knobs da tela -->

## Controles

| Controle | Função |
| --- | --- |
| `SIZE` | Duração do grão, 1 ms a 2 s. |
| `DENSITY` | Grãos por segundo, 1 a 200. |
| `POSITION` | Ponto de leitura no material, 0 a 100%. |
| `SPRAY` | Dispersão aleatória da posição de cada grão, 0 a 100%. |
| `PITCH` | Transposição em semitons, -48 a +48. |
| `WINDOW` | Envelope do grão. |
| `PAN` | Posição estéreo. |
| `LEVEL` | Nível de cada grão. |
| `P.RAND` | Desvio aleatório de afinação por grão, 0 a 12 st. |
| `SCAN` | Velocidade da varredura automática, 0,25x a 4x. |

`SPRAY` é dispersão pura: não lê entropia. A entropia só mexe em um parâmetro
quando você roteia `Entropy` na matriz de modulação.

A referência completa dos 41 parâmetros está em `docs/PARAMETERS.md`.

Atalhos: `1` a `4` alternam as vistas (onda, hex, entropia, matriz);
`Espaço` liga o transporte; `L` abre um arquivo; `K` leva o foco ao teclado; `A`
alinha a região à seção PE mais próxima; `Ctrl+Z` desfaz o último gesto de
parâmetro (no Standalone; no *host*, o desfazer é o do *host*). O clique num
controle não tira o foco do teclado: tocar notas continua a funcionar em qualquer
vista, e o MIDI que chega do *host* acende as teclas do teclado virtual.

As abas `GRAIN`, `ENV`, `FILTER`, `MOD`, `CLOUD` e `KEYS` mostram ícone e nome. Em
`ENV` e `FILTER`, o painel abre com o desenho do que se ouve — a forma do ADSR
com o ponto vivo do envelope, e a resposta dos dois biquads com o corte já
modulado — e os knobs ficam à direita, menores. As pegas do desenho mudam os
mesmos parâmetros: no envelope, os cantos dão ataque, decay/sustain e release, e
as pegas de meio de segmento dobram as curvas; no filtro, cada pega dá corte (na
horizontal) e Q (na vertical).

| `ENV`: ADSR desenhado, com o ponto vivo | `FILTER`: resposta dos dois biquads |
| --- | --- |
| ![Envelope com o ponto vivo no patamar do sustain](docs/assets/screenshots/opcoda-env.png) | ![Resposta dos filtros com as pegas de corte e Q](docs/assets/screenshots/opcoda-filter.png) |

| `HEX`: bytes, ASCII e ASM | `ENTROPY` a 25 % de zoom |
| --- | --- |
| ![Grade de bytes com a coluna ASM a desmontar o código](docs/assets/screenshots/opcoda-hex.png) | ![Curva de entropia com zoom, em janelas de 256 bytes](docs/assets/screenshots/opcoda-entropy.png) |

<!-- fluxo de audio -->

## Arquitetura de áudio

```mermaid
flowchart LR
  A[Arquivo PE] --> B[Parser e validação]
  B --> C[Bytes para amostras]
  C --> D[Buffer de áudio]
  D --> E[Motor granular<br/>8 vozes]
  E --> F[DC blocker]
  F --> G[Gate ADSR]
  G --> H[Limiter]
  H --> I[Saída estéreo]
  J[LFO e matriz] --> E
  K[Envelope ADSR] --> G
  K -.-> E
```

O núcleo fica em `src/opcoda_core` e não inclui JUCE. O plugin em
`src/opcoda_plugin` conecta o núcleo ao JUCE, ao estado do *host* e à interface.
O callback de áudio não aloca, não bloqueia e não faz I/O. A comunicação entre
interface e áudio passa por átomos e uma fila SPSC.

Detalhes em `docs/SOUND_ENGINE.md`.

<!-- presets -->

## Presets

A interface oferece dez presets de fábrica: Init, Cloud, Shimmer, Pulse, Drone,
Grit, Pad, Keys, Pluck e Bass. Os seis primeiros são texturais; os quatro últimos
são pontos de partida tonais, com o filtro 1 a responder à nota. O menu de preset
carrega e salva arquivos `.opcoda`. O estado do projeto também guarda o material,
a região e a matriz de modulação.

<!-- limites -->

## Requisitos técnicos

- Formato: VST3 e *standalone*.
- Plataforma: Windows x64.
- *Host* verificado: Ableton Live 12.
- Taxa de amostragem: definida pelo *host*.
- Fonte mono, saída estéreo com pan por grão; oito vozes de grão, não oito
  vozes de nota.

<!-- o que falta -->

## Estado do projeto

Implementado e testado: parser PE, motor granular, filtros, LFO, envelope,
matriz, presets, transporte, teclado e as quatro vistas. Os visualizadores de
envelope e de filtro, o desfazer e o espelho do MIDI no teclado foram verificados
por build e captura de ecrã; os testes `Ui.*` da F023 continuam não
implementados.

Pendente ou não verificado:

- Medição de p99 e xruns em sessão real (T2).
- Verificação de arrasto, parâmetros e CCs em *host* (T4).
- Correção dos valores iniciais do *standalone* quando há estado de sessão antigo.
- Formato CLAP.
- Visualização da nuvem de grãos.

<!-- defeito conhecido -->

## Limitação conhecida

O *standalone* pode abrir com valores diferentes dos padrões, por exemplo
`100 ms`, `13 /s` e `69%` em vez de `40 ms`, `20 /s` e `50%`. A causa é o estado
de sessão do JUCE em `%APPDATA%\Opcoda\Opcoda.settings`. Apague esse arquivo
para voltar aos padrões. O defeito está documentado em
`docs/16-arquitetura-implementacao.md`.

<!-- licenca -->

## Licença

O repositório não contém arquivo `LICENSE`. A situação jurídica do código ainda
não foi definida. O projeto usa JUCE 8.0.14, com licença dupla GPL/comercial, e
GoogleTest 1.15.2, com licença BSD-3-Clause. Não redistribua o binário sem
resolver essas licenças.

<!-- docs -->

## Documentação

- `docs/USER_GUIDE.md`: instalação, primeiros passos e operação.
- `docs/SOUND_ENGINE.md`: fluxo de sinal, granulação, spray e entropia.
- `docs/PARAMETERS.md`: referência dos parâmetros.
- `docs/DEVELOPMENT.md`: ambiente, build, testes e depuração.
- `docs/ROADMAP.md`: próximos passos.
- `CHANGELOG.md`: histórico de alterações.
- `docs/atlas/`: inventário e diagramas da arquitetura.

<!-- creditos -->

## Créditos

Referências de síntese granular: Granulator II (Robert Henke) e Quanta 2.
A documentação acadêmica do projeto está em `docs/artigo-opcoda.pdf` e
`docs/monografia_abntex.pdf`.
