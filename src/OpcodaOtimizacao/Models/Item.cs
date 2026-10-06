namespace OpcodaOtimizacao.Models;

public sealed class Item
{
    public int Index { get; init; }
    public double Peso { get; init; }
    public double Valor { get; init; }
    public double Entropia { get; init; }
    public int TamanhoBytes { get; init; }
    public double ProporcaoZero { get; init; }
}