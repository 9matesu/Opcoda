# 025 · Pesquisa e Otimização: PCV sobre blocos com três métodos de busca

**Feature Branch**: `feat/academic-optimization`
**Criada**: 2026-10-26
**Status**: Implemented
**Objetivos atendidos**: fora do mapa de OE — módulo académico da disciplina Programação
Linear / Otimização (FATEC Cruzeiro, ADS, 5º semestre). Os portões B (testes), E
(interface) e F (documentação) continuam a valer; o portão C é atendido por construção,
porque nenhuma linha do caminho de áudio muda.
**Ensaios**: portões A, B, E, F

## Contexto

A disciplina de Programação Linear / Otimização pede um trabalho com três entregas:
implementação, documentação e apresentação. O recorte escolhido é o Problema do
Caixeiro Viajante (PCV) sobre blocos de dados binários: os nós são blocos do material
carregado, o custo de transição mede a mudança estatística entre dois blocos, e a
sequência ótima reorganiza a ordem em que o material é sonificado.

Existe precedente no repositório: o trabalho anterior usou a mochila com têmpera
simulada (`apresentacao-mochila-tempera-simulada.pptx`, branch
`feature/busca-local-mochila-otimizacao`). Esta feature substitui o modelo: a mochila
seleciona um subconjunto de blocos; o PCV ordena todos os blocos. Ordenar é o que
interessa à sonificação — o motor lê o material em sequência, e a ordem de leitura é
uma permutação, não uma seleção.

Os três métodos exigidos são Subida de Encosta, Subida de Encosta com Tentativas
(TMAX) e Têmpera Simulada. Algoritmos genéticos ficam fora do escopo: a interface
exibe "Módulo em Desenvolvimento".

## Decisões de modelagem

1. **Nós.** Cada nó é um bloco contíguo de bytes. Na instância FIXA, os blocos saem
   da região corrente do material carregado, dividida em 16 blocos de tamanho igual.
   Na instância ALEATÓRIA, o usuário escolhe n (8 a 64) e o gerador cria uma matriz
   simétrica de custos inteiros a partir de uma semente registrada.
2. **Custo de transição.** Para blocos reais, d(i,j) é a distância de Manhattan entre
   os histogramas de 256 valores de byte, normalizados pelo tamanho do bloco,
   multiplicada por 100 para leitura numérica. Simétrica por construção; diagonal
   zero; blocos vazios têm histograma nulo. Para instâncias aleatórias, d(i,j) é
   sorteado em [1, 200], com d(i,i) = 0. A métrica mede mudança de distribuição de
   bytes, não qualidade musical — e a documentação repete essa limitação.
3. **Solução.** Um percurso é uma permutação dos nós, fechada em ciclo (a última
   transição volta ao primeiro). Toda solução visitada pelos algoritmos é válida por
   construção: as vizinhanças (troca e 2-opt) preservam a propriedade de permutação.
4. **Reprodutibilidade.** Todo algoritmo recebe uma semente explícita. As instâncias
   aleatórias saem de `std::mt19937_64` com a semente no registro. A instância fixa de
   referência é um binário PE versionado no repositório
   (`specs/025-pesquisa-e-otimizacao/fixtures/material-fixo.bin`), o que permite
   reproduzir o experimento da monografia em qualquer máquina.

## Cenários

### História 1 — Núcleo de otimização sem JUCE (P1)

Como estudante que precisa justificar cada número, quero os algoritmos no núcleo,
testáveis e determinísticos, para citar resultados com lastro.

**Por que P1**: sem núcleo correto não há trabalho académico; a interface é apresentação.

**Teste independente**: `Opt.*` cobre instâncias, custos, percursos, vizinhanças, os
três métodos, paradas, sementes e comparação com enumeração exata (n ≤ 10).

### História 2 — Painel Pesquisa e Otimização no editor (P1)

Como estudante em demonstração, quero um botão no cabeçalho que troca o corpo do
editor por um painel académico, para configurar o problema, executar cada método e
mostrar resultados sem sair do instrumento.

**Por que P1**: é a entrega visível da atividade.

**Teste independente**: build release + captura `tools/capture-ui.ps1`; operação por
teclado (Tab entre controles, Enter executa), nomes acessíveis em cada controle.

### História 3 — Ouvir a referência e o resultado (P2)

Como apresentador, quero alternar entre o material original e o material reordenado
pela solução, para demonstrar o efeito da otimização na sonificação — sem prometer
que custo menor soa "melhor".

**Por que P2**: depende do painel e do núcleo, mas não bloqueia as outras histórias.

**Teste independente**: com instância fixa, "Ouvir resultado" publica os blocos na
ordem do percurso pela mesma fila SPSC; "Ouvir referência" restaura a região original.
Fora da thread de áudio, sem segunda engine.

### História 4 — Análise comparativa com registro (P2)

Como estudante que escreve o relatório, quero executar os três métodos sobre a mesma
instância em repetições semeadas, ver tabela e gráfico, e exportar CSV para o
relatório.

**Por que P2**: gera os dados que o PDF cita; sem eles o relatório não existe.

**Teste independente**: a bateria roda na interface (5 repetições) e pelo executor de
console `opcoda_experiment` (10 repetições, protocolo da monografia); o CSV lista
instância, método, parâmetros, custos, iterações, avaliações, tempo, tentativas e
semente.

### História 5 — Algoritmos genéticos anunciados como pendência (P3)

Como professor, quero ver "Módulo em Desenvolvimento" ao selecionar Algoritmos
Genéticos, em vez de um botão que finge executar.

**Por que P3**: honestidade de escopo; custo trivial.

**Teste independente**: seleção do método genético exibe o texto fixo e desativa Executar.

## Fora de escopo

- Algoritmos genéticos (implementação).
- Formulação linear inteira com variáveis binárias e eliminação de subciclos — o
  trabalho usa a formulação de permutação, e a documentação declara que a versão ILP
  não foi implementada.
- Afirmações de qualidade sonora: custo PCV não mede agrado musical, e nenhum teste
  perceptivo foi feito.
- Alteração do fluxo SPSC, do motor granular ou do parser PE.
