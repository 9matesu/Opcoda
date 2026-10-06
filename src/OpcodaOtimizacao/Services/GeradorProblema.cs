using System.IO;
using OpcodaOtimizacao.Models;
using OpcodaOtimizacao.Utilities;

namespace OpcodaOtimizacao.Services;

public enum TipoExecucao
{
    Fixo,
    Aleatorio,
}

public static class GeradorProblema
{
    public static ProblemaMochila Gerar(TipoExecucao tipo, int tamanho, Random? rng = null)
    {
        return tipo switch
        {
            TipoExecucao.Fixo => GerarFixo(),
            TipoExecucao.Aleatorio => GerarAleatorio(tamanho, rng ?? new Random()),
            _ => throw new ArgumentOutOfRangeException(nameof(tipo)),
        };
    }

    public static ProblemaMochila GerarFixo()
    {
        var itens = new List<Item>
        {
            Novo(0, 10, 8.0),
            Novo(1, 12, 9.5),
            Novo(2, 7, 6.0),
            Novo(3, 15, 11.0),
            Novo(4, 9, 7.5),
            Novo(5, 18, 12.0),
            Novo(6, 5, 4.0),
            Novo(7, 11, 9.0),
            Novo(8, 14, 10.5),
            Novo(9, 8, 6.5),
            Novo(10, 16, 11.5),
            Novo(11, 6, 5.0),
            Novo(12, 13, 10.0),
            Novo(13, 19, 12.5),
            Novo(14, 4, 3.5),
        };

        return new ProblemaMochila { Itens = itens, Capacidade = 50 };
    }

    public static ProblemaMochila GerarAleatorio(int n, Random rng)
    {
        if (n < 1)
            throw new ArgumentOutOfRangeException(nameof(n), "Tamanho deve ser >= 1.");

        var itens = new List<Item>(n);
        for (int i = 0; i < n; i++)
        {
            double peso = rng.Next(1, 21);
            double entropia = Math.Round(rng.NextDouble() * 8.0, 4);
            double proporcaoZero = Math.Round(rng.NextDouble() * 0.9, 4);
            double valor = CalcularValor(entropia, proporcaoZero);
            int tamBytes = (int)(peso * 1024);

            itens.Add(new Item
            {
                Index = i,
                Peso = peso,
                Valor = valor,
                Entropia = entropia,
                ProporcaoZero = proporcaoZero,
                TamanhoBytes = tamBytes,
            });
        }

        double somaPesos = itens.Sum(x => x.Peso);
        double capacidade = Math.Round(somaPesos * 0.5, 4);

        return new ProblemaMochila { Itens = itens, Capacidade = capacidade };
    }

    public static ProblemaMochila CarregarDeArquivo(string caminho, int tamanhoBloco = 1024)
    {
        byte[] dados = File.ReadAllBytes(caminho);
        if (dados.Length == 0)
            throw new ArgumentException("Arquivo vazio.", nameof(caminho));

        var itens = new List<Item>();
        int idx = 0;
        for (int off = 0; off < dados.Length; off += tamanhoBloco)
        {
            int len = Math.Min(tamanhoBloco, dados.Length - off);
            var bloco = dados.AsSpan(off, len);
            double entropia = EntropiaShannon.BitsPorByte(bloco);
            double pz = EntropiaShannon.ProporcaoZero(bloco);
            double valor = CalcularValor(entropia, pz);
            double peso = len / 1024.0;

            itens.Add(new Item
            {
                Index = idx++,
                Peso = Math.Round(peso, 4),
                Valor = Math.Round(valor, 4),
                Entropia = Math.Round(entropia, 4),
                ProporcaoZero = Math.Round(pz, 4),
                TamanhoBytes = len,
            });
        }

        double soma = itens.Sum(x => x.Peso);
        return new ProblemaMochila { Itens = itens, Capacidade = Math.Round(soma * 0.5, 4) };
    }

    public static double CalcularValor(double entropia, double proporcaoZero)
    {
        double penalidade = 1.0 - proporcaoZero;
        return Math.Round(entropia * penalidade, 4);
    }

    private static Item Novo(int index, double peso, double valor)
    {
        return new Item
        {
            Index = index,
            Peso = peso,
            Valor = valor,
            Entropia = valor,
            ProporcaoZero = 0.0,
            TamanhoBytes = (int)(peso * 1024),
        };
    }
}