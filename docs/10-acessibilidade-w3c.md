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
| 2.1.1 Teclado | knobs, campo de endereço, botão ALINHAR e botões operam por teclado, sem armadilha de foco | **cumpre** | os knobs são `juce::Slider`, que já entregam foco e ajuste com setas. `ByteAddressField` é um `TextEditor` com nome acessível: `Enter` confirma, `Esc` volta ao valor anterior, setas sobem e descem um byte e PageUp/PageDown saltam dezasseis. O `ALINHAR` é um `juce::TextButton`. Ver a ressalva do 2.5.8 para o que a grelha de bytes **não** faz |
| 2.4.7 Foco visível | indicador de foco em todo controle | **cumpre** | `drawRotarySlider` desenha anel laranja quando `hasKeyboardFocus(true)`, `drawButtonBackground` desenha contorno no botão, e o `ByteAddressField` desenha contorno laranja à volta do campo quando `editor_.hasKeyboardFocus(true)` |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível à API do SO | **cumpre** | nome acessível é a descrição completa em português ("Tamanho de grao, em milissegundos"), não o rótulo curto do mock ("SIZE"), que seria inútil por leitor de tela. O campo de endereço escreve o valor em hexadecimal, que é a unidade em que se lê. A grelha tem `setName` e `setHelpText`: o nome diz o que é e a ajuda diz que os dois caminhos de teclado estão noutro sítio, porque `Component` não expõe `setTooltip` a não ser por cabeçalho transitivo e a dica de rato não serviria para nada numa grelha que só se opera com o rato |
| 2.5.8 Tamanho do alvo | alvos de no mínimo 24×24 px | **não cumpre, exceção declarada** | ver a ressalva abaixo |
| 1.4.3 Contraste mínimo | texto a 4,5:1 sobre a superfície | **cumpre** | `textDark #1b1e22` sobre `chassis #c2c6c9` dá 9,1:1; `textSub #383d42` dá 5,1:1. O rodapé do display usa `textOnDark #c6cacc` sobre `#0e1013`, que dá 11,6:1 |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem em texto com código tipado, nunca só por cor | **cumpre** | o LED de falha é vermelho e a linha de status escreve `RECUSADO / E_BAD_PE`. Cor e texto sempre juntos. A linha de estado fica no topo do display e não no rodapé: um `E_BAD_PE` em mono de 10 px ao lado do pico e das vozes é uma coisa que passa sem ser lida |
| 1.4.1 Uso de cor | LEDs, a região ativa e a cabeça de leitura nunca são o único sinal | **cumpre** | cada LED é acompanhado de rótulo. Na grelha, a região ativa leva fundo laranja a 22 % **e** um filete de 1 px em cima e em baixo da linha: o fundo diz a quem vê cor, o filete diz a quem não vê. A cabeça de leitura é uma barra branca de 2 px na margem esquerda da célula, com fundo claro por baixo do texto. O rodapé escreve `POS` e `REG` em hexadecimal |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | **parcial** | redimensiona de 560 a 4096 px sem rolagem, verificado por captura nos dois extremos. Abaixo de 520 px de largura a grelha **omite a coluna ASCII** em vez de cortar colunas de hexadecimal, que partiriam os endereços ao meio. Não há medição a 200% de zoom em display de baixa densidade |

### Ressalva declarada ao 2.5.8

**A grelha de bytes não cumpre o 2.5.8, e não há como fazê-la cumprir.** A célula
é um byte, que é a unidade que a grelha existe para mostrar. Tem 24 px de altura
e cerca de 17 de largura, e a cláusula alternativa do critério também não salva:
um círculo de 24 px centrado em duas células vizinhas da mesma linha
sobrepõe-se, porque 24 > 17.

A densidade **é** a função. Um alvo de 24 px por byte numa grelha de dezasseis
colunas daria 384 px de largura só para os alvos, e o ficheiro de 12 MB deixaria
de caber em qualquer janela razoável.

O que se fez em vez de aumentar o alvo:

- **As linhas têm 24 px**, e não 12. O critério reprova pela largura, mas dar a
  altura ao alvo não custa nada e ajuda o clique a esmo, que é o gesto mais
  comum.
- **O caminho de teclado existe e é completo.** Mover a cabeça de leitura é o
  knob POSITION, com foco e setas. Mover o início da região é o campo de
  endereço, com `Enter`, setas e PageUp/PageDown. São dois controles com nome e
  valor próprios, não a mesma coisa desenhada outra vez.
- **O duplo clique e o `ALINHAR`** dão teclado ao alinhamento de seção, que antes
  estava na tecla `Enter` do seletor removido.

Isto entra pelo mesmo caminho da lacuna do TSan em `docs/07-plano-testes.md`: uma
limitação que a plataforma impõe, escrita e repetida nos artefatos entregues, e
não omitida. Se a reauditoria do congelamento de código (etapa S5) exigir 2.5.8
pleno, a saída é um modo de seleção por teclado na grelha com cursor de célula,
que é trabalho novo e não uma correção.

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
grelha; o 2.5.8 está com exceção declarada e à espera de decisão na S5.
