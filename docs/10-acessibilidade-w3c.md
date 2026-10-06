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

Verificados no protótipo em 05/10/2026, com o Standalone aberto em 900×540 e no
mínimo de 480×420. As três larguras foram fotografadas com a janela do plugin
renderizada diretamente, porque a 480 é a que exercita a refluxo da grelha.

| Critério WCAG | Regra no Opcoda | Status | Evidência |
| --- | --- | --- | --- |
| 2.1.1 Teclado | knobs, campo de endereço, botões e display operam por teclado, sem armadilha de foco | **cumpre** | os knobs são `juce::Slider`, que já entregam foco e ajuste com setas. `ByteAddressField` é um `TextEditor` com nome acessível: `Enter` confirma, `Esc` volta ao valor anterior, setas sobem e descem um byte e PageUp/PageDown saltam dezasseis. O `ALINHAR` e o `PLAY` são `juce::TextButton`. O `ByteDisplay` é focalizável e tem caret: o mapa completo está abaixo |
| 2.4.7 Foco visível | indicador de foco em todo controle | **cumpre** | `drawRotarySlider` desenha anel laranja quando `hasKeyboardFocus(true)`, `drawButtonBackground` desenha contorno no botão, e o `ByteAddressField` desenha contorno laranja à volta do campo quando `editor_.hasKeyboardFocus(true)` |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível à API do SO | **cumpre** | nome acessível é a descrição completa em português ("Tamanho de grao, em milissegundos"), não o rótulo curto do mock ("SIZE"), que seria inútil por leitor de tela. O campo de endereço escreve o valor em hexadecimal, que é a unidade em que se lê. A grelha tem `setName` e `setHelpText`: o nome diz o que é e a ajuda diz que os dois caminhos de teclado estão noutro sítio, porque `Component` não expõe `setTooltip` a não ser por cabeçalho transitivo e a dica de rato não serviria para nada numa grelha que só se opera com o rato |
| 2.5.8 Tamanho do alvo | alvos de no mínimo 24×24 px | **cumpre** | a célula de byte continua a 17 px de largura, que não tem correção possível, mas a grelha deixou de ser de rato. O `ByteDisplay` é focalizável e tem caret de navegação: as setas, `Início`, `Fim`, `Enter` e `Esc` operam o display inteiro sem passar por nenhum outro controle. Ver o mapa de teclas abaixo |
| 1.4.3 Contraste mínimo | texto a 4,5:1 sobre a superfície | **cumpre** | `textDark #1b1e22` sobre `chassis #c2c6c9` dá 9,1:1; `textSub #383d42` dá 5,1:1. O rodapé do display usa `textOnDark #c6cacc` sobre `#0e1013`, que dá 11,6:1 |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem em texto com código tipado, nunca só por cor | **cumpre** | o LED de falha é vermelho e a linha de status escreve `RECUSADO / E_BAD_PE`. Cor e texto sempre juntos. A linha de estado fica no topo do display e não no rodapé: um `E_BAD_PE` em mono de 10 px ao lado do pico e das vozes é uma coisa que passa sem ser lida |
| 1.4.1 Uso de cor | LEDs, a região ativa e a cabeça de leitura nunca são o único sinal | **cumpre** | cada LED é acompanhado de rótulo. Na grelha, a região ativa leva fundo laranja a 22 % **e** um filete de 1 px em cima e em baixo da linha: o fundo diz a quem vê cor, o filete diz a quem não vê. A cabeça de leitura é uma barra branca de 2 px na margem esquerda da célula, com fundo claro por baixo do texto. O rodapé escreve `POS` e `REG` em hexadecimal |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | **parcial** | redimensiona de 560 a 4096 px sem rolagem, verificado por captura nos dois extremos. Abaixo de 520 px de largura a grelha **omite a coluna ASCII** em vez de cortar colunas de hexadecimal, que partiriam os endereços ao meio. Não há medição a 200% de zoom em display de baixa densidade |

### Ressalva declarada ao 2.5.8 — **retirada em 06/10/2026**

A exceção existia porque a grelha de bytes **só** se operava com o rato, e a
razão estava escrita em `hex_grid.h`: as células são pintadas e não componentes,
e um `Component` só não tem filhos acessíveis. Não havia como dar nome a duzentas
células, e o caminho de teclado equivalente — o knob POSITION e o campo de
endereco - eram dois controles distintos com nome e valor proprios, o que o criterio
2.1.1 aceita mas não é a mesma coisa que operar a grelha.

**O `ByteDisplay` é focalizável e tem uma caret de navegação, e isso fecha o 2.5.8
pelo ramo certo.** Não se aumentou o alvo — isso continua impossível, e a
densidade continua a ser a função —; o que passou a existir é uma operação
completa por teclado sobre a grelha:

| Tecla | O que faz |
| --- | --- |
| `←` `→` | movem a caret um byte |
| `Shift` + `←` `→` | movem a caret dezasseis bytes, uma linha |
| `↑` `↓` | movem a caret uma linha |
| `PgUp` `PgDn` | movem a caret 1024 bytes, que é a janela que o rodapé mede |
| `Início` `Fim` | início da região e `fim − 2` |
| `Enter` | ativa o byte sob a caret, que é o que o clique faz |
| `Esc` | devolve a caret à cabeça de leitura |
| `Espaço` | liga e desliga a reprodução da região |
| `1` `2` `3` | forma de onda, hex, entropia |
| `A` | alterna a região entre a exata e a secção PE mais próxima |
| `L` | abre o diálogo de arquivo |

Duas decisões fazem isto funcionar, e ambas são sobre o que o utilizador ouve:

- **A caret é separada da cabeça de leitura.** As setas movem a caret e não mudam
  o som; só o `Enter` escreve. Navegar fica inofensivo.
- **A caret segue a cabeça de leitura enquanto o utilizador não lhe tocar**, e
  deixa de seguir assim que ele mexe. Sem isso, os vinte tiques por segundo do
  editor repunham a caret no meio e as setas não fariam nada.

O `Enter` mudou de dono: o `ALINHAR` foi criado para substituir o `Enter` que o
seletor removido usava, e com a grelha focalizável o `Enter` passou a ativar o
byte sob a caret. O alinhamento ficou com a tecla `A`. A dica do `ALINHAR` mudou
no mesmo commit, ou ficava a mentir.

O clique dá o foco do teclado ao display, pelo mesmo cuidado que o campo de
endereço tem em `grabFocusOnField`: um utilizador que clica no display e depois
carrega em `Espaço` veria o transporte não responder, e a única pista seria o anel
de foco que só aparece depois de um `Tab`.

### Ressalva ao 2.1.1 que permanece

**As setas nunca chegam ao display com o campo de endereço focado, e isso é
deliberado.** `AddressEditor::keyPressed` retira as quatro teclas de navegação ao
`juce::TextEditor` de propósito, porque um campo que responde a setas de forma
diferente conforme o cursor está ou não no fim do texto é um defeito que só
aparece quando o campo já tem conteúdo. O display só vê as setas quando tem o
foco, e a divisão é a que o JUCE já impõe: a tecla chega primeiro ao componente
focado e só depois sobe para os ancestrais.

Isto não é uma ressalva ao critério — cada um dos dois componentes opera por
teclado, e nenhum rouba teclas ao outro — mas é uma consequência do desenho que
convém escrever para não parecer um defeito quando alguém ler o código.

### Duas decisões de desenho que afetam acessibilidade

O display é escuro porque o chassi é claro, e não por estética. Halo laranja
sobre superfície clara dá 2,4:1 e desaparece, então toda a energia de brilho
ficou na única região com contraste suficiente.

O rodapé de telemetria vive dentro do display, e isso obriga a uma segunda
par de tokens: um rótulo sem cor de texto própria herda a quase preta do
`Label` e fica ilegível sobre o LCD escuro. `textOnDark #c6cacc` sobre
`display #0e1013` dá 11,6:1. Regra geral: **quem decide a cor de fundo decide
a cor do texto**, e por isso o rodapé não pode usar os tokens do chassi claro.

O campo de endereço e o botão `ALINHAR` ficam na linha de estado, e não no
rodapé. São controles, e o rodapé é um alinhamento de leituras: um campo de
texto dentro do rodapé seria indistinguível de uma leitura.

## Encaminhamento

Corrigir os links genéricos da proposta completa, fixar `lang="pt-BR"` no
template e medir o refluxo a 200% de zoom em display de baixa densidade. O 1.4.1
da região ativa fechou com o filete de 1 px em cima e em baixo da linha da
grelha; o 2.5.8 fechou em 06/10/2026 com a caret de teclado do `ByteDisplay`, e
a exceção declarada foi retirada em vez de adiada para a S5.
