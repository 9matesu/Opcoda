using OpcodaOtimizacao.Algorithms;
using OpcodaOtimizacao.Models;

namespace OpcodaOtimizacao.Services;

public sealed class ConfiguracaoComparativa
{
    public int TMax { get; set; } = 10;
    public double TemperaturaInicial { get; set; } = 100.0;
    public double TemperaturaFinal { get; set; } = 1.0;
    public double FatorRedutor { get; set; } = 0.95;
}

public sealed class AnaliseComparativaService
{
    private readonly ConfiguracaoComparativa _config;

    public AnaliseComparativaService(ConfiguracaoComparativa config)
    {
        _config = config;
    }

    public IReadOnlyList<ResultadoExecucao> Executar(ProblemaMochila problema, Random? rng = null)
    {
        var r = rng ?? new Random(42);
        var resultados = new List<ResultadoExecucao>
        {
            new SubidaDeEncosta(new Random(r.Next())).Executar(problema),
            new SubidaDeEncostaComTentativas(_config.TMax, new Random(r.Next())).Executar(problema),
            new TemperaSimulada(_config.TemperaturaInicial, _config.TemperaturaFinal, _config.FatorRedutor, new Random(r.Next())).Executar(problema),
        };
        return resultados;
    }
}