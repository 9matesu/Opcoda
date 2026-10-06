# Modelagem como Problema da Mochila 0/1

## Contextualização

O projeto Opcoda (Op-code + Coda) pesquisa a sonificação de arquivos executáveis no formato PE (.exe). Um executável contém regiões com diferentes características estatísticas: algumas possuem grande quantidade de bytes repetitivos (inclusive sequências de 0x00), enquanto outras apresentam maior diversidade de valores.

Para fins de sonificação, deseja-se selecionar os segmentos mais informativos do arquivo, evitando aqueles dominados por padding ou zeros. Essa seleção pode ser formalizada como um problema combinatório clássico: o Problema da Mochila 0/1.

## Formulação Matemática

Seja um arquivo binário dividido em N blocos consecutivos de tamanho fixo B bytes. Cada bloco i possui:

- peso w_i: proporcional ao tamanho do bloco (em KB);
- valor v_i: medida de diversidade estatística do bloco;
- decisão x_i ∈ {0, 1}: se o bloco é selecionado ou não.

Dada uma capacidade máxima C, o problema consiste em:

    maximizar   Σ (v_i · x_i)
    sujeito a  Σ (w_i · x_i) ≤ C
               x_i ∈ {0, 1}   para todo i

## Função Valor

O valor de cada bloco é calculado a partir da entropia de Shannon normalizada pelos bits por byte, com penalização linear pela proporção de bytes iguais a 0x00:

    H(bloco) = -Σ p(b) · log2(p(b))     para b ∈ [0..255]
    Z(bloco) = (#bytes == 0x00) / tamanho_bloco
    v_i      = H(bloco_i) · (1 - Z(bloco_i))

A entropia mede a imprevisibilidade dos dados. Blocos muito homogêneos (por exemplo, grandes extensões de zeros usadas como padding em seções .rdata ou alinhamento) recebem valor próximo de zero mesmo que tenham tamanho relevante. Isso faz com que o otimizador naturalmente descarte essas regiões.

## Representação da Solução

Uma solução é representada por um vetor binário de comprimento N:

    S = [x_0, x_1, ..., x_{N-1}]

onde x_i = 1 indica que o bloco i foi escolhido para compor a amostra final usada na sonificação.

## Restrição e Reparação

Soluções podem violar a restrição de capacidade durante a busca. Adota-se uma estratégia de reparação gulosa: quando o peso total excede C, os itens selecionados são ordenados decrescentemente pela razão v_i/w_i e removidos até satisfazer a restrição. Essa abordagem preserva os itens mais "rentáveis" e garante que toda solução avaliada seja viável.

## Vizinhança

Três operadores são usados para gerar vizinhos de uma solução S:

1. Inclusão: escolhe um índice i tal que x_i = 0 e passa a x_i = 1; aplica reparação.
2. Exclusão: escolhe um índice i tal que x_i = 1 e passa a x_i = 0.
3. Troca: escolhe i selecionado e j não selecionado; troca os dois; aplica reparação.

Esses operadores cobrem movimentos locais simples e mantêm a conectividade do espaço de busca.

## Complexidade

O Problema da Mochila 0/1 é NP-difícil no número de itens. Para instâncias pequenas (até centenas de blocos), métodos de busca local e têmpera simulada encontram boas soluções em tempo compatível com uso interativo, que é o objetivo deste protótipo.