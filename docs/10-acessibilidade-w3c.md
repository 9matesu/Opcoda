# 10. Análise W3C aplicada ao TG

## Técnica escolhida

WCAG 2.2 nível AA, aplicada pela metodologia WCAG-EM (Website Accessibility Conformance Evaluation Methodology) do W3C. A escolha cobre dois artefatos do TG: a documentação em `docs/` (artefato existente, auditável hoje) e o protótipo da GUI do plugin (alvo com critérios de aceite definidos antes da implementação).

Ferramentas: W3C Nu Html Checker na versão renderizada dos docs, axe DevTools e medição manual de contraste.

## Escopo e amostra

Escopo: páginas renderizadas de `README.md`, `docs/proposta-completa.md` e `docs/08-cronograma.md`, mais a tela principal planejada da GUI (mapa de seções, waveform, controles).

Amostra: três páginas de docs e três telas da GUI (principal, parâmetros, diálogo de erro do parser).

## Achados na documentação (verificados)

| Critério WCAG | Página | Resultado | Evidência e correção |
| --- | --- | --- | --- |
| 2.4.4 Finalidade do link | proposta-completa | Falha parcial | Cinco links com o texto "Ver detalhes em 04-objetivos.md". O destino só fica claro pelo contexto imediato. Correção: trocar por textos descritivos como "detalhes dos objetivos em 04-objetivos.md". |
| 1.3.1 Informação e relações | 08-cronograma | Passa | Tabela com linha de cabeçalho (Etapa, semestres, entrega), que o renderizador converte em `th` com escopo. |
| 2.4.6 Cabeçalhos e rótulos | todas | Passa | Hierarquia sem saltos: título, seções, subseções. |
| 3.1.1 Idioma da página | todas | Atenção | Docs escritos em pt-BR exigem `lang="pt-BR"` no HTML renderizado. Verificar no template do gerador. |
| 1.4.3 Contraste mínimo | todas | A verificar | Docs herdam o tema do visualizador. Medir pares texto e fundo e exigir 4.5:1 antes da entrega. |

## Critérios de aceite da GUI

Verificados no protótipo em 05/10/2026 e revistos em 24/10/2026, com a janela
fixa de 1180×760. A revisão retirou o refluxo do escopo: a janela não
redimensiona, e a largura deixou de ser uma variável a exercitar.

| Critério WCAG | Regra no Opcoda | Status | Evidência |
| --- | --- | --- | --- |
| 2.1.1 Teclado | knobs, botões e display operam por teclado, sem armadilha de foco | **cumpre** | os knobs são `juce::Slider`, que já entregam foco e ajuste com setas. Transporte, pasta, navegação do hex e abas são `juce::TextButton` sem cromo — o componente continua, só o desenho achatou — e `1` a `4`, `A`, `L` e `Espaço` operam tudo sem rato. O `ByteDisplay` tem caret e o editor encaminha-lhe as setas, `PageUp`/`PageDown`, `Home`, `End` e `Enter`: o mapa completo está abaixo. O detalhe que fecha o critério: o clique num controle não lhe dá o foco, e as teclas de nota chegam ao teclado virtual mesmo com o foco noutro sítio |
| 2.4.7 Foco visível | indicador de foco em todo controle | **cumpre** | `drawRotarySlider`, `drawButtonText` e os combos desenham o anel em `focusRing` (`#ffaa24`, 8,29:1 sobre o fundo `#202326`) quando há foco de teclado, ou seja, na travessia por `Tab`. O clique já não pinta anel nenhum — é o que o utilizador pediu —, e o `ByteDisplay` trocou o contorno laranja que desenhava por um caret branco de 6 px, que diz onde o cursor está sem emoldurar a vista inteira |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível à API do SO | **cumpre** | nome acessível é a descrição completa em português ("Tamanho de grao, em milissegundos"), não o rótulo curto do mock ("SIZE"), que seria inútil por leitor de tela. A grelha tem `setName` e `setHelpText`: o nome diz o que é e a ajuda diz que os dois caminhos de teclado estão noutro sítio, porque `Component` não expõe `setTooltip` a não ser por cabeçalho transitivo e a dica de rato não serviria para nada numa grelha que se opera por rato e por teclado |
| 2.5.8 Tamanho do alvo | alvos de no mínimo 24×24 px | **cumpre** | a célula de byte tem agora ~30 px de largura — a grelha estica na janela fixa, e o endereço ocupa uma gutter própria — mas a exceção continua declarada para a coluna ASCII, cujas células têm ~14 px. O `ByteDisplay` é focalizável e tem caret de navegação: as setas, `Início`, `Fim`, `Enter` e `Esc` operam o display inteiro sem passar por nenhum outro controle. Os botões de vista medem 38×32 px e as teclas de escolha de 34 a 42 px, com as teclas `1` a `4` como caminho garantido. Ver o mapa de teclas abaixo |
| 1.4.3 Contraste mínimo | texto a 4,5:1 sobre a superfície | **cumpre** | `textDark #e4e7e8` sobre o fundo `#202326` dá 12,70:1; `textSub #92999e` dá 5,47:1. O rodapé do display usa `textOnDark` (o mesmo claro) sobre `#0e1013`, que dá 19,05:1 em branco e ~12:1 no tom de leitura |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem em texto com código tipado, nunca só por cor | **cumpre** | o LED de falha é vermelho e a linha de status escreve `RECUSADO / E_BAD_PE`. Cor e texto sempre juntos. A linha de estado fica no topo do display e não no rodapé: um `E_BAD_PE` em Inter de 10 px ao lado do pico é uma coisa que passa sem ser lida |
| 1.4.1 Uso de cor | LEDs, a região ativa e a cabeça de leitura nunca são o único sinal | **cumpre** | cada LED é acompanhado de rótulo. Na grelha, a região é um filete de 1 px no topo e na base da linha; o preenchimento laranja a 3 px fica reservado para o endereço tocado, que é o único que a barra marca — cor e forma juntas. A cabeça de leitura é uma barra branca de 2 px na margem esquerda da célula, com fundo claro por baixo do texto. Os grãos são riscos brancos baixos, nunca barras de altura total — a forma distingue-os da cabeça de leitura mesmo sendo a mesma cor. A cor da onda e das barras conta a entropia, mas posição e forma já a contam: quem não distingue a cor lê a mesma altura no mesmo sítio. O rodapé escreve `POS` e `REG` em hexadecimal |
| 1.4.11 Contraste não textual | gráficos e indicadores de foco a 3:1 | **cumpre** | anéis de foco em `focusRing`: 8,29:1 sobre o fundo. Grãos em branco: 19,05:1 sobre o fundo e acima de 3:1 sobre o núcleo em teal. Onda: `waveLow` 10,86:1, `accentSoft` 12,32:1, `waveHigh` 6,40:1, todos contra o display. Viewport no overview leva duas marcas de pé `accent` (10,0:1) e um filete de base, e a miniatura usa a mesma tinta da onda; o preenchimento e o filete da onda partilham a mesma cor, pelo que a forma não depende de duas tintas. A borda estrutural (`#44494d`, 1,73:1) não carrega estado sozinha: abas têm texto + filete, e por isso o 1.4.11 não se aplica a ela |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | **não aplicável** | desde 24/10/2026 a janela é fixa em 1180×760 e não redimensiona; o host não tem asa de redimensionamento e não há largura variável a exercitar. O que o critério pedia em largura variável, a grelha cobre em largura fixa: endereços de 8 dígitos, 16 colunas e coluna ASCII sempre presentes |

### Ressalva declarada ao 2.5.8 — **retirada em 06/10/2026**

A exceção existia porque a grelha de bytes **só** se operava com o rato, e a
razão estava escrita em `hex_grid.h`: as células são pintadas e não componentes,
e um `Component` só não tem filhos acessíveis. Não havia como dar nome a duzentas
células, e o caminho de teclado equivalente — o knob POSITION e o campo de
endereco, este removido na janela fixa — eram controles distintos com nome e
valor proprios, o que o criterio 2.1.1 aceita mas não é a mesma coisa que operar
a grelha.

**O `ByteDisplay` é focalizável e tem uma caret de navegação, e isso fecha o 2.5.8
pelo ramo certo.** Não se aumentou o alvo — isso continua impossível, e a
densidade continua a ser a função —; o que passou a existir é uma operação
completa por teclado sobre a grelha:

| Tecla | O que faz |
| --- | --- |
| `←` `→` | movem a caret um byte |
| `Shift` + `←` `→` | movem a caret dezasseis bytes, uma linha |
| `↑` `↓` | movem a caret uma linha |
| `PgUp` `PgDn` | movem a caret 1024 bytes, uma página |
| `Início` `Fim` | início da região e `fim − 2` |
| `Enter` | ativa o byte sob a caret, que é o que o clique faz |
| `Esc` | devolve a caret à cabeça de leitura |
| `Espaço` | liga e desliga a reprodução da região |
| `1` `2` `3` `4` | forma de onda, hex, entropia, matriz |
| `A` | alterna a região entre a exata e a secção PE mais próxima |
| `L` | abre o diálogo de arquivo |

Duas decisões fazem isto funcionar, e ambas são sobre o que o utilizador ouve:

- **A caret é separada da cabeça de leitura.** As setas movem a caret e não mudam
  o som; só o `Enter` escreve. Navegar fica inofensivo.
- **A caret segue a cabeça de leitura enquanto o utilizador não lhe tocar**, e
  deixa de seguir assim que ele mexe. Sem isso, os vinte tiques por segundo do
  editor repunham a caret no meio e as setas não fariam nada.

O `Enter` mudou de dono quando o campo de endereço saiu: com o display
focalizável, o `Enter` passou a ativar o byte sob a caret e o alinhamento ficou
com a tecla `A` — e depois com os quatro botões de página no pé do display. A
dica vive no help text do display, ou ficava a mentir.

O clique **não** dá o foco a quem foi clicado: dá-o ao teclado virtual. A regra
anterior, que dava o foco ao display, resolvia o `Espaço` e criava um problema
pior — trocar de vista calava as teclas de nota até alguém clicar no teclado, e
o anel laranja aparecia a cada clique como se fosse um aviso. Agora o foco
pertence ao teclado, os controles dispensam o foco no rato
(`setMouseClickGrabsKeyboardFocus(false)`) e o editor encaminha as setas para o
cursor do display, de sorte que a navegação continua a existir sem depender de
quem tem o foco.

### Ressalva ao 2.1.1 que permanece

**A divisão de teclas segue o foco, e a divisão é a que o JUCE já impõe.** A
tecla chega primeiro ao componente focado e só depois sobe para os ancestrais:
com um combo da vista MATRIX focado, as setas mudam a escolha do combo e não
movem a caret da grelha — cada componente opera por teclado e nenhum rouba
teclas ao outro. A navegação do cursor, essa, deixou de depender do foco: o
editor trata das setas, `PageUp`/`PageDown`, `Home`, `End` e `Enter` antes de
qualquer outro dono. A tecla que sobe, quando o foco não está no teclado, é
encaminhada por `keyStateChanged` para que nenhuma nota fique presa. Não é
ressalva ao critério, é consequência do desenho que convém escrever para não
parecer um defeito quando alguém ler o código.

### Duas decisões de desenho que afetam acessibilidade

O device inteiro é escuro (fundo `#202326`, display `#0e1013`), e é por
contraste, não só por estética. No tema claro anterior, halo laranja sobre
superfície clara dava 1,24:1 e desaparecia, e toda a energia de brilho ficava
na única região escura; com o fundo escuro, ambar e teal passam 3:1 em toda a
superfície (8,29:1 e 9,00:1) e o glow contido vale em todo o lado.

O rodapé de telemetria vive dentro do display. A regra geral mantém-se:
**quem decide a cor de fundo decide a cor do texto** — o rodapé usa os tokens
escuros (`textOnDark`), nunca os do chassi. Os combos e a barra da matriz
seguem a mesma regra sobre o fundo do display.

## Encaminhamento

Corrigir os links genéricos da proposta completa, fixar `lang="pt-BR"` no
template e medir o refluxo a 200% de zoom em display de baixa densidade. O 1.4.1
da região ativa fechou com o filete de 1 px em cima e em baixo da linha da
grelha; o 2.5.8 fechou em 06/10/2026 com a caret de teclado do `ByteDisplay`, e
a exceção declarada foi retirada em vez de adiada para a S5.
