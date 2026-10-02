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

Verificados no protótipo em 02/10/2026, com o Standalone aberto em 822×498 e no
mínimo de 562×408.

| Critério WCAG | Regra no Opcoda | Status | Evidência |
| --- | --- | --- | --- |
| 2.1.1 Teclado | knobs e botões operam por teclado, sem armadilha de foco | **cumpre** | os knobs são `juce::Slider`, que já entrega foco e ajuste por setas. Sem componente customizado que capture teclado |
| 2.4.7 Foco visível | indicador de foco em todo controle | **cumpre** | `OpcodaLookAndFeel::drawRotarySlider` desenha anel laranja de 2 px quando `slider.hasKeyboardFocus(true)` |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível à API do SO | **cumpre** | nome acessível é a descrição completa em português ("Tamanho de grao, em milissegundos"), não o rótulo curto do mock ("SIZE"), que seria inútil por leitor de tela |
| 2.5.8 Tamanho do alvo | alvos de no mínimo 24×24 px | **cumpre** | o menor alvo é o botão `LOAD` com 50×24; os knobs têm 62 px de diâmetro |
| 1.4.3 Contraste mínimo | texto a 4,5:1 sobre a superfície | **cumpre** | `textDark #1b1e22` sobre `chassis #c2c6c9` dá 9,1:1; `textSub #383d42` dá 5,1:1. O laranja `#ff9a00` dá 2,4:1 e por isso é só preenchimento e indicador, nunca portador de texto pequeno |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem em texto com código tipado, nunca só por cor | **cumpre** | o LED de falha é vermelho e a linha de status escreve `RECUSADO / E_BAD_PE`. Cor e texto sempre juntos |
| 1.4.1 Uso de cor | LEDs nunca são o único sinal | **cumpre** | cada LED é acompanhado de rótulo; nenhum estado depende só da cor |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | **parcial** | redimensiona de 560 a 4096 px sem rolagem, mas não há medição a 200% de zoom em display de baixa densidade |
| 1.4.1 Uso de cor (curva e mapa de seções) | curva de entropia e mapa de seções usam rótulo ou padrão, não só cor | **pendente** | a curva de entropia e as abas de seção entram na etapa F008; ainda não existem |

### Uma decisão de desenho que afeta acessibilidade

O display é escuro porque o chassi é claro, e não por estética. Halo laranja
sobre superfície clara dá 2,4:1 e desaparece, então toda a energia de brilho
ficou na única região com contraste suficiente. Se a curva de entropia for
vermelha sobre o LCD escuro, o contraste do texto adjacente não muda: é a
mesma decisão, aplicada antes de a curva existir.

## Encaminhamento

Corrigir os links genéricos da proposta completa, fixar `lang="pt-BR"` no
template e medir o refluxo a 200% de zoom em display de baixa densidade. Os itens
pendentes acima pertencem à etapa F008 e à reauditoria no congelamento de código
do MVP (etapa S5).
