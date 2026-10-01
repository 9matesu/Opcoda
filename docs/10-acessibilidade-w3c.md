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

## Critérios de aceite para a GUI (a verificar no protótipo)

| Critério WCAG | Regra no Opcoda | Status |
| --- | --- | --- |
| 2.1.1 Teclado | todos os knobs, sliders e a navegação de waveform operam por teclado, sem armadilha de foco | a verificar |
| 2.4.7 Foco visível | indicador de foco com contraste 3:1 em todo controle | a verificar |
| 4.1.2 Nome, função e valor | cada controle expõe nome acessível (ex.: "tamanho de grão, 40 ms") à API de acessibilidade do SO | a verificar |
| 1.4.10 Refluxo | interface utilizável a 200% de zoom sem rolagem horizontal | a verificar |
| 3.3.1 Identificação de erro | binário rejeitado gera mensagem de erro em texto com código tipado, nunca só por cor | a verificar |
| 1.4.1 Uso de cor | curva de entropia e mapa de seções usam também rótulo ou padrão, não só cor | a verificar |

## Encaminhamento

Corrigir os links genéricos da proposta completa, fixar `lang="pt-BR"` no template e rodar o checklist da GUI no primeiro protótipo navegável, antes do congelamento de código (M11). Reauditoria marcada para o fim da etapa E4.
