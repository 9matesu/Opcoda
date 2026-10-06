using Xunit;
using OpcodaOtimizacao.Models;
using OpcodaOtimizacao.Services;
using OpcodaOtimizacao.Algorithms;
using OpcodaOtimizacao.Utilities;

namespace OpcodaOtimizacao.Tests;

public class EntropiaTests
{
    [Fact]
    public void Entropia_DadosUniformes_RetornaZero()
    {
        var dados = new byte[100];
        Assert.Equal(0.0, EntropiaShannon.BitsPorByte(dados), 4);
    }

    [Fact]
    public void Entropia_Diversos_MaiorQueUniforme()
    {
        var uniforme = new byte[100];
        var diversos = new byte[100];
        for (int i = 0; i < diversos.Length; i++) diversos[i] = (byte)(i % 256);
        Assert.True(EntropiaShannon.BitsPorByte(diversos) > EntropiaShannon.BitsPorByte(uniforme));
    }

    [Fact]
    public void ProporcaoZero_TudoZero_RetornaUm()
    {
        var dados = new byte[50];
        Assert.Equal(1.0, EntropiaShannon.ProporcaoZero(dados), 4);
    }

    [Fact]
    public void Entropia_Vazio_RetornaZero()
    {
        Assert.Equal(0.0, EntropiaShannon.BitsPorByte(ReadOnlySpan<byte>.Empty));
    }
}

public class SolucaoTests
{
    [Fact]
    public void Avaliar_SomaCorretamente()
    {
        var p = GeradorProblema.GerarFixo();
        var s = Solucao.FromIndices(p.N, new[] { 0, 1, 2 });
        s.Avaliar(p);
        double esperado = p.Itens[0].Valor + p.Itens[1].Valor + p.Itens[2].Valor;
        Assert.Equal(esperado, s.ValorTotal, 4);
        Assert.Equal(3, s.ItensSelecionados);
    }

    [Fact]
    public void EValido_PesoExcedeCapacidade_RetornaFalso()
    {
        var itens = new List<Item>
        {
            new Item { Index = 0, Peso = 100, Valor = 1 },
            new Item { Index = 1, Peso = 100, Valor = 1 },
        };
        var p = new ProblemaMochila { Itens = itens, Capacidade = 50 };
        var s = Solucao.FromIndices(2, new[] { 0, 1 });
        s.Avaliar(p);
        Assert.False(s.EValido(p));
    }

    [Fact]
    public void Clone_NaoAfetaOriginal()
    {
        var s = new Solucao(new[] { true, false, true });
        var c = s.Clone();
        c.Vetor[0] = false;
        Assert.True(s.Vetor[0]);
    }
}

public class GeradorProblemaTests
{
    [Fact]
    public void Fixo_GeraQuinzeItens()
    {
        var p = GeradorProblema.GerarFixo();
        Assert.Equal(15, p.N);
        Assert.Equal(50, p.Capacidade);
    }

    [Fact]
    public void Aleatorio_RespeitaTamanho()
    {
        var p = GeradorProblema.GerarAleatorio(20, new Random(1));
        Assert.Equal(20, p.N);
        Assert.True(p.Capacidade > 0);
    }

    [Fact]
    public void Aleatorio_TamanhoInvalido_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => GeradorProblema.GerarAleatorio(0, new Random()));
    }

    [Fact]
    public void CalcularValor_AplicaPenalizacaoZero()
    {
        double v = GeradorProblema.CalcularValor(8.0, 0.5);
        Assert.Equal(4.0, v, 4);
    }
}

public class ReparacaoTests
{
    [Fact]
    public void Reparar_ReduzPesoParaCapacidade()
    {
        var p = GeradorProblema.GerarFixo();
        var vetor = Enumerable.Range(0, p.N).Select(_ => true).ToArray();
        var reparado = VizinhancaMochila.Reparar(vetor, p);
        var s = new Solucao(reparado);
        s.Avaliar(p);
        Assert.True(s.EValido(p));
    }

    [Fact]
    public void Reparar_JaValido_NaoAltera()
    {
        var p = GeradorProblema.GerarFixo();
        var vetor = new bool[p.N];
        vetor[0] = true;
        var reparado = VizinhancaMochila.Reparar((bool[])vetor.Clone(), p);
        Assert.True(reparado[0]);
    }
}

public class AlgoritmosTests
{
    [Fact]
    public void SubidaDeEncosta_RetornaSolucaoValida()
    {
        var p = GeradorProblema.GerarFixo();
        var res = new SubidaDeEncosta(new Random(42)).Executar(p);
        Assert.True(res.Valido);
        Assert.True(res.ValorFinal > 0);
        Assert.True(res.PesoFinal <= p.Capacidade + 1e-9);
    }

    [Fact]
    public void SubidaComTentativas_MelhorOuIgualASimples()
    {
        var p = GeradorProblema.GerarFixo();
        var simples = new SubidaDeEncosta(new Random(1)).Executar(p);
        var tentativas = new SubidaDeEncostaComTentativas(20, new Random(1)).Executar(p);
        Assert.True(tentativas.ValorFinal >= simples.ValorFinal - 1e-9);
        Assert.True(tentativas.Valido);
    }

    [Fact]
    public void TemperaSimulada_RetornaSolucaoValida()
    {
        var p = GeradorProblema.GerarFixo();
        var res = new TemperaSimulada(100, 1, 0.95, new Random(42)).Executar(p);
        Assert.True(res.Valido);
        Assert.True(res.PesoFinal <= p.Capacidade + 1e-9);
    }

    [Fact]
    public void TodosItensExcedemCapacidade_SolucaoVazia()
    {
        var itens = new List<Item>
        {
            new Item { Index = 0, Peso = 100, Valor = 10 },
            new Item { Index = 1, Peso = 200, Valor = 20 },
        };
        var p = new ProblemaMochila { Itens = itens, Capacidade = 10 };
        var res = new SubidaDeEncosta(new Random(1)).Executar(p);
        Assert.True(res.Valido);
        Assert.Equal(0, res.ItensSelecionados);
    }

    [Fact]
    public void TodosCabem_SelecionaTodos()
    {
        var itens = new List<Item>
        {
            new Item { Index = 0, Peso = 1, Valor = 5 },
            new Item { Index = 1, Peso = 1, Valor = 5 },
            new Item { Index = 2, Peso = 1, Valor = 5 },
        };
        var p = new ProblemaMochila { Itens = itens, Capacidade = 100 };
        var res = new SubidaDeEncosta(new Random(1)).Executar(p);
        Assert.Equal(15, res.ValorFinal, 4);
        Assert.Equal(3, res.ItensSelecionados);
    }

    [Fact]
    public void ValoresIguais_EncontraSolucaoValida()
    {
        var itens = Enumerable.Range(0, 10).Select(i => new Item { Index = i, Peso = 5, Valor = 3 }).ToList();
        var p = new ProblemaMochila { Itens = itens, Capacidade = 20 };
        var res = new SubidaDeEncosta(new Random(1)).Executar(p);
        Assert.True(res.Valido);
        Assert.True(res.PesoFinal <= p.Capacidade + 1e-9);
    }

    [Fact]
    public void PesoZero_EPermitido()
    {
        var itens = new List<Item>
        {
            new Item { Index = 0, Peso = 0, Valor = 10 },
            new Item { Index = 1, Peso = 5, Valor = 3 },
        };
        var p = new ProblemaMochila { Itens = itens, Capacidade = 5 };
        var res = new SubidaDeEncosta(new Random(1)).Executar(p);
        Assert.True(res.Valido);
        Assert.True(res.ValorFinal >= 10);
    }
}

public class AnaliseComparativaTests
{
    [Fact]
    public void Executar_ReturnsTresResultados()
    {
        var p = GeradorProblema.GerarFixo();
        var svc = new AnaliseComparativaService(new ConfiguracaoComparativa());
        var res = svc.Executar(p, new Random(1));
        Assert.Equal(3, res.Count);
        foreach (var r in res)
            Assert.True(r.Valido);
    }
}