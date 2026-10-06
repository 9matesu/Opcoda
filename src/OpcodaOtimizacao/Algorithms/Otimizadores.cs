using System.Diagnostics;
using OpcodaOtimizacao.Models;

namespace OpcodaOtimizacao.Algorithms;

public sealed class VizinhancaMochila
{
    public static IEnumerable<bool[]> GerarTodos(Solucao atual, ProblemaMochila p)
    {
        for (int i = 0; i < p.N; i++)
        {
            if (!atual.Vetor[i])
            {
                var novo = (bool[])atual.Vetor.Clone();
                novo[i] = true;
                yield return Reparar(novo, p);
            }
        }

        for (int i = 0; i < p.N; i++)
        {
            if (atual.Vetor[i])
            {
                var novo = (bool[])atual.Vetor.Clone();
                novo[i] = false;
                yield return novo;
            }
        }

        for (int i = 0; i < p.N; i++)
        {
            if (!atual.Vetor[i]) continue;
            for (int j = 0; j < p.N; j++)
            {
                if (atual.Vetor[j]) continue;
                var novo = (bool[])atual.Vetor.Clone();
                novo[i] = false;
                novo[j] = true;
                yield return Reparar(novo, p);
            }
        }
    }

    public static bool[] VizinhoAleatorio(Solucao atual, ProblemaMochila p, Random rng)
    {
        int tipo = rng.Next(3);
        var novo = (bool[])atual.Vetor.Clone();

        if (tipo == 0)
        {
            var naoSel = Enumerable.Range(0, p.N).Where(i => !atual.Vetor[i]).ToList();
            if (naoSel.Count == 0) return Reparar(novo, p);
            novo[naoSel[rng.Next(naoSel.Count)]] = true;
            return Reparar(novo, p);
        }

        if (tipo == 1)
        {
            var sel = Enumerable.Range(0, p.N).Where(i => atual.Vetor[i]).ToList();
            if (sel.Count == 0) return novo;
            novo[sel[rng.Next(sel.Count)]] = false;
            return novo;
        }

        var s = Enumerable.Range(0, p.N).Where(i => atual.Vetor[i]).ToList();
        var ns = Enumerable.Range(0, p.N).Where(i => !atual.Vetor[i]).ToList();
        if (s.Count == 0 || ns.Count == 0) return Reparar(novo, p);
        novo[s[rng.Next(s.Count)]] = false;
        novo[ns[rng.Next(ns.Count)]] = true;
        return Reparar(novo, p);
    }

    public static bool[] Reparar(bool[] vetor, ProblemaMochila p)
    {
        double peso = 0;
        for (int i = 0; i < p.N; i++)
            if (vetor[i]) peso += p.Itens[i].Peso;

        if (peso <= p.Capacidade + 1e-9) return vetor;

        var indices = Enumerable.Range(0, p.N).Where(i => vetor[i]).ToList();
        indices.Sort((a, b) =>
        {
            double raZa = p.Itens[a].Valor / Math.Max(p.Itens[a].Peso, 1e-9);
            double raZb = p.Itens[b].Valor / Math.Max(p.Itens[b].Peso, 1e-9);
            return raZa.CompareTo(raZb);
        });

        foreach (var idx in indices)
        {
            if (peso <= p.Capacidade + 1e-9) break;
            vetor[idx] = false;
            peso -= p.Itens[idx].Peso;
        }

        return vetor;
    }

    public static Solucao SolucaoInicial(ProblemaMochila p, Random rng)
    {
        var v = new bool[p.N];
        for (int i = 0; i < p.N; i++)
            v[i] = rng.NextDouble() < 0.5;
        Reparar(v, p);
        return new Solucao(v);
    }
}

public sealed class SubidaDeEncosta : IOptimizador
{
    private readonly Random _rng;

    public string Nome => "Subida de Encosta";

    public SubidaDeEncosta(Random? rng = null)
    {
        _rng = rng ?? new Random();
    }

    public ResultadoExecucao Executar(ProblemaMochila problema)
    {
        var sw = Stopwatch.StartNew();
        var atual = VizinhancaMochila.SolucaoInicial(problema, _rng);
        atual.Avaliar(problema);
        int iteracoes = 0;
        var evolucao = new List<double> { atual.ValorTotal };

        bool melhorou = true;
        while (melhorou)
        {
            melhorou = false;
            Solucao? melhor = null;

            foreach (var vizBool in VizinhancaMochila.GerarTodos(atual, problema))
            {
                var viz = new Solucao(vizBool);
                viz.Avaliar(problema);
                iteracoes++;

                if (!viz.EValido(problema)) continue;
                if (viz.ValorTotal > atual.ValorTotal && (melhor == null || viz.ValorTotal > melhor.ValorTotal))
                    melhor = viz;
            }

            if (melhor != null)
            {
                atual = melhor;
                melhorou = true;
                evolucao.Add(atual.ValorTotal);
            }
        }

        sw.Stop();
        return new ResultadoExecucao
        {
            Metodo = Nome,
            MelhorSolucao = atual,
            ValorFinal = atual.ValorTotal,
            PesoFinal = atual.PesoTotal,
            Capacidade = problema.Capacidade,
            ItensSelecionados = atual.ItensSelecionados,
            Iteracoes = iteracoes,
            TempoMs = sw.ElapsedMilliseconds,
            Valido = atual.EValido(problema),
            Parametros = new[] { $"N={problema.N}", $"Capacidade={problema.Capacidade}" },
            Evolucao = evolucao,
        };
    }
}

public sealed class SubidaDeEncostaComTentativas : IOptimizador
{
    private readonly int _tMax;
    private readonly Random _rng;

    public string Nome => "Subida de Encosta com Tentativas";

    public SubidaDeEncostaComTentativas(int tMax, Random? rng = null)
    {
        _tMax = tMax;
        _rng = rng ?? new Random();
    }

    public ResultadoExecucao Executar(ProblemaMochila problema)
    {
        var sw = Stopwatch.StartNew();
        Solucao? globalMelhor = null;
        int totalIteracoes = 0;
        var evolucao = new List<double>();

        for (int tentativa = 0; tentativa < _tMax; tentativa++)
        {
            var atual = VizinhancaMochila.SolucaoInicial(problema, _rng);
            atual.Avaliar(problema);

            bool melhorou = true;
            while (melhorou)
            {
                melhorou = false;
                Solucao? melhor = null;

                foreach (var vizBool in VizinhancaMochila.GerarTodos(atual, problema))
                {
                    var viz = new Solucao(vizBool);
                    viz.Avaliar(problema);
                    totalIteracoes++;

                    if (!viz.EValido(problema)) continue;
                    if (viz.ValorTotal > atual.ValorTotal && (melhor == null || viz.ValorTotal > melhor.ValorTotal))
                        melhor = viz;
                }

                if (melhor != null)
                {
                    atual = melhor;
                    melhorou = true;
                }
            }

            evolucao.Add(atual.ValorTotal);
            if (globalMelhor == null || atual.ValorTotal > globalMelhor.ValorTotal)
                globalMelhor = atual;
        }

        sw.Stop();
        return new ResultadoExecucao
        {
            Metodo = Nome,
            MelhorSolucao = globalMelhor,
            ValorFinal = globalMelhor!.ValorTotal,
            PesoFinal = globalMelhor.PesoTotal,
            Capacidade = problema.Capacidade,
            ItensSelecionados = globalMelhor.ItensSelecionados,
            Iteracoes = totalIteracoes,
            TempoMs = sw.ElapsedMilliseconds,
            Valido = globalMelhor.EValido(problema),
            Parametros = new[] { $"TMAX={_tMax}", $"N={problema.N}" },
            Evolucao = evolucao,
        };
    }
}

public sealed class TemperaSimulada : IOptimizador
{
    private readonly double _tempInicial;
    private readonly double _tempFinal;
    private readonly double _fatorRedutor;
    private readonly Random _rng;

    public string Nome => "Têmpera Simulada";

    public TemperaSimulada(double tempInicial, double tempFinal, double fatorRedutor, Random? rng = null)
    {
        _tempInicial = tempInicial;
        _tempFinal = tempFinal;
        _fatorRedutor = fatorRedutor;
        _rng = rng ?? new Random();
    }

    public ResultadoExecucao Executar(ProblemaMochila problema)
    {
        var sw = Stopwatch.StartNew();
        var atual = VizinhancaMochila.SolucaoInicial(problema, _rng);
        atual.Avaliar(problema);
        var melhor = atual.Clone();
        melhor.Avaliar(problema);

        double t = _tempInicial;
        int iteracoes = 0;
        var evolucao = new List<double> { atual.ValorTotal };

        while (t >= _tempFinal)
        {
            var vizBool = VizinhancaMochila.VizinhoAleatorio(atual, problema, _rng);
            var viz = new Solucao(vizBool);
            viz.Avaliar(problema);
            iteracoes++;

            if (viz.EValido(problema))
            {
                double delta = viz.ValorTotal - atual.ValorTotal;
                if (delta > 0)
                {
                    atual = viz;
                }
                else
                {
                    double prob = Math.Exp(-delta / t);
                    if (_rng.NextDouble() < prob)
                        atual = viz;
                }

                if (atual.ValorTotal > melhor.ValorTotal)
                {
                    melhor = atual.Clone();
                    melhor.Avaliar(problema);
                }
            }

            evolucao.Add(melhor.ValorTotal);
            t *= _fatorRedutor;
        }

        sw.Stop();
        return new ResultadoExecucao
        {
            Metodo = Nome,
            MelhorSolucao = melhor,
            ValorFinal = melhor.ValorTotal,
            PesoFinal = melhor.PesoTotal,
            Capacidade = problema.Capacidade,
            ItensSelecionados = melhor.ItensSelecionados,
            Iteracoes = iteracoes,
            TempoMs = sw.ElapsedMilliseconds,
            Valido = melhor.EValido(problema),
            Parametros = new[] { $"TempInicial={_tempInicial}", $"TempFinal={_tempFinal}", $"FatorRedutor={_fatorRedutor}", $"N={problema.N}" },
            Evolucao = evolucao,
        };
    }
}