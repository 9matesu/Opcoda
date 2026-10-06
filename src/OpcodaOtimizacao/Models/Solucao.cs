using System.Diagnostics;

namespace OpcodaOtimizacao.Models;

public sealed class Solucao
{
    public bool[] Vetor { get; }
    public double ValorTotal { get; private set; }
    public double PesoTotal { get; private set; }
    public int ItensSelecionados { get; private set; }

    public Solucao(int n)
    {
        Vetor = new bool[n];
    }

    public Solucao(bool[] vetor)
    {
        Vetor = (bool[])vetor.Clone();
    }

    public static Solucao FromIndices(int n, IEnumerable<int> indices)
    {
        var s = new Solucao(n);
        foreach (var i in indices)
            s.Vetor[i] = true;
        return s;
    }

    public void Avaliar(ProblemaMochila p)
    {
        double v = 0, w = 0;
        int k = 0;
        for (int i = 0; i < p.N; i++)
        {
            if (!Vetor[i]) continue;
            v += p.Itens[i].Valor;
            w += p.Itens[i].Peso;
            k++;
        }
        ValorTotal = v;
        PesoTotal = w;
        ItensSelecionados = k;
    }

    public bool EValido(ProblemaMochila p) => PesoTotal <= p.Capacidade + 1e-9;

    public Solucao Clone() => new((bool[])Vetor.Clone());
}