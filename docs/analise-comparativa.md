# Análise Comparativa

## Objetivo

Responder experimentalmente às seguintes questões:

1. Qual método encontrou a melhor solução?
2. Qual método foi mais rápido?
3. Qual método apresentou maior estabilidade entre execuções?
4. Como os parâmetros da Têmpera Simulada influenciaram o resultado?
5. Como o tamanho do problema influencia cada método?

## Metodologia

Para cada instância (fixa ou aleatória de tamanho informado):

1. Executar Subida de Encosta.
2. Executar Subida de Encosta com Tentativas usando TMAX configurado.
3. Executar Têmpera Simulada usando (T0, Tf, α) configurados.
4. Registrar para cada execução:
    - Valor final da solução;
    - Peso total consumido;
    - Capacidade disponível;
    - Quantidade de itens selecionados;
    - Número de avaliações de vizinhos (iterações);
    - Tempo decorrido em milissegundos;
    - Lista de índices escolhidos;
    - Curva de evolução da função objetivo.
5. Exportar tudo em CSV na pasta `results/` com timestamp.

As configurações padrão estão centralizadas em `ConfiguracaoComparativa`, no arquivo `src/OpcodaOtimizacao/Services/AnaliseComparativaService.cs`. Alterar ali muda os defaults sem tocar na interface. Novos conjuntos experimentais definidos em aula podem ser adicionados estendendo essa classe.

## Métricas derivadas

A partir do CSV é possível calcular:

- Razão qualidade/tempo: ValorFinal / TempoMs.
- Estabilidade: desvio-padrão do ValorFinal entre múltiplas execuções com seeds distintas.
- Sensibilidade paramétrica: variar T0, Tf ou α e observar mudança no ValorFinal médio.
- Escalabilidade: repetir medições para N crescente (ex.: 10, 30, 100, 300 itens).

## Discussão esperada

Em linhas gerais espera-se observar:

- Subida de Encosta pura tende a convergir rápido mas frequentemente fica em ótimo local moderado.
- Random Restart melhora a qualidade média ao custo proporcional de tempo (multiplicado por TMAX).
- Têmpera Simulada oferece melhor trade-off quando T0 suficientemente alto para permitir exploração e α suficientemente próximo de 1 para dar tempo de refinar perto do fim.

Resultados concretos devem vir apenas das execuções reais registradas nos CSVs exportados pela aplicação. Nenhum número desta seção deve ser interpretado como resultado obtido — trata-se exclusivamente da metodologia prevista.

## Limitações

- Instâncias sintéticas não capturam necessariamente a estrutura real de um PE. Para avaliação definitiva seria preciso rodar sobre arquivos executáveis verdadeiros via `GeradorProblema.CarregarDeArquivo`.
- As métricas temporais dependem fortemente da máquina. Recomenda-se relatar hardware junto dos resultados.
- Busca local não garante otimalidade. Não há comparação com solver exato neste escopo acadêmico.