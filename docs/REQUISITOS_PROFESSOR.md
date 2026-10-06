# Checklist de Requisitos do Professor

Cada item abaixo corresponde literalmente ao enunciado. Ao lado, o status e a localização no código.

## 1. Interface "Principal"

- [x] Botão "Sobre" abre a interface gráfica "Sobre o Sistema" — `src/OpcodaOtimizacao/ViewModels/MainViewModel.cs:25`, `Views/SobreWindow.xaml`
- [x] Botão "Configurações" abre "Configuração do Problema" — `MainViewModel.cs:26`, `Views/ConfiguracaoWindow.xaml`
- [x] Botão "Métodos Básicos" abre "Métodos Básicos" — `MainViewModel.cs:27`, `Views/MetodosWindow.xaml`
- [x] Botão "Algoritmos Genéticos" exibe exatamente "Módulo em Desenvolvimento" — `MainViewModel.cs:28-29` (`MessageBox.Show("Módulo em Desenvolvimento", ...)`)

## 2. Interface "Sobre o Sistema"

- [x] Descrição do problema abordado — `Views/SobreWindow.xaml` (TextBlocks iniciais)
- [x] Dados da disciplina configuráveis em um único local — `src/OpcodaOtimizacao/DadosAcademicos.cs`
- [x] Placeholders claramente identificados `[PREENCHER]` sem nomes inventados — `DadosAcademicos.cs:5-10`

## 3. Interface "Configuração do Problema"

- [x] Componente "Tipo de Execução" com opções FIXO / ALEATÓRIO — `Views/ConfiguracaoWindow.xaml` (ComboBox), `ViewModels/ConfiguracaoViewModel.cs`
- [x] Componente "Tamanho do Problema" usado para ALEATÓRIO com validação de inválidos — `ConfiguracaoViewModel.cs:GerarProblema()`
- [x] Botão "Gerar Problema" executa método Gerar Problema — `ConfiguracaoViewModel.cs:GerarProblemaCommand`
- [x] Área de exibição apresenta índice, peso, valor, entropia, capacidade total — `ConfiguracaoViewModel.cs:Formatar()`

## 4. Interface "Métodos Básicos"

- [x] Seleção entre Subida de Encosta, Subida com Tentativas, Têmpera Simulada — `Views/MetodosWindow.xaml` (ComboBox), `MetodosViewModel.cs`
- [x] Componente para inserir TMAX (usado pela Subida com Tentativas) — `MetodosWindow.xaml` + `MostraTMax`/`VisibilidadeTMax`
- [x] Componentes Temperatura Inicial, Final e Fator Redutor para Têmpera — `MetodosWindow.xaml` + `VisibilidadeTempera`
- [x] Botão "Executar" roda o método selecionado e exibe resultado — `MetodosViewModel.cs:Executar()`
- [x] Botão "Análise Comparativa" com estrutura configurável documentada — `MetodosViewModel.cs:ExecutarAnalise()`, defaults em `Services/AnaliseComparativaService.cs` (`ConfiguracaoComparativa`)

## 5. Algoritmos

- [x] Subida de Encosta implementada corretamente — `Algorithms/Otimizadores.cs:SubidaDeEncosta`
- [x] Subida de Encosta com Tentativas respeitando TMAX — `Otimizadores.cs:SubidaDeEncostaComTentativas`
- [x] Têmpera Simulada com P = exp(-Δ/T) — `Otimizadores.cs:TemperaSimulada`
- [x] Representação vetor binário — `Models/Solucao.cs`
- [x] Função objetivo maximiza valor com restrição peso ≤ capacidade — `Solucao.Avaliar/EValido`
- [x] Estratégia de vizinhança definida (inclusão/exclusão/troca) — `VizinhancaMochila.GerarTodos/VizinhoAleatorio`
- [x] Reparação documentada impedindo soluções inválidas — `VizinhancaMochila.Reparar`; descrita em `docs/modelagem.md`

## 6. Contexto Opcoda / Mochila

- [x] Modelo como Problema da Mochila — `docs/modelagem.md`
- [x] Valor baseado em entropia de Shannon com penalização para 0x00 documentada — `Utilities/EntropiaShannon.cs`, `Services/GeradorProblema.cs:CalcularValor`
- [x] Não otimiza parâmetros de áudio; objeto é seleção de blocos — escopo mantido; nada de synth implementado

## 7. Arquivo de Entrada

- [x] Modo FIXO reproduzível — `GeradorProblema.GerarFixo()`
- [x] Modo ALEATÓRIO gerando instância válida pelo tamanho — `GeradorProblema.GerarAleatorio()`
- [x] Carregamento opcional de arquivo binário complementar — `GeradorProblema.CarregarDeArquivo()`
- [x] Execução não depende de .exe externo — modos FIXO/ALEATÓRIO autocontidos

## 8. Arquitetura / Stack

- [x] C# / .NET / WPF — `src/OpcodaOtimizacao/OpcodaOtimizacao.csproj` (net9.0-windows, UseWPF)
- [x] Estrutura Models/Algorithms/Services/Views/ViewModels/Utilities — pastas correspondentes
- [x] Branch principal intocada; trabalho isolado em `feature/otimizacao-opcoda` — confirmado via git

## 9. Documentação Acadêmica

- [x] Contextualização, problema, relação com Opcoda — `docs/metodologia.md`, `README_OPCODA_OTIMIZACAO.md`
- [x] Modelagem, representação, função objetivo, vizinhança — `docs/modelagem.md`
- [x] Os três algoritmos, critério de aceitação, parâmetros, complexidade — `docs/algoritmos.md`
- [x] Análise comparativa, limitações, conclusão — `docs/analise-comparativa.md`

## 10. LeiaMe.txt

- [x] Criado com requisitos, versão .NET, restauração, compilação, execução — `LeiaMe.txt` seções 1-6
- [x] Como configurar dados dos discentes — seção 9
- [x] Como executar cada algoritmo e análise comparativa — seções 10-11
- [x] Onde ficam resultados e possíveis problemas/soluções — seções 12-14
- [x] Comandos reais fornecidos (dotnet build/run/test)

## 11. PDF da Análise Comparativa

- [x] Mecanismo reproduzível lendo CSV real da aplicação — `ferramentas/gerar-pdf.ps1`
- [x] Sem números inventados; conteúdo vem dos resultados exportados
- [x] Identificação, problema, configuração, tabela, comparação, discussão, conclusão

## 12. Testes Automatizados

- [x] Função objetivo, validação de capacidade, solução válida, vizinhos, aceitação têmpera, instâncias pequenas, geração aleatória, todos excedem, todos cabem, valores iguais, peso zero — `src/OpcodaOtimizacao.Tests/OtimizacaoTests.cs` (21 testes, todos passando)

## 13. Validação Final

- [x] Compila sem erros/warnings
- [x] 21 testes aprovados
- [x] Interfaces solicitadas existem
- [x] "Algoritmos Genéticos" mostra exatamente "Módulo em Desenvolvimento"
- [ ] Execução manual das telas (requer ambiente gráfico; código verificado por build+testes)