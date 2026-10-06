using System.Diagnostics;
using OpcodaOtimizacao.Models;

namespace OpcodaOtimizacao.Algorithms;

public sealed class ResultadoExecucao
{
    public required string Metodo { get; init; }
    public Solucao? MelhorSolucao { get; init; }
    public double ValorFinal { get; init; }
    public double PesoFinal { get; init; }
    public double Capacidade { get; init; }
    public int ItensSelecionados { get; init; }
    public int Iteracoes { get; init; }
    public long TempoMs { get; init; }
    public bool Valido { get; init; }
    public IReadOnlyList<string> Parametros { get; init; } = Array.Empty<string>();
    public IReadOnlyList<double> Evolucao { get; init; } = Array.Empty<double>();
}

public interface IOptimizador
{
    string Nome { get; }
    ResultadoExecucao Executar(ProblemaMochila problema);
}