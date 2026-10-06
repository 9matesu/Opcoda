namespace OpcodaOtimizacao.Models;

public sealed class ProblemaMochila
{
    public IReadOnlyList<Item> Itens { get; init; } = Array.Empty<Item>();
    public double Capacidade { get; init; }

    public int N => Itens.Count;
}