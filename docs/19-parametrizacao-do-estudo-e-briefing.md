# 19. Parametrização do estudo e briefing para redação do artigo

Este documento existe para uma finalidade prática: dar a quem vai escrever o
artigo — humano ou modelo de linguagem — o estado real do instrumento e o desenho
real do estudo, em forma fechada, para que o texto não invente parâmetro, número
nem resultado. Tudo o que aqui está ou foi medido, ou está marcado como
**proposto** e ainda não existe no código.

Duas regras valem para o artigo inteiro:

1. **Nada de resultado de usuário foi coletado.** O estudo está desenhado e não
   executado. Escrever "os produtores avaliaram" é fabricar dado.
2. **O instrumento é mono.** Não existe polifonia, não existe "oito vozes de
   nota". As oito vozes do motor são oito grãos simultâneos.

---

## Parte 1 — O que o instrumento é

Opcoda é um sintetizador granular que usa um executável Portable Executable (PE)
como matéria-prima e como mecanismo de controle. O utilizador arrasta um `.exe`,
`.dll`, `.bin` ou `.sys` para a janela; o parser valida o cabeçalho, enumera as
seções e converte os bytes da região escolhida em amostras de áudio. A entropia
de Shannon, calculada por janelas de 2048 bytes, vira sinal de controle da
distribuição dos grãos.

### 1.1 Cadeia de sinal

```
arquivo PE
  -> parser com bounds checking (fora do callback)
  -> conversão byte -> amostra + curva de entropia por janela de 2048 B
  -> fila SPSC sem trava (ponteiro cru, acquire-release)
  -> motor granular: 8 slots de grão, janela Hann/Gauss/Hamming/Blackman
  -> envelope global ADSR (curvas por estágio, emenda 1.2.0)
  -> filtro 1 -> filtro 2 (biquads em série, 12/24 dB no primeiro)
  -> DC-blocker (R = 0,995) -> limiter (teto -1 dBFS)
  -> saída estéreo (pan por grão)
```

O que o instrumento lê do PE: assinatura MZ, cabeçalho PE, tabela de seções
(`.text`, `.data`, `.rsrc` e as restantes), `SizeOfRawData` recortado ao tamanho
real do arquivo. O que ele **não** faz: não executa o binário, não o desmonta,
não procura assinaturas de empacotador, não faz análise de malware.

### 1.2 O que ficou de fora, por decisão escrita

- **FM**: o grão é reprodução amostrada, não oscilador. FM real exigiria
  portadora por grão.
- **Seleção inteligente de blocos**: adiada pela própria especificação; a
  entropia já modula a dispersão.
- **CLAP**: previsto na arquitetura, fora da entrega. A entrega é VST3 x64 e
  Standalone para Windows.
- **Polifonia**: nunca existiu. O motor tem um gate e um envelope global.
- **Paridade analógica entre os dois filtros**: o caráter (inclinação, drive,
  keytrack, envelope) entra só no filtro 1. Ver Parte 6.
- **Automação da matriz de modulação**: as 8 rotas vivem no estado, não na lista
  de parâmetros. Ver Parte 6.

### 1.3 Formatos e requisitos

| Item | Valor |
| --- | --- |
| Formatos entregues | VST3 x64 e Standalone, Windows |
| Framework | JUCE 8.0.14; núcleo em C++20 sem JUCE |
| Instalação do VST3 | `C:\Program Files\Common Files\VST3` (elevado) |
| Host de referência | Ableton Live 12.3.1, licenciado |
| Taxa de amostragem | 44,1 e 48 kHz nos ensaios |

---

## Parte 2 — Os 41 parâmetros

Esta é a tabela fechada pela emenda 1.2.0 da constitution. A coluna **estado**
distingue o que já existe no código do que é proposta a congelar na spec: os 41
estão implementados e testados, e a coluna fica como registro histórico do que
entrou em cada fatia (F018–F021).

### 2.1 Grupo A — grão (11)

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `grain` | Grain Size | 1–100 ms | 40 ms | implementado |
| `density` | Density | 1–200 grãos/s | 20 | implementado |
| `position` | Position | 0–1 | 0,5 | implementado |
| `spray` | Spray | 0–1 | 0 | implementado |
| `pitch` | Pitch | −24 a +24 st | 0 | implementado |
| `volume` | Volume | −60 a 0 dB | 0 dB | implementado |
| `window` | Window | Hann, Gaussian, Hamming, Blackman | Hann | implementado |
| `pan` | Pan | −1 a +1 | 0 | implementado |
| `grainlevel` | Grain Level | 0–1 | 1 | implementado |
| `pitchrand` | Pitch Random | 0–12 st | 0 | implementado |
| `scanspeed` | Scan Speed | 0,25–4× | 1× | implementado |

### 2.2 Grupo B — envelope global (7)

O envelope é **global**, aplicado na saída, e não por nota: com o instrumento
mono, um envelope por voz não teria a quem se aplicar.

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `attack` | Attack | 1 ms–2 s | 5 ms | implementado |
| `decay` | Decay | 1 ms–2 s | 100 ms | implementado |
| `sustain` | Sustain | 0–1 | 1 | implementado |
| `release` | Release | 1 ms–2 s | 50 ms | implementado |
| `atkcurve` | Attack Curve | −1 a +1 | 0 | implementado |
| `deccurve` | Decay Curve | −1 a +1 | 0 | implementado |
| `relcurve` | Release Curve | −1 a +1 | 0 | implementado |

Convenção das curvas, igual à do Serum: `0` é linear, `−1` é exponencial (sobe
rápido, aproxima devagar) e `+1` é logarítmica. O valor entra no cálculo como
expoente, e não como tabela: a fase do estágio sobe a esse expoente, sem alocar.

### 2.3 Grupo C — filtros (10)

Dois biquads em série. Coeficientes recalculados só quando corte ou Q mudam,
nunca por amostra.

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `f1type` | Filter 1 Type | Low-pass, High-pass, Band-pass, Notch | Low-pass | implementado |
| `f1cutoff` | Filter 1 Cutoff | 20 Hz–20 kHz (log) | 20 kHz | implementado |
| `f1q` | Filter 1 Q | 0,5–12 | 0,7071 | implementado |
| `f2type` | Filter 2 Type | Low-pass, High-pass, Band-pass, Notch | Low-pass | implementado |
| `f2cutoff` | Filter 2 Cutoff | 20 Hz–20 kHz (log) | 20 kHz | implementado |
| `f2q` | Filter 2 Q | 0,5–12 | 0,7071 | implementado |
| `f1slope` | Filter 1 Slope | 12 dB, 24 dB | 12 dB | implementado |
| `f1drive` | Filter 1 Drive | 0–24 dB | 0 dB | implementado |
| `f1keytrack` | Filter 1 Keytrack | 0–1 | 0 | implementado |
| `f1env` | Filter 1 Env | −1 a +1 | 0 | implementado |

`f1slope` nasce em 12 dB e não em 24, apesar do que esta tabela dizia: 24 dB por
defeito mudaria o som de todas as sessões e presets já gravados, porque o
segundo estágio é um filtro a mais. Quem quer o caráter escolhe-o no knob.

`f1keytrack` a `1` faz o corte seguir a nota tocada (1,0 = 1 oitava por oitava).
`f1env` é bipolar e escala o envelope ADSR em oitavas de corte: ±5 oitavas no
extremo.

### 2.4 Grupo D — LFO (7)

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `lforate` | LFO Rate | 0–20 Hz | 1 Hz | implementado |
| `lfodepth` | LFO Depth | 0–1 | 0 | implementado |
| `lfotarget` | LFO Target | Pitch, Density, Cutoff, Position | Pitch | implementado |
| `lfowave` | LFO Wave | Sine, Tri, Saw, Square, S&H | Sine | implementado |
| `lfosync` | LFO Sync | Free, Sync | Free | implementado |
| `lfodiv` | LFO Division | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64, 1/4., 1/8., 1/4T, 1/8T, 1/16T | 1/4 | implementado |
| `lfosteps` | LFO Steps | 1-16 | 16 | implementado |

Em `Sync`, a taxa vem do tempo do host (BPM e posição em ppq entregues ao núcleo
como escalares, porque o núcleo não conhece JUCE). `lfosteps` quantiza a forma em
degraus: `4` produz quatro patamares por ciclo. As divisões pontuadas e tercinas
estão dentro de `lfodiv`, que é o que evita um segundo parâmetro de "feel".

### 2.5 Grupo E — teclado e escala (3)

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `scale` | Scale | Chromatic, Major, Natural Minor, Harmonic Minor, Dorian, Phrygian, Lydian, Mixolydian, Pentatonic Major, Pentatonic Minor, Blues, Whole Tone | Chromatic | implementado |
| `root` | Root | C, C#, D, D#, E, F, F#, G, G#, A, A#, B | C | implementado |
| `glide` | Glide | 0–2 s (0 = desligado) | 0 | implementado |

A nota MIDI deixa de ser só gate e passa a definir a taxa de reprodução: o grão
lê o material a uma velocidade que corresponde à altura da nota, relativa a
C4 (MIDI 60). Fora da escala, a nota é quantizada ao grau mais próximo, com
prioridade descendente — decisão registada para o teste de borda.

### 2.6 Grupo F — nuvem de grãos (3)

| ID | Nome exibido | Faixa / opções | Padrão | Estado |
| --- | --- | --- | --- | --- |
| `cloudsize` | Cloud Size | 0–1 (0–500 ms) | 0 | implementado |
| `pitchspread` | Pitch Spread | 0–24 st | 0 | implementado |
| `grainjit` | Grain Jitter | 0–1 | 0 | implementado |

`cloudsize` alarga a janela temporal em que os grãos de um mesmo disparo são
distribuídos; `pitchspread` dispersa a afinação de cada grão dentro do intervalo;
`grainjit` varia a duração de cada grão. Com os três a zero, o comportamento é o
atual — e é isso que permite dizer no artigo que a nuvem é um modo, não uma
reescrita do motor.

### 2.7 Contagem

11 + 7 + 10 + 7 + 3 + 3 = **41**.

---

## Parte 3 — O que está medido e o que não está

Esta é a secção que impede o artigo de herdar números falsos. Os valores abaixo
foram lidos do binário e do repositório em 19/10/2026.

| Afirmação | Estado | Evidência |
| --- | --- | --- |
| 289 casos na suíte principal | medido | contagem no binário de `build\tests\opcoda_tests.exe` |
| 8 casos do guard de alocação | medido | binário separado, por desenho |
| Atenuação de DC ≥ 40 dB | medido | T1, FFT N = 65536, janela Blackman-Harris |
| Energia < 20 Hz abaixo de −60 dBFS | medido | T1, contra controle negativo |
| 50 mutações por bit-flip + 10 casos de borda | medido | T3, todas com erro tipado |
| Zero alocação no callback | medido | guard de `operator new`, processo inteiro |
| p99 do `processBlock` no host | **não medido** | T2 agendado; host licenciado disponível desde 19/10 |
| Aceite de arrasto, parâmetros e CCs no host | **não medido** | T4 agendado |
| Ida e volta do estado no host | **não medido** | não há teste automatizado; entra no T4 |
| Cobertura de ramo ≥ 70% | **não medido** | não há `llvm-cov` nesta toolchain |
| TSan | indisponível | o pacote LLVM para Windows não traz o runtime |
| UBSan | indisponível | idem |
| Estudo com produtores | **não executado** | desenhado; ver Parte 4 |
| Áudio publicado | não existe | nenhum áudio do projeto foi publicado |

### 3.1 Defeito conhecido e não corrigido

No Standalone, alguns parâmetros arrancam com valores diferentes dos defaults
declarados: aparecem 100 ms, cerca de 13 grãos/s e 69 % onde os defaults são
40 ms, 20 grãos/s e 50 %. A causa é o estado de sessão persistido em
`%APPDATA%\Opcoda\Opcoda.settings`, restaurado no arranque pelo
`StandalonePluginHolder`. Apagar esse ficheiro devolve os defaults. O Dr. Memory
afastou leitura de memória não inicializada: nenhuma das 35 leituras reportadas
tem um frame do código do projeto. O que falta identificar é **qual sessão
escreveu** os valores errados; os candidatos são os CCs 74 e 71 de um controlador
MiniLab mkII e a automação do host — e ambos passaram a ser testáveis com o host
licenciado.

Este parágrafo deve aparecer no artigo. É a diferença entre um trabalho que
declara o que não fechou e um que esconde.

---

## Parte 4 — O estudo com produtores

### 4.1 Objetivo

Avaliar se o Opcoda é utilizável como instrumento musical por quem produz música,
e não se ele é tecnicamente correto — isso os ensaios T1 a T4 já medem. A
pergunta de usabilidade é outra: o produtor consegue, sem manual, chegar a um som
que considere útil?

### 4.2 Desenho

Estudo de método misto, de grupo único, com quatro tarefas em ordem
contrabalançada por quadrado latino (4×4). Não há grupo de controle: o
comparador implícito é o próprio produtor antes da sessão, e o relato declarado
disso. A escolha é deliberada e deve ser declarada como limitação — sem grupo de
controle não se atribui a melhoria ao instrumento, só se descreve a experiência.

### 4.3 Participantes

| Critério | Definição |
| --- | --- |
| Amostra | 5 a 8 produtores |
| Inclusão | ≥ 2 anos de prática; familiaridade com DAW; uso de ao menos um instrumento virtual |
| Inclusão (perfil B) | produtores iniciantes são aceitos, e a análise separa os dois perfis |
| Exclusão | envolvimento no desenvolvimento do Opcoda; impossibilidade de dar consentimento |
| Amostragem | conveniência + bola de neve |

A separação por perfil existe para não confundir falta de habilidade com falta de
usabilidade. Com 5 a 8 participantes, o perfil B pode ficar com 1 ou 2 pessoas —
e nesse caso a comparação entre perfis é **descritiva, nunca inferencial**.

### 4.4 Recrutamento — o processo, em detalhe

Este é o trecho que o artigo precisa descrever com precisão, porque é o que
permite replicar o estudo.

1. **Divulgação (dias 1 a 5).** Convite direto em grupos de produção musical da
   região e chamada nas redes sociais dos cursos de música e produção. O texto do
   convite deve dizer o que é o instrumento, quanto tempo dura a sessão, que não
   há recompensa financeira e que a desistência é livre.
2. **Triagem (dias 3 a 8).** Formulário curto com quatro itens: anos de prática,
   DAW que usa, instrumentos virtuais que usa, disponibilidade de horário. A
   triagem não é avaliação de habilidade; serve para confirmar os critérios e
   equilibrar os perfis.
3. **Agendamento (dias 5 a 10).** Sessões individuais de 60 a 75 minutos, no
   mesmo equipamento e na mesma sala, para que a variação de máquina não entre
   como variável.
4. **Confirmação (24 h antes).** Mensagem com local, duração, o que trazer
   (nenhum material é necessário) e o termo de consentimento em anexo para
   leitura prévia.
5. **Sessão (dias 10 a 20).**
6. **Devolutiva (opcional, após a análise).** Resumo de uma página com os
   indicadores agregados, sem identificar participantes.

Riscos do recrutamento que o artigo deve declarar: a amostra é de conveniência,
concentrada em uma região e em uma rede de contatos; quem aceita participar
tende a ter interesse prévio pelo tema, o que enviesa a favor; e o período de
recrutamento coincide com o fim do semestre, o que reduz a adesão.

### 4.5 Ética

O protocolo segue a Resolução 466/2012 do Conselho Nacional de Saúde, com o
complemento da Resolução 510/2016 para as ciências humanas e sociais, e inclui:

- consentimento livre e esclarecido, por escrito, com objetivo, procedimento,
  gravação, tratamento e guarda dos dados;
- sigilo: os participantes são identificados por código (P1 a P8), nunca por nome;
- retirada a qualquer momento, sem justificativa e sem prejuízo;
- ausência de recompensa e de qualquer forma de coerção;
- gravação de áudio, quando necessária à análise do diário, explicitada no
  consentimento e destruída ao fim do prazo de análise;
- dados brutos guardados pelo prazo declarado e depois eliminados.

Submissão a CEP: se o trabalho for submetido a comitê, o número do parecer entra
no artigo. Se não for, o artigo deve dizer que o protocolo seguiu a resolução sem
submissão formal — e essa é uma limitação, não um detalhe.

### 4.6 Materiais

- bundle VST3 e aplicativo Standalone, na mesma versão, instalados na máquina da
  sessão;
- três executáveis de referência, escolhidos por diversidade estatística: um
  binário de sistema pequeno, um binário comprimido com empacotador e um arquivo
  grande;
- um arquivo inválido (por exemplo, um `.txt`) para a tarefa de erro;
- controlador MIDI com pelo menos um knob, para o mapeamento de CC;
- ficha de observação, diário de sessão e os dois questionários.

### 4.7 Tarefas

Quatro tarefas, em ordem contrabalançada. Cada tarefa tem um objetivo observável,
e é isso que a impede de virar conversa.

| # | Tarefa | O que o participante faz | O que se observa |
| --- | --- | --- | --- |
| 1 | Carregamento | arrasta um dos três executáveis e relata o tempo percebido até o primeiro som | tempo até o primeiro som útil, em segundos |
| 2 | Exploração | percorre os parâmetros e produz dois ajustes que mudem o som de forma satisfatória | quais parâmetros usa sem instrução; quantos ignora |
| 3 | Performance | toca 2 minutos com notas MIDI e pedal, mapeando ao menos um CC | se o mapeamento expande ou restringe o que consegue fazer |
| 4 | Erro | carrega um arquivo inválido e relata o que aconteceu | se a mensagem tipada é compreendida sem explicação externa |

Instrução ao participante, palavra por palavra, para o artigo: *"Você vai usar um
instrumento que transforma arquivos executáveis em som. Não há resposta certa.
Pense em voz alta quando puder. Eu não vou ajudar durante as tarefas; se algo
travar, diga, e eu anoto."*

### 4.8 Instrumentos de coleta

1. **SUS** (Brooke, 1996): 10 itens, escala de 5 pontos, nota final de 0 a 100.
   Aplicado ao fim, sobre o instrumento inteiro.
2. **Questionário de atributos**: para cada um dos 41 parâmetros (ou para o
   subconjunto que o participante usou), uma pergunta de três opções — "não
   percebi mudança", "percebi mudança sem saber o que faz", "percebi e sei o que
   faz". Mede percepção, não satisfação.
3. **Diário de sessão**: uma linha por ajuste, com duas colunas — "o que eu
   esperava" e "o que eu ouvi".
4. **Telemetria do plugin**: registada em arquivo durante os dois minutos de
   performance, com grãos ativos, pico de saída e entropia da janela de leitura.
   **Sem captura de áudio.**

### 4.9 Roteiro da sessão (60–75 min)

| Min | Bloco |
| --- | --- |
| 0–10 | acolhimento, leitura e assinatura do consentimento, apresentação do instrumento em 60 segundos |
| 10–20 | tarefa 1, carregamento, com os três executáveis |
| 20–35 | tarefa 2, exploração livre |
| 35–50 | tarefa 3, performance com MIDI e CC |
| 50–58 | tarefa 4, erro |
| 58–68 | SUS e questionário de atributos |
| 68–75 | entrevista curta: o que mudaria primeiro, o que não mudaria |

### 4.10 Dados e planilha

Uma linha por participante, com as colunas:

```
id, perfil, anos_pratica, daw, tarefa_ordem,
t1_tempo_percebido_s, t1_tempo_observado_s, t1_exe,
t2_params_usados, t2_params_ignorados, t2_ajustes_satisfatorios,
t3_cc_mapeado, t3_notas_tocadas, t3_duracao_s,
t4_mensagem_compreendida,
sus_item_1 ... sus_item_10, sus_score,
attr_param_01 ... attr_param_41,
diario_linhas, observacoes
```

### 4.11 Análise

- **Quantitativa:** estatística descritiva — mediana e intervalo interquartílico
  para o SUS; mediana e amplitude para o tempo até o primeiro som; contagem e
  percentagem para parâmetros usados sem instrução e para executáveis
  indistinguíveis sem ouvir. **Sem teste de significância.** Com 5 a 8
  participantes, um p-valor seria ruído com aparência de resultado.
- **Qualitativa:** tematização do diário por dois codificadores independentes,
  com comparação das codificações e resolução de divergência por discussão. O
  acordo entre codificadores pode ser reportado por kappa de Cohen, e o valor
  deve ser lido como indicador de consistência, não como validação.
- **Triangulação:** o que o participante diz (diário), o que ele faz (observação)
  e o que o instrumento registra (telemetria) são confrontados. Quando os três
  discordam, a discordância é o achado.

### 4.12 Indicadores principais

1. **Tempo até o primeiro som útil** — o que separa um instrumento de uma
   demonstração.
2. **Parâmetros usados sem instrução** — o que a interface comunica sozinha.
3. **Executáveis que o produtor não distingue sem ouvir** — o que a leitura do
   binário entrega de facto ao ouvido.
4. **SUS mediano** — impressão global, com a ressalva de que SUS não foi validado
   para instrumentos musicais.
5. **Tarefas concluídas sem ajuda** — 0 a 4 por participante.

### 4.13 Ameaças à validade

| Ameaça | Efeito | Mitigação declarada |
| --- | --- | --- |
| n = 5 a 8 | nenhuma inferência | análise descritiva, declarada |
| amostra de conveniência | viés de interesse prévio | declarada como limitação |
| um só sítio e uma só máquina | efeito de contexto | mesma sala e mesmo equipamento |
| ordem das tarefas | aprendizagem e fadiga | quadrado latino |
| efeito do pesquisador | instrução variável | roteiro escrito, palavra por palavra |
| novidade | entusiasmo inicial | tarefa de exploração com tempo fixo |
| telemetria sem áudio | não valida o que se ouviu | triangulação com diário e observação |

---

## Parte 5 — Perguntas de pesquisa

Escritas antes de qualquer sessão, e são as que a análise responde.

1. **Controlabilidade.** O produtor consegue, com os parâmetros do Opcoda, uma
   variedade de sons que reconheça como diferentes entre si?
2. **Tempo.** Quanto tempo leva, em minutos, para produzir o primeiro som que
   considera útil, e esse tempo depende do executável escolhido?
3. **Curva de aprendizagem.** Quais parâmetros usa sem instrução, e quais ignora?
4. **Leitura do binário.** Percebe diferença audível entre os três executáveis, e
   essa diferença corresponde a algo que consiga nomear?
5. **MIDI.** O mapeamento por nota e controlador expande ou restringe o que
   consegue fazer?
6. **Erro.** A mensagem de erro do parser é compreendida sem explicação externa?
7. **Preferência.** Qual dos três executáveis escolheria para uma produção real,
   e por quê?
8. **Recomendação.** O que mudaria primeiro no instrumento, e o que não mudaria
   mesmo com liberdade total?

---

## Parte 6 — Os dois desenhos declarados fora da tabela

Esta secção existe para que o artigo possa descrever o instrumento completo sem
mentir sobre o que está implementado, e para que a próxima versão saiba
exatamente o que restaurar.

### 6.1 Matriz de modulação: 8 rotas no estado

Cada rota é um terno:

```
{ source, destination, amount }
```

- **Fontes:** LFO 1, envelope 1, envelope 2, velocity, keytrack, mod wheel, CC 74,
  CC 71, entropia da janela de leitura.
- **Destinos:** pitch, density, position, spray, grain size, cutoff 1, Q 1,
  cutoff 2, Q 2, pan, grain level, drive.
- **Profundidade:** −1 a +1, somada ao valor do parâmetro e limitada à faixa.

As 8 rotas são avaliadas por bloco, com o resultado escrito em memória
pré-alocada. **Não são parâmetros do APVTS**: vivem como propriedades da árvore e
viajam no estado do projeto e no preset `.opcoda`. Consequência declarada: o host
não as automatiza, não as mostra no painel de parâmetros e não faz undo por rota.

Torná-las automatizáveis custaria 3 parâmetros por rota — 24 no total, 65 na
tabela. Fica como desenho completo para uma emenda futura, com o custo escrito à
frente, que é o que permite decidir.

### 6.2 Paridade do filtro 2

O filtro 1 ganha inclinação, drive, keytrack e profundidade de envelope. O filtro
2 mantém tipo, corte e Q. Restaurar a paridade custa quatro parâmetros
(`f2slope`, `f2drive`, `f2keytrack`, `f2env`) e levaria a tabela a 45. A
assimetria é declarada, e não acidental.

---

## Parte 7 — Regras de redação para quem escreve o artigo

O que **não** pode aparecer no texto, e por quê:

| Não escrever | Porque é falso |
| --- | --- |
| "instrumento polifônico de oito vozes" | é mono; oito são grãos simultâneos |
| "25 parâmetros" ou "seis parâmetros" | são 41 na versão 1.2.0; seis era o MVP de outubro |
| "os produtores avaliaram / relataram" | o estudo não foi executado |
| "SUS médio de X" | não há dado coletado |
| "CLAP" como formato entregue | CLAP é pós-entrega |
| "latência p99 de X ms" | o T2 está agendado, não medido |
| "cobertura de 70%" | não há ferramenta de cobertura nesta toolchain |
| "validado por usuários" | não houve validação com usuários |
| "TSan limpo" | TSan não existe no Windows com esta toolchain |
| "automatização da matriz pelo host" | as rotas não são parâmetros |

O que **pode** ser afirmado, com o número certo:

- 289 casos automatizados mais 8 do guard de alocação, medidos em 19/10/2026;
- atenuação de DC ≥ 40 dB e energia abaixo de 20 Hz < −60 dBFS no ensaio
  espectral;
- 50 mutações por bit-flip e 10 casos de borda, todos com erro tipado, sem
  segfault;
- zero alocação no caminho de áudio, verificado por substituição global de
  `operator new`;
- núcleo sem JUCE, o que permite rodar ASan e os testes de borda sem o framework
  no caminho;
- 41 parâmetros automatizáveis, com os IDs de 1.1.0 preservados.

---

## Parte 8 — Referências que o artigo cita

As que já estão em `docs/referencias.md` e sustentam este texto:

- SHANNON, C. E. A Mathematical Theory of Communication. 1948.
- ROADS, C. Microsound. MIT Press, 2001.
- BROOKE, J. SUS: A quick and dirty usability scale. 1996.
- NIELSEN, J. Usability Engineering. 1993.
- STEINBERG. VST 3 SDK documentation. 2024.
- CLAP. CLever Audio Plugin Specification. 2024.
- PIRKLE, W. Designing Audio Effect Plugins in C++. 2019.
- ZÖLZER, U. DAFX: Digital Audio Effects. 2011.
- LYDA, R.; HAMROCK, J. Using Entropy Analysis to Find Encrypted and Packed
  Malware. 2007.
- Microsoft. PE Format Specification.
- Conselho Nacional de Saúde. Resolução 466/2012 e Resolução 510/2016.

O que **não** se cita porque não foi usado: nenhuma referência a testes com
usuários que não sejam as de método (Brooke, Nielsen). Não há estudo anterior
com instrumentos que leiam binários, porque não existe — e essa ausência é um
argumento, não uma lacuna a disfarçar.

---

## Parte 9 — Cronologia para o artigo

| Data | Marco |
| --- | --- |
| 01/10/2026 | constitution 1.1.0: ADSR, dois filtros, LFO, 25 parâmetros |
| 06/10/2026 | fecha o critério 2.5.8 de acessibilidade com caret de teclado |
| 08/10/2026 | presets de fábrica e formato `.opcoda` |
| 19/10/2026 | emenda 1.2.0: mono, 41 parâmetros, matriz no estado, OpenGL; host licenciado confirmado |
| 19–21/10/2026 | T4, ida e volta do estado, baseline do T2 |
| 21–31/10/2026 | F018 a F024 |
| 01–02/11/2026 | congelamento, docs sincronizados, artigo e monografia |
| 03/11/2026 | entrega |

A cronologia importa para o artigo porque explica por que a versão descrita não é
a versão do MVP de outubro: o instrumento cresceu depois da primeira redação, e o
texto tem de refletir a versão congelada, não a lembrança dela.
