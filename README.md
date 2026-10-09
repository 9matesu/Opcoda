# Opcoda

Sintetizador granular em tempo real (VST3, CLAP e Standalone, Windows x64) que
transforma binários Portable Executable em som.

Trabalho de Graduação em Análise e Desenvolvimento de Sistemas, 5º semestre.

## Build

Requer Visual Studio Build Tools 2026, CMake 3.28+ e JUCE 8.0.14 clonado em
`I:\deps\JUCE`. O `cl.exe` depende do vcvars64, então o build passa pelo script:

```powershell
.\tools\build.ps1              # debug + AddressSanitizer + testes
.\tools\build.ps1 -Release     # release, gera VST3 e Standalone
```

## Estrutura

```
src/opcoda_core/    núcleo C++20 puro, sem JUCE
  pe/               parser PE, conversão byte para amostra
  entropy/          entropia de Shannon por janela
  dsp/              motor granular, janelas, DC-blocker, limiter
  rt/               fila SPSC, troca de amostra, guard de alocação
src/opcoda_plugin/  AudioProcessor JUCE, VST3 e Standalone
tests/              GoogleTest, 237 casos mais 8 do guard de alocação
docs/               documentação acadêmica e técnica
specs/              especificações por feature, do spec-kit
```

O núcleo não depende de JUCE, e isso não é estilo: é o que permite rodar o
AddressSanitizer, os testes de borda e o ensaio end-to-end com `notepad.exe`
real sem o framework no caminho.

## Usar

O Standalone e o VST3 abrem com a interface de três faixas: arraste um `.exe`,
`.dll`, `.bin` ou `.sys` para dentro da janela, ou use o ícone de pasta, e o
material toca em até um segundo. Binário recusado mostra o código tipado, como
`E_BAD_MZ`, e o material anterior continua tocando.

São seis parâmetros automatizáveis: tamanho de grão, densidade, posição, spray,
afinação e volume. As notas ligam o motor, o pedal de sustain segura as notas
soltas, e os CCs 74 e 71 movem densidade e posição.

O display tem três vistas da região — forma de onda, grelha de bytes e curva de
entropia — e cada grão ativo aparece onde está a ler o material, com uma cauda
de 24 quadros que mostra a direção da varredura. Sem spray nem pitch a cauda
lê-se como ponto, e isso é honesto: não há direção para mostrar. Na grelha, o
grão marca a célula do byte e a janela ancora no início da região; rolar ou
clicar corta o seguimento, porque quem lê bytes manda. Clicar num byte move a
cabeça de leitura para ele; se o byte estiver fora da região, a região é puxada
para lá primeiro, com o mesmo comprimento. O campo hexadecimal à direita escreve
o endereço da região, e a tecla `A` (ou o duplo-clique) alterna entre a região
exata e a seção PE mais próxima. A região entra no estado do projeto, pelo mesmo
caminho do ficheiro carregado.

A grelha é operada com o rato. Quem só usa teclado move a cabeça de leitura com
o knob `POSITION` e escreve o endereço no campo, e ambos têm foco e nome
acessível. O 2.5.8 do WCAG 2.2 fica com exceção declarada, porque uma célula de
um byte não tem 24 px de largura e a densidade é a função: ver
`docs/10-acessibilidade-w3c.md`.

## Processo

O desenvolvimento segue spec-kit. A constitution em `.specify/memory/constitution.md`
define seis portões de qualidade (build, testes, tempo real, robustez,
interface, documentação) que nenhuma tarefa fecha sem cumprir.

## Documentação

- `docs/01-titulo-e-tema.md` a `docs/05-fundamentacao.md`: tema, problema,
  objetivos e base teórica
- `docs/06-metodologia-arquitetura.md` a `docs/08-cronograma.md`: arquitetura,
  testes e cronograma S1–S5
- `docs/09-similares.md` a `docs/13-topicos-apresentacao.md`: análise de
  similares, acessibilidade WCAG, BPMN e apresentação
- `docs/14-relatorio-apoio.md` e `docs/15-artigo-tg.pdf`: artefatos para entrega
- `docs/16-arquitetura-implementacao.md`: o que o código faz, incluindo os
  defeitos encontrados e as decisões que mudaram
- `docs/17-guia-de-build.md`: toolchain, presets, instalação e as guardas de
  regressão do build
- `docs/18-processo-sdd.md`: constitution, agentes de revisão e rastreabilidade
- `design/DESIGN-SYSTEM.md`: tokens visuais extraídos do Stitch
- `docs/16-arquitetura-implementacao.md`: decisões de implementação e bugs
  encontrados
- `docs/17-guia-de-build.md`: toolchain, presets e limitações declaradas
- `docs/18-processo-sdd.md`: spec-kit, constitution, portões e agentes
- `specs/001-ingestao-pe/spec.md`: ingestão de PE, com as armadilhas encontradas
- `specs/003-troca-material/spec.md`: troca entre interface e áudio
- `specs/008-transporte-e-vistas/spec.md`: transporte de audição e as três leituras
  do material, com a regra da posição 1,0 ser silêncio e as duas armadilhas do
  display

## Estado

Núcleo completo e testado, plugin VST3 e Standalone com a interface de três
faixas, MIDI por nota e CC, e estado do host. 204 casos de teste e o guard de
alocação verdes.

O display tem **três vistas** do material carregado, porque são três perguntas
diferentes sobre a mesma região:

| Vista | Pergunta | Tecla |
| --- | --- | --- |
| forma de onda | que som é este | `1` |
| grelha de bytes | que bytes são estes | `2` |
| curva de entropia | o quanto o material é aleatório | `3` |

O transporte (▶, ou `Espaço`) percorre a região inteira de início a fim **sem nota
MIDI**, que é a única forma de ouvir o material sem um teclado. Com a região de 4
KB ou menos a duração tem um piso de 100 ms, porque abaixo do comprimento do
maior grão uma passagem não dá para completar um grão. O knob POSITION é a âncora
de busca: mexer nele durante a reprodução reposiciona.

**Mapa de teclas.** `Espaço` liga e desliga a reprodução · `←` `→` movem o cursor um
byte · `Shift` com as setas move dezasseis · `↑` `↓` movem uma linha · `PgUp` `PgDn`
movem 1024 bytes · `Início` e `Fim` vão aos extremos · `Enter` ativa o byte sob o
cursor · `Esc` volta o cursor à cabeça de leitura · `1` `2` `3` mudam de vista ·
`A` alinha a região a uma secção PE · `L` abre o ficheiro.

Há um defeito conhecido e **não corrigido**, visível no Standalone: os seis
parâmetros arrancam em valores que não são os defaults declarados em
`createParameterLayout`. Os valores mostram o que fica: `100 ms`,
`13 /s`, `69 %`, onde os defaults são `40 ms`, `20 /s` e `50 %`. Foi confirmado
em `c8006ae` compilado à parte, logo não vem da grelha.

**A causa é o estado de sessão, não memória por inicializar.** O Standalone do
JUCE 8 persiste o estado do plugin em `%APPDATA%\Opcoda\Opcoda.settings` e
restaura-o no arranque; se esse ficheiro tiver parâmetros errados, o plugin abre
com eles. **Apagar esse ficheiro traz os defaults de volta.** O mecanismo foi
confirmado com o Dr. Memory, e o palpite inicial de memória não inicializada
ficou refutado: nenhuma das leituras não inicializadas que ele reporta tem um
frame do nosso código. O que ainda não está identificado é qual sessão escreveu
os valores errados. Detalhe em `docs/16-arquitetura-implementacao.md`.

Falta a validação no Ableton: T2 com medição de p99 e T4 de arrasto, parâmetros e
CCs. As duas etapas estão **bloqueadas**, não atrasadas: não há DAW licenciado
nesta máquina, e a única cópia do Ableton presente não abre janela utilizável sem
licença. O motivo está em `docs/07-plano-testes.md`.
