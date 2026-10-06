# Relatório de Métodos de Otimização — Projeto Opcoda

## Contexto do Projeto

O **Opcoda** (Op-code + Coda) pesquisa a **sonificação de arquivos executáveis (.exe)**. A ideia central é: um arquivo binário é dividido em blocos, e cada bloco vira um "item" da mochila. O objetivo é escolher quais blocos entram na sonificação para **maximizar a diversidade estatística** dos dados audíveis, respeitando uma capacidade total.

### Modelagem como Problema da Mochila 0/1

| Conceito | Significado no projeto | Código |
|---|---|---|
| Item (`Item`) | Um bloco do arquivo binário | `Models/Item.cs` |
| Peso | Tamanho do bloco (KB) → custo de processamento | `Item.Peso` |
| Valor | Diversidade medida por entropia de Shannon | `Item.Valor` |
| Capacidade | Limite total de dados que cabem na sonificação | `ProblemaMochila.Capacidade` |
| Solução (`Solucao`) | Vetor booleano indicando quais blocos foram escolhidos | `Models/Solucao.cs` |

A fórmula do valor é calculada em `Services/GeradorProblema.cs:112-116`:

```csharp
valor = entropia × (1 − proporçãoDeZeroes)
```

Ou seja: blocos com alta entropia (mais informação/variação) e poucos bytes zerados recebem valor maior. Blocos cheios de zeros são penalizados porque produzem silêncio na sonificação.

---

## Interface Comum

Todos os três métodos implementam `IOptimizador` (`Algorithms/IOptimizador.cs:21-25`):

```csharp
public interface IOptimizador
{
    string Nome { get; }
    ResultadoExecucao Executar(ProblemaMochila problema);
}
```

Cada método retorna um `ResultadoExecucao` (`Algorithms/IOptimizador.cs:6-19`) contendo: valor final, peso total, itens selecionados, número de iterações, tempo decorrido, se a solução é válida, parâmetros usados e histórico de evolução.

---

## Estrutura Compartilhada: Vizinhança

Antes de detalhar cada algoritmo, é essencial entender a classe `VizinhancaMochila` (`Algorithms/Otimizadores.cs:6-107`), usada pelos três métodos. Ela define como explorar soluções vizinhas e garantir viabilidade.

### Operadores de Movimento

Um movimento altera o vetor booleano da solução atual. Existem três tipos básicos:

1. **Adicionar item** (`Otimizadores.cs:10-18`): pega um índice não selecionado, marca como verdadeiro, e aplica reparação.
2. **Remover item** (`Otimizadores.cs:20-28`): pega um índice já selecionado, marca como falso (não precisa reparar).
3. **Trocar items** (`Otimizadores.cs:30-41`): remove um selecionado e adiciona um não-selecionado, depois repara.

Esses operadores aparecem tanto na versão enumerativa (`GerarTodos`, linhas 8-42) quanto na aleatória (`VizinhoAleatorio`, linhas 44-71).

### Reparação Greedy (`Reparar`, `Otimizadores.cs:73-97`)

Quando um movimento torna a solução inviável (peso > capacidade), esta função corrige removendo itens pela menor razão valor/peso até caber novamente. Isso garante que toda geração de vizinhos retorne soluções factíveis ou quase-factíveis.

### Geração Inicial (`SolucaoInicial`, `Otimizadores.cs:99-106`)

Sorteia cada bit com probabilidade 0.5 e chama `Reparar`. Assim começa qualquer busca local com uma solução plausível mas diversificada.

---

## Método 1 — Subida de Encosta (Hill Climbing)

Classe: `SubidaDeEncosta` (`Algorithms/Otimizadores.cs:109-169`)

### Como funciona teoricamente

É o mais simples dos três. Começa numa solução inicial e repete: avalia TODOS os vizinhos possíveis via `GerarTodos`, escolhe o melhor entre eles que tenha valor superior ao atual, move para ele, e continua enquanto houver melhoria. Quando nenhum vizinho melhora, para num ótimo local.

### Fluxo no código

| Etapa | Onde | Detalhe |
|---|---|---|
| Solução inicial | `Otimizadores.cs:123` | `VizinhancaMochila.SolucaoInicial(...)` |
| Loop externo ("melhorou") | `Otimizadores.cs:129-151` | Enquanto algum vizinho melhorar |
| Enumerar vizinhos | `Otimizadores.cs:134` | `foreach var vizBool in GerarTodos(atual, problema)` |
| Avaliar vizinho | `Otimizadores.cs:136-137` | `new Solucao(vizBool)` + `.Avaliar(problema)` |
| Filtrar inválidos | `Otimizadores.cs:140` | `if (!viz.EValido(problema)) continue` |
| Escolher melhor | `Otimizadores.cs:141-142` | Maior valor acima do atual |
| Aceitar movimento | `Otimizadores.cs:145-150` | Atualiza `atual`, registra evolução |
| Retornar resultado | `Otimizadores.cs:154-168` | Monta `ResultadoExecucao` |

### Pontos fortes e fracos no contexto Opcoda

✅ Rápido e determinístico dada a mesma seed  
❌ Fica preso facilmente em ótimos locais — pode deixar de fora blocos interessantes cuja combinação só faria sentido após passar por um pior intermediário  

---

## Método 2 — Subida de Encosta com Tentativas (Multi-Restart Hill Climbing)

Classe: `SubidaDeEncostaComTentativas` (`Algorithms/Otimizadores.cs:171-241`)

### Como funciona teoricamente

Mesmo princípio do anterior, mas executa TMAX reinícios independentes. Em cada tentativa gera uma nova solução inicial aleatória e roda subida pura internamente. No fim, devolve a MELHOR entre todas as tentativas. É uma forma barata de escapar parcialmente de ótimos locais através de múltiplas partidas diferentes.

Parâmetro chave: `_tMax` (definido pelo usuário na UI como campo "TMAX").

### Diferenças estruturais vs. Subida Pura

| Aspecto | Subida Pura | Com Tentativas |
|---|---|---|
| Reinício único/múltiplo | Único | TMAX vezes (`Otimizadores.cs:191`) |
| Nova sol. inicial por ciclo | Não | Sim (`Otimizadores.cs:193`) |
| Comparação global | N/A | Guarda melhor geral (`Otimizadores.cs:221-222`) |
| Histórico (`Evolucao`) | Valores intermediários | Um ponto por tentativa (`Otimizadores.cs:220`) |

O núcleo interno (linhas 197-218) é idêntico à subida pura; apenas embrulhado num loop extra.

### Trade-off no projeto

✅ Reduz significativamente risco de ótimo local ruim sem complexidade adicional  
❌ Custo linear em TMAX — muitas tentativas tornam execução lenta para problemas grandes  

---

## Método 3 — Têmpera Simulada (Simulated Annealing)

Classe: `TemperaSimulada` (`Algorithms/Otimizadores.cs:243-320`)

### Como funciona teoricamente

Inspirado no recozimento de metais. Mantém temperatura alta no início (aceita muitos movimentos piores probabilisticamente) e reduz gradualmente (concentra-se em refinamento). Permite atravessar regiões menos promissoras para alcançar melhores picos distantes.

Três parâmetros controlam o processo:
- `TempInicial`: quão agressivo aceita degradações no começo
- `TempFinal`: quando parar (temperatura mínima)
- `FatorRedutor`: taxa geométrica de resfriamento (ex.: 0.95 ⇒ t ← t×0.95 a cada passo)

### Critério de aceitação (`Otimizadores.cs:281-291`)

Para cada vizinho gerado aleatoriamente (`VizinhoAleatorio`):

```csharp
double delta = viz.ValorTotal - atual.ValorTotal;
if (delta > 0)          // melhoria → aceita sempre
    atual = viz;
else                    // piora → aceita com probabilidade exp(-Δ/T)
{
    double prob = Math.Exp(-delta / t);
    if (_rng.NextDouble() < prob)
        atual = viz;
}
```

Quanto maior a temperatura, maior a chance de aceitar pioras. Conforme esfria, praticamente só aceita melhorias — convergindo para comportamento guloso.

### Registro da melhor solução histórica (`Otimizadores.cs:293-297`)

Diferente da subida pura (que retorna onde parou), aqui guardamos separadamente a melhor vista durante todo o percurso:

```csharp
if (atual.ValorTotal > melhor.ValorTotal)
{
    melhor = atual.Clone();
    melhor.Avaliar(problema);
}
```

Isso evita perder boas configurações visitadas temporariamente antes de subir/descer montanhas.

### Resfriamento e término (`Otimizadores.cs:300-301`)

Ao final de cada iteração incrementa contador de evolução e multiplica temperatura pelo fator redutor. O laço principal termina quando `t < _tempFinal`.

### Papel no projeto Opcoda

✅ Escapada eficaz de ótimos locais graças à aceitação estocástica de pioras  
✅ Adequado para espaços discretos binários como seleção de blocos  
⚠️ Sensível aos hiperparâmetros: ajuste fino necessário para bons resultados consistentes  

---

## Integração com a Aplicação WPF

Os três algoritmos são instanciados dinamicamente conforme escolha do usuário em `ViewModels/MetodosViewModel.cs:75-84`:

```csharp
IOptimizador opt = Metodo switch
{
    MetodoSelecionado.SubidaDeEncosta => new SubidaDeEncosta(),
    MetodoSelecionado.SubidaDeEncostaComTentativas => new SubidaDeEncostaComTentativas(LerInt(TMaxTexto,...)),
    MetodoSelecionado.TemperaSimulada => new TemperaSimulada(...),
};
var res = opt.Executar(problema);
Saida = FormatarResultado(res);
```

### Análise Comparativa Automática

Em vez de rodar manualmente um método por vez, existe também `AnaliseComparativaService` (`Services/AnaliseComparativaService.cs`). Ele dispara automaticamente os três otimistas sobre o MESMO problema usando seeds derivadas deterministicamente (`Random(42)` seguido de derivações individuais), permitindo comparação justa lado-a-lado. Os resultados são exportados em CSV (`MetodosViewModel.cs:126-153`).

---

## Resumo Visual das Diferenças Chave

| Característica | Subida Pura | Multi-Tentativas | Têmpera Simulada |
|---|---|---|---|
| Exploração vizinhança | Completa (`GerarTodos`) | Completa | Aleatória única (`VizinhoAleatorio`) |
| Aceita pioras? | Nunca | Nunca | Probabilisticamente |
| Ótimo local provável | Alto risco | Médio risco | Baixo risco |
| Parâmetros extras | Nenhum | TMAX | Ti, Tf, α |
| Complexidade temporal | O(iterações × \|N\|²) | O(TMAX × ...) | O(número passos refrigeração) |
| Uso típico recomendado | Testes rápidos/baseline | Melhor qualidade previsível | Problemas difíceis/grandes |

---

*Documento gerado automaticamente a partir da análise estática do código-fonte.*