# Opcoda - Otimização de Segmentos Binários

Protótipo acadêmico para a disciplina de Programação Linear / Meta-heurísticas.

## Contexto

O projeto Opcoda (Op-code + Coda) pesquisa a sonificação de arquivos executáveis (PE/.exe). Este subprojeto demonstra a aplicação de métodos de otimização combinatória para selecionar segmentos de dados binários que maximizam a diversidade estatística dos blocos utilizados na sonificação.

## Problema

O problema é modelado como um **Problema da Mochila 0/1**:

- Cada bloco do arquivo possui **peso** (tamanho em KB) e **valor** (diversidade medida por entropia de Shannon com penalização para bytes 0x00).
- Existe uma **capacidade máxima**.
- O objetivo é selecionar um subconjunto de blocos que maximize o valor total sem ultrapassar a capacidade.

## Algoritmos Implementados

1. **Subida de Encosta** - busca local determinística
2. **Subida de Encosta com Tentativas** - múltiplas reinicializações (TMAX)
3. **Têmpera Simulada** - aceitação probabilística de soluções piores

## Estrutura

```
src/
  OpcodaOtimizacao/          # Aplicação WPF principal
    Models/                  # Item, ProblemaMochila, Solucao
    Algorithms/              # Otimizadores e vizinhança
    Services/                # Geração de problema e análise comparativa
    Utilities/               # Entropia de Shannon
    ViewModels/              # MVVM
    Views/                   # XAML
  OpcodaOtimizacao.Tests/    # Testes xUnit
docs/                        # Documentação acadêmica
results/                     # Resultados exportados (CSV)
LeiaMe.txt                   # Instruções de execução
```

## Requisitos

- Windows 10/11
- .NET SDK 9.0 ou superior

## Compilação e Execução

```powershell
dotnet build src/OpcodaOtimizacao/OpcodaOtimizacao.csproj
dotnet run --project src/OpcodaOtimizacao/OpcodaOtimizacao.csproj
```

## Testes

```powershell
dotnet test src/OpcodaOtimizacao.Tests/OpcodaOtimizacao.Tests.csproj
```

## Configuração dos Discentes

Edite `src/OpcodaOtimizacao/DadosAcademicos.cs` preenchendo os placeholders `[PREENCHER]`.

## Documentação

Consulte a pasta `docs/`:
- `modelagem.md` - Modelagem como problema da mochila
- `algoritmos.md` - Descrição dos algoritmos
- `analise-comparativa.md` - Metodologia de comparação
- `REQUISITOS_PROFESSOR.md` - Checklist item a item do enunciado