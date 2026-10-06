# Metodologia

## Etapas seguidas no desenvolvimento deste trabalho acadêmico

1. Leitura atenta do enunciado entregue pelo professor, identificando todos os itens obrigatórios (interfaces, algoritmos, parâmetros, documentação, PDF de análise).
2. Análise do repositório existente do Opcoda (C++/CMake/JUCE) para garantir isolamento total desta atividade. Decidiu-se não alterar nenhum arquivo da branch principal nem interferir no build nativo.
3. Criação de uma nova branch dedicada (`feature/otimizacao-opcoda`) onde todo o código do subprojeto foi desenvolvido.
4. Escolha da stack: .NET 9 + WPF, por ser simples, reproduzível em qualquer Windows moderno e adequada para protótipos acadêmicos com interface gráfica básica.
5. Modelagem formal do problema como Mochila 0/1, definindo peso, valor, capacidade, representação binária da solução, função objetivo e operadores de vizinhança. Documentada em `docs/modelagem.md`.
6. Implementação incremental:
    - utilitário de entropia de Shannon portado logicamente do núcleo C++ já presente no Opcoda;
    - geradores de instância FIXA (determinística) e ALEATÓRIA (controlada pelo tamanho informado);
    - suporte opcional para carregar blocos de um arquivo binário real;
    - classes base de modelo (`Item`, `ProblemaMochila`, `Solucao`);
    - três otimizadores independentes atrás de uma interface comum;
    - camada MVVM mínima ligando modelos às janelas XAML.
7. Escrita de testes automatizados xUnit cobrindo casos-limite previstos no enunciado (todos excedem capacidade, todos cabem, valores iguais, peso zero, reparação, etc.).
8. Validação contínua com `dotnet build` e `dotnet test` após cada alteração relevante.
9. Produção da documentação acadêmica separada por tema: modelagem, algoritmos, análise comparativa e esta metodologia.
10. Preparação do mecanismo reproduzível de exportação dos resultados reais para CSV e conversão em PDF via script PowerShell incluído em `ferramentas/gerar-pdf.ps1`.
11. Revisão final contra o checklist completo do enunciado, registrado em `docs/REQUISITOS_PROFESSOR.md`.

## Princípios adotados

- Simplicidade sobre sofisticação: nada além do solicitado pelo professor foi implementado.
- Isolamento rigoroso em relação ao projeto principal do Opcoda.
- Reprodutibilidade: modo FIXO sempre idêntico; modo ALEATÓRIO controlado pelo tamanho informado; seeds opcionais nos testes.
- Transparência dos parâmetros: todos expostos na interface e registrados nos relatórios.
- Linguagem acessível para apresentação oral em sala de graduação.