# Algoritmos Implementados

Todos os algoritmos compartilham:
- representação: vetor binário de comprimento N;
- função objetivo: somatório dos valores dos itens selecionados;
- restrição: somatório dos pesos ≤ capacidade;
- reparação gulosa aplicada sempre que um vizinho viola a restrição.

## 1. Subida de Encosta (Hill Climbing)

### Ideia

Partir de uma solução inicial viável e iterativamente mover para o melhor vizinho estritamente superior. Parar quando nenhum vizinho melhorar a solução corrente.

### Passos

1. Gerar solução inicial aleatória e repará-la.
2. Avaliar todos os vizinhos gerados pelos três operadores.
3. Escolher o vizinho válido com maior valor.
4. Se houver melhoria, substituir a solução corrente e repetir o passo 2.
5. Caso contrário, retornar a solução corrente.

### Critério de parada

Ausência de melhoria após varrer toda a vizinhança.

### Observações

Determinística dado o ponto de partida. Pode ficar presa em ótimos locais, especialmente em instâncias maiores.

## 2. Subida de Encosta com Tentativas (Random Restart Hill Climbing)

### Ideia

Executar K buscas independentes de subida de encosta, cada uma partindo de uma solução inicial diferente. Retornar a melhor entre todas as execuções.

### Parâmetro TMAX

Número máximo de tentativas/reinicializações. Deve ser inteiro ≥ 1. Valores altos aumentam a chance de escapar de ótimos locais ruins, ao custo de tempo adicional.

### Passos

1. Para tentativa t = 1 .. TMAX:
    a. Executar Subida de Encosta a partir de nova semente aleatória.
    b. Registrar o resultado dessa tentativa.
2. Retornar a melhor solução global encontrada.

### Interpretação adotada

TMAX é entendido aqui como o número de reinicializações completas. Se o professor definir outro significado (por exemplo, limite de iterações por tentativa), basta ajustar o laço externo em `SubidaDeEncostaComTentativas.Executar`.

## 3. Têmpera Simulada (Simulated Annealing)

### Ideia

Permitir aceitação probabilística de soluções piores, controlada por uma temperatura que decresce ao longo das iterações. Isso permite escapar de ótimos locais nas fases iniciais e convergir para refinamento nas finais.

### Parâmetros

- Temperatura inicial T0: controle da fase exploratória. Padrão sugerido: 100.
- Temperatura final Tf: critério de parada. Padrão sugerido: 1.
- Fator redutor α ∈ (0,1): multiplicador aplicado à temperatura a cada iteração. Padrão sugerido: 0.95.

### Fórmula de aceitação

Se Δ = f(vizinho) − f(atual):

- Se Δ > 0 → aceita automaticamente.
- Se Δ ≤ 0 → aceita com probabilidade P = exp(−Δ / T).

Nota: como Δ é negativo nesse ramo, −Δ/T fica positivo e P ∈ (0,1]. Quanto menor T, menor a chance de aceitar piora.

### Passos

1. Gerar solução inicial viável.
2. Definir T ← T0.
3. Enquanto T ≥ Tf:
    a. Sortear um único vizinho usando os três operadores com igual probabilidade.
    b. Reparar se necessário.
    c. Aplicar critério de aceitação acima.
    d. Atualizar melhor solução global vista.
    e. Registrar valor atual na curva de evolução.
    f. T ← T · α.
4. Retornar a melhor solução global.

### Critério de parada

Temperatura abaixo de Tf. Alternativamente seria possível limitar o número de iterações; optou-se pelo corte térmico por estar diretamente ligado aos parâmetros pedidos no enunciado.

## Comparação metodológica

Os três métodos produzem métricas equivalentes: valor final, peso utilizado, capacidade, itens selecionados, tempo decorrido, iterações realizadas, lista de parâmetros e histórico da função objetivo. Essas métricas alimentam a Análise Comparativa exibida na interface e exportadas em CSV para posterior geração do PDF.