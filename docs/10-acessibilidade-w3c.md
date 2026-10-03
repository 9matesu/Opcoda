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

Verificados no protótipo em 03/10/2026, com o Standalone aberto em 822×498 e no
mínimo de 562×408.

| Critério WCAG | Regra no Opcoda | Status | Evidência |
| --- | --- | --- | --- |
| 2.1.1 Teclado | knobs, seletor de bytes e botões operam por teclado, sem armadilha de foco | **cumpre** | os knobs e o seletor são `juce::Slider`, que já entregam foco e ajuste com setas. O seletor aceita `Home` e `End` para os extremos do ficheiro, e `Enter` para alternar entre a região exata e a seção mais próxima |
| 2.4.7 Foco visível | indicador de foco em todo controle | **cumpre** | `drawRotarySlider` desenha anel laranja quando `hasKeyboardFocus(true)`, `drawButtonBackground` desenha contorno no botão, e `drawLinearSlider` desenha contorno na pega do seletor |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível à API do SO | **cumpre** | nome acessível é a descrição completa em português ("Tamanho de grao, em milissegundos"), não o rótulo curto do mock ("SIZE"), que seria inútil por leitor de tela. O seletor expõe `Value` em bytes, lido como `1024` e não como fração |
| 2.5.8 Tamanho do alvo | alvos de no mínimo 24×24 px | **cumpre** | o menor alvo é o botão `LOAD` com 76×28; os knobs têm 62 px de diâmetro; a pega do seletor tem 18 px de altura numa faixa de 30 px |
| 1.4.3 Contraste mínimo | texto a 4,5:1 sobre a superfície | **cumpre** | `textDark #1b1e22` sobre `chassis #c2c6c9` dá 9,1:1; `textSub #383d42` dá 5,1:1. O rodapé do display usa `textOnDark #c6cacc` sobre `#0e1013`, que dá 11,6:1 |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem em texto com código tipado, nunca só por cor | **cumpre** | o LED de falha é vermelho e a linha de status escreve `RECUSADO / E_BAD_PE`. Cor e texto sempre juntos |
| 1.4.1 Uso de cor | LEDs e o estado da região nunca são o único sinal | **cumpre** | cada LED é acompanhado de rótulo. A região ativa nas abas de seção leva uma faixa preta de 2 px além do fundo laranja, e o rodapé escreve o offset em hexadecimal |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | **parcial** | redimensiona de 560 a 4096 px sem rolagem, verificado por captura nos dois extremos, mas não há medição a 200% de zoom em display de baixa densidade |

### Duas decisões de desenho que afetam acessibilidade

O display é escuro porque o chassi é claro, e não por estética. Halo laranja
sobre superfície clara dá 2,4:1 e desaparece, então toda a energia de brilho
ficou na única região com contraste suficiente.

O rodapé de telemetria vive dentro do display, e isso obriga a uma segunda
par de tokens: um rótulo sem cor de texto própria herda a quase preta do
`Label` e fica ilegível sobre o LCD escuro. `textOnDark #c6cacc` sobre
`display #0e1013` dá 11,6:1. Regra geral: **quem decide a cor de fundo decide
a cor do texto**, e por isso o rodapé não pode usar os tokens do chassi claro.

A marca da região ativa nas abas é uma faixa preta de 2 px, e não só o fundo
laranja. Suspen 4 px acima da base da aba: encostada ao fundo escuro a faixa
desaparecia, e dentro do laranja mantém-se legível nos dois casos.

## Encaminhamento

Corrigir os links genéricos da proposta completa, fixar `lang="pt-BR"` no
template e medir o refluxo a 200% de zoom em display de baixa densidade. O 1.4.1
das abas e da curva fechou com a faixa preta na região ativa; o que resta de
pendente entra na reauditoria no congelamento de código do MVP (etapa S5).
