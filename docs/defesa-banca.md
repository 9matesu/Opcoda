# Documento de defesa — Opcoda

Material de estudo para a banca. O texto segue a voz do autor e cobre o que o
trabalho é, o que faz, como está construído, os pontos fortes e fracos, a
justificativa da escolha e as perguntas que a banca costuma fazer.

## 1 O que é o Opcoda

O Opcoda é um sintetizador granular em tempo real para Windows x64 que lê
arquivos Portable Executable — `.exe`, `.dll`, `.bin`, `.sys` — como matéria-prima
sonora. O binário é o material: o programa converte os bytes do arquivo em
amostras normalizadas e os reproduz como uma nuvem de grãos de 1 ms a 2 s,
organizada por um motor de oito vozes com sobreposição. Existe em duas formas,
VST3 para dentro do DAW e *standalone* para tocar sozinho, na versão 0.1.0,
ainda sem lançamento comercial.

A escolha do formato não é decorativa. Um executável carrega, em cada região,
uma assinatura estatística própria: código de máquina com entropia intermediária,
tabelas e recursos com distribuições enviesadas, áreas comprimidas ou cifradas
perto dos 8 bits por byte. O instrumento expõe essa leitura em quatro vistas do
material — forma de onda, grade de bytes com desmontagem, curva de entropia e
matriz de modulação — e deixa o usuário navegar por essa estatística com
parâmetros de grão, afinação, filtros, envelope e modulação. O resultado sonoro
é a matéria; a estrutura do arquivo é o mapa.

## 2 O que ele faz na prática

O fluxo é direto. O usuário carrega um arquivo no campo do cabeçalho ou arrasta
o binário para o display; o programa valida a assinatura MZ e PE, confere os
limites de cada seção contra o tamanho real do arquivo e expõe o material nas
quatro vistas. O transporte — Play, Hold e os botões de página — varre o arquivo
em velocidade ajustável de 0,25 a 4 vezes; as teclas do computador ou um
controlador MIDI tocam notas, e cada nota dispara grãos lidos da posição atual
com o tamanho, a densidade, a dispersão e a afinação escolhidos.

O painel de parâmetros organiza o som em módulos. O Grain define a nuvem: tamanho
do grão, densidade, posição, spray, afinação, glide e a janela (Hann, Gauss,
Hamming ou Blackman). O Envelope desenha o ADSR com curvas por estágio. O Filter
oferece dois filtros biquad em série, com tipo, corte, ressonância, slope,
drive, keytracking e amount de envelope. O Mod traz o LFO com taxa, profundidade,
alvo, forma de onda, passos e sincronização com o tempo do host. O Master reúne
volume, panorama, largura estéreo e nível do grão, com medidor vertical. Por fim,
a matriz de modulação roteia até oito rotas entre cinco fontes — velocity,
envelope, LFO, keytracking e mod wheel — e treze destinos, cada célula com o
amount em percentagem, arrastável para cima e para baixo.

Tudo isso cabe numa janela de 1180 px de largura com o display como elemento
dominante e um teclado MIDI que desliza para fora do chassi quando aberto. O
*standalone* dispõe ainda de desfazer por Ctrl+Z, presets de fábrica e arquivos
`.opcoda` para gravar e recuperar o estado do instrumento.

## 3 Como está construído

A arquitetura parte de uma divisão que o áudio exige na prática: duas threads
com regras distintas. A thread de interface pode alocar memória, abrir arquivos e
demorar; a thread de áudio é chamada a cada 128 amostras — cerca de 2,9 ms a
44,1 kHz — e não pode alocar, travar, fazer I/O ou lançar exceção. O projeto
respeita essa divisão no código, não no papel.

O núcleo, `src/opcoda_core/`, é C++20 puro, sem nenhum cabeçalho do JUCE. Essa
separação não é estilo: é o que permite rodar sanitizers e testes sobre o parser
e o DSP sem o framework no caminho, e o build falha se alguém incluir algo de
fora. O núcleo reúne quatro famílias: o parser PE com verificação de limites;
a entropia de Shannon; o DSP com o motor granular, janelas, filtros, DC-blocker
e limitador; e o tempo real com a fila SPSC, a troca de amostra e o guard de
alocação. O plugin, `src/opcoda_plugin/`, envolve esse núcleo com o JUCE 8, o
editor, os 61 parâmetros automatizáveis e a matriz.

A cadeia de sinal é curta e auditável: bytes convertidos em float normalizado,
motor granular de oito vozes com *overlap-add*, DC-blocker com coeficiente
0,9983 — corte em 12 Hz a 44,1 kHz — e limitador com teto de −1 dBFS e clamp
final. Cada grão nasce com a janela no índice zero, o que zera a amplitude nas
bordas e evita o clique da sobreposição; o interpolador é linear, com passo
limitado entre 0,0625 e 16 para impedir *aliasing* audível na afinação máxima de
±48 semitones. As janelas são quatro tabelas de 4096 pontos preenchidas uma vez
no `prepare`, e cada grão guarda a tabela com que nasceu — mudar a janela não
salta o envelope de um grão longo a meio.

A entropia é a frequência de símbolos, não análise espectral: H(X) = −Σ p(xi)·log2 p(xi), com o alfabeto de 256 valores do byte, mede a informação média por símbolo e satura em 8 bits por byte quando todos os valores são equiprováveis. A escolha é teórica, não decorativa: o material é um fluxo de símbolos, não uma onda, e a estatística canônica de uma fonte de símbolos é a entropia — a FFT seria erro de categoria, análise espectral do que ainda não é espectro. O cálculo desliza em janelas de 2048 bytes (a fundamentação registra a faixa de 256 a 1024 como referência comparativa), porque um número único para o arquivo inteiro não serve à navegação: o perfil entrópico é o mapa. Nele, regiões de zeros e áreas não inicializadas ficam perto de 0, código compilado fica no meio-termo — distribuição enviesada de opcodes e operandos —, e blocos comprimidos, empacotados ou cifrados encostam nos 8 bits, comportamento análogo ao ruído estocástico de banda larga (LYDA; HAMROCK, 2007; SHANNON, 1948). O eixo de 0 a 8 bits permanece verdadeiro em qualquer zoom, e o custo é uma histograma de 256 caixas por janela — nenhum caminho de áudio usa FFT; a transformada existe apenas no ensaio T1, que mede a saída do motor.

A entropia não é só mapa: é sinal de controle. O valor de H na posição de leitura entra na matriz como fonte — o Entropy-to-CV da fundamentação —, de sorte que regiões repetitivas ancoram o grão e regiões densas o dispersam; a mesma curva desenha a vista de entropia que o usuário usa para navegar. O instrumento não toca os bytes: toca a incerteza dos bytes.

A segurança do parser segue três princípios. O arquivo é aberto somente para
leitura, copiado para um buffer próprio e fechado em seguida; nenhum trecho
recebe permissão de execução. Toda leitura indexada por campos do cabeçalho —
`e_lfanew`, `NumberOfSections`, `PointerToRawData`, `SizeOfRawData` — passa por
conferência de limites em aritmética de 64 bits, de sorte que `offset + tamanho`
nunca transborda antes da comparação. Valores fora de faixa implicam rejeição
com erro tipado — `E_BAD_MZ`, `E_BAD_PE`, `E_TRUNCATED`, `E_OOB`,
`E_TOO_MANY_SECTIONS` — e nenhuma exceção atravessa a ABI do plugin. O parser
tem ainda um decodificador próprio de x86-64 para a coluna ASM da vista
hexadecimal, sem biblioteca externa.

A comunicação entre interface e áudio é lock-free: fila de produtor único e
consumidor único, parâmetros atômicos com suavização, e um guard de alocação que
substitui o `operator new` global do processo para vigiar o callback de áudio —
por isso ele vive num binário de teste separado, o `opcoda_alloc_guard_test`, e
não na suíte principal.

## 4 Pontos fortes

O primeiro é a separação em camadas. O núcleo C++20 puro, com zero dependência
de JUCE, é testável isoladamente: a suíte `opcoda_tests` passa de 150 casos entre
parser, entropia, DSP e tempo real, e o build de desenvolvimento roda com
AddressSanitizer. O parser PE tem testes dedicados — inclusive um PE mínimo
construído em memória em `tests/pe_builder.h`, que foi o que revelou o bug de
leitura em `eLfanew + 2`, campo que pertence à assinatura e não ao cabeçalho
COFF. Esse bug só apareceu porque o teste existia; é o argumento mais concreto
a favor da arquitetura.

O segundo é o regime de tempo real. O callback de áudio não aloca, não trava,
não faz I/O e não lança exceção, e essa regra é garantida por código e não por
revisão: o guard de alocação vigia o `operator new` global do processo. A
comunicação com a interface é fila SPSC e átomos, sem mutex no caminho do áudio.
O resultado é um instrumento que se comporta sob carga — o que a banca de áudio
costuma perguntar e o que separa um projeto de sonificação de um instrumento.

O terceiro é a robustez do parser. Leitura com limites verificados, truncamento
de `SizeOfRawData` e `NumberOfSections` ao que existe de fato, rejeição de
seções acima de 96 e erros tipados em vez de exceções — o parser devolve erros
que a interface traduz, nunca um crash. O Dr. Memory confirmou, em sessão de
depuração, que não há quadro de memória não inicializada vindo do nosso código.

O quarto é a interface como produto. O visual é próprio — sem *widgets*
genéricos do JUCE: knobs de arco com parâmetros e valores desenhados à mão,
ícones autorais, a gaveta do teclado MIDI fora do chassi, o LFO como
osciloscópio que corre com a fase viva, a matriz com células arrastáveis, o
medidor vertical e o display com quatro vistas. A tipografia, o espaçamento e as
hairlines seguem tokens e uma paleta única, e o resultado é um instrumento
que se apresenta como produto comercial e não como protótipo de faculdade.

O quinto é a entrega completa. O projeto traz instalador profissional em inglês
— wizard com licença, componentes, pastas, peso total, *Visual C++ 2015-2022*
com consentimento e deteção, instalação silenciosa e desinstalação —, manual em
PDF com todos os parâmetros documentados e a documentação de arquitetura em
`docs/`, com atlas de diagramas e processo de desenvolvimento registrado em
*constitution* e *spec-kit*. Poucos trabalhos de TG entregam o produto
empacotado; este entrega.

## 5 Pontos fracos

O primeiro é o escopo do instrumento. O Opcoda é mono: uma tecla de cada vez,
com prioridade à última nota; as oito vozes do motor são *slots* de grão
simultâneos, não polifonia de notas. Há um único envelope, um único LFO e a
matriz limita-se a oito rotas. Para um sintetizador comercial, isso é pouco; para
o recorte do trabalho — um instrumento de textura granular sobre material
binário —, é coerente, mas a banca pode cobrar.

O segundo é a plataforma. O projeto roda em Windows x64 apenas; não há macOS,
Linux, VST2, AU ou AAX, e o VST3 é estático, sem dependências de *runtime* mas
preso ao JUCE 8.0.14 fixado no build. A escolha foi deliberada — o alvo é o
Ableton Live no Windows, e o formato VST3 cobre o essencial —, mas é uma
limitação real de alcance.

O terceiro é a cobertura de testes. O núcleo e o parser têm suíte própria, com
ASan e *fuzzing*; a interface não tem suíte automatizada — os testes de UI
continuam fora do escopo —, e a validação visual é feita por captura de ecrã e
análise de pixéis, método que funciona mas não escala. Não há *profiling*
extensivo de áudio nem teste de carga de horas; o que existe é a verificação
por bloco e a disciplina de tempo real no código.

O quarto é um defeito conhecido. O *standalone* às vezes abre com valores de
parâmetros errados — 100 ms onde o padrão é 40 ms —, e a causa é o estado de
sessão do JUCE em `%APPDATA%\Opcoda\Opcoda.settings`, arquivo stale. O Dr.
Memory confirmou que não vem de memória não inicializada no nosso código; o
escritor dos valores errados continua não identificado. A mitigação existe —
apagar o arquivo de sessão restaura os padrões —, mas o defeito é real e fica
registrado.

O quinto é a portabilidade do build. O script `tools/build.ps1` carrega o
`vcvars64.bat` com caminhos fixos — Visual Studio Build Tools, CMake em
`C:\Program Files\CMake\bin\`, JUCE em `I:\deps\JUCE` — e funciona na máquina do
projeto; em outra máquina, exige ajuste de caminhos. É uma escolha de
reprodutibilidade local, não de portabilidade.

E há o que não existe: o *standalone* toca mas não grava nem exporta áudio — a
captura fica a cargo do host —, e o instrumento não lê formatos WAV/AIFF como
material; o binário é o único insumo. São fronteiras do recorte, e a banca pode
perguntar por elas.

## 6 Por que o projeto foi escolhido

O ineditismo está na fonte. Samplers granulares comerciais — Portal, Padshop 2,
Fragments — só aceitam WAV e não entendem estrutura PE; a sonificação forense de
binários, tipo BinVis, é trabalho offline em Python, sem requisito de tempo real;
o *raw import* do Audacity é offline e sem tratamento. Não existia, no
universo que o projeto mapeou, um instrumento em tempo real que lesse um
executável como matéria-prima sonora. O Opcoda ocupa essa intersecção: um parser
PE seguro de um lado, um motor granular em tempo real do outro.

A escolha tem três pernas, e cada uma se sustenta sozinha. A análise estática de
binários entra pela segurança — leitura somente de leitura, limites verificados,
erros tipados, nenhuma execução; a teoria da informação entra pela entropia, e
aqui a justificativa é mais fina: Shannon (1948) é fundacional fora da música e
nichada dentro dela, e o projeto importa H(X) não como enfeite, mas como a
linguagem correta do material. Bytes são símbolos, e a entropia é a medida
canônica da informação de uma fonte de símbolos; a mesma estatística que comprime
arquivos, que distingue binários empacotados e cifrados (LYDA; HAMROCK, 2007) e
que alimenta a detecção de malware é a que aqui move o grão. Uma teoria
central no mundo e nichada no estúdio ganha, dentro da produção musical, um uso
que nenhuma ferramenta comercial oferece — parâmetro de composição, não
sonificação de dados; e a engenharia de tempo real rígida entra pela arquitetura
— duas threads, fila SPSC, zero alocação no callback, guard de alocação.
Nenhuma das três é decorativa: as três aparecem no código e nos testes.

Da entropia como parâmetro abrem-se intersecções ainda menos ortodoxas de
produção musical. A curva de H pode virar *arrangement* — as seções do arquivo
como seções da faixa; pode ditar densidade e tamanho do grão, regiões densas
pedindo grãos longos e regiões planas pedindo grãos curtos; pode selecionar
material — varrer uma pasta de binários e escolher os de H perto de 8, ruidosos,
ou perto de 0, quase silenciosos; e pode dobrar sobre si mesma — a entropia da
entropia, a velocidade de complexidade do material, como modulador. Nenhuma
dessas ligações existe em sampler comercial: o Opcoda é um instrumento
estatístico, e é aí que a produção musical encontra a teoria da informação.

Para um aluno de ADS, o valor do trabalho está na arquitetura — o som é só parte
do resultado. O projeto demonstra, na prática, a divisão entre thread de
interface e thread de áudio, a comunicação lock-free, a memória estática no DSP
e a leitura com permissão mínima, que são os temas que separam um projeto de
sonificação de um brinquedo. O nome sela o conceito: bloodwormtrax, o verme que
consome o binário, e o Opcoda como o instrumento que faz o arquivo soar.

## 7 Como defender

Quando perguntarem o que o trabalho faz, a resposta é a matéria: o Opcoda é um
sintetizador granular que lê executáveis como material sonoro; o usuário toca
notas, e cada nota dispara grãos lidos do binário, com tamanho, densidade,
posição, afinação, filtros, envelope e modulação controlados em tempo real.
Quando perguntarem por que um binário, a resposta é a estatística: cada seção do
arquivo tem uma assinatura própria — código com entropia intermediária, recursos
enviesados, áreas comprimidas perto de 8 bits —, e o instrumento expõe essa
leitura em quatro vistas e a transforma em parâmetro de síntese.

Quando perguntarem por que entropia e não análise espectral, a resposta é a
natureza do material: bytes são símbolos, não onda; H(X) = −Σ p·log2 p é a
estatística canônica de uma fonte de símbolos, e a FFT seria erro de categoria.
A entropia dá um eixo universal de 0 a 8 bits com interpretação física por
região — zeros perto de 0, código no meio, comprimido perto de 8 — e é a mesma
medida que comprime arquivos e detecta binários empacotados: teoria central no
mundo, nichada no estúdio. E habilita o que nenhuma ferramenta comercial
oferece: modular por densidade de informação, a curva do arquivo virando
arrangement, o grão seguindo a incerteza do material.

Quando perguntarem sobre tempo real, a resposta é o guard: o callback de áudio
não aloca, não trava, não faz I/O e não lança exceção; a regra é vigiada por um
`operator new` global substituído num binário de teste, e a suíte roda com
AddressSanitizer. Quando perguntarem sobre segurança, a resposta é o parser:
leitura somente de leitura, limites em aritmética de 64 bits, erros tipados e
nenhuma exceção atravessando a ABI do plugin.

Quando apontarem o mono, a resposta é o recorte: o instrumento é mono por
desenho — uma nota por vez, com glide —, e as oito vozes do motor são *overlap*
de grãos; o escopo é textura granular, não polifonia. Quando apontarem o bug do
*standalone*, a resposta é a depuração: o Dr. Memory confirmou que a memória
não inicializada não vem do nosso código; a causa é o estado de sessão do JUCE,
a mitigação é apagar o arquivo de sessão, e o defeito fica documentado em
`docs/16-arquitetura-implementacao.md`.

Quando perguntarem o que foi feito de diferente, a resposta é a entrega: o
projeto traz instalador profissional com consentimento de dependências, manual
em PDF, documentação de arquitetura com atlas, e um processo de desenvolvimento
registrado — *constitution* com seis *gates* de qualidade (build, testes, tempo
real, robustez, UI, documentação) e *spec-kit* com especificações versionadas.
O que a banca vê é um produto, não um trabalho de gaveta.