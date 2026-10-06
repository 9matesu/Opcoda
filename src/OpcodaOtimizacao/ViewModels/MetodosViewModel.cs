using System.Globalization;
using System.IO;
using System.Text;
using System.Windows;
using OpcodaOtimizacao.Algorithms;
using OpcodaOtimizacao.Models;
using OpcodaOtimizacao.Services;

namespace OpcodaOtimizacao.ViewModels;

public enum MetodoSelecionado
{
    SubidaDeEncosta,
    SubidaDeEncostaComTentativas,
    TemperaSimulada,
}

public sealed class MetodosViewModel : ViewModelBase
{
    private readonly ConfiguracaoViewModel _configVm;

    private MetodoSelecionado _metodo = MetodoSelecionado.SubidaDeEncosta;
    private string _tMaxTexto = "10";
    private string _tempInicialTexto = "100";
    private string _tempFinalTexto = "1";
    private string _fatorRedutorTexto = "0.95";
    private string _saida = "";

    public MetodoSelecionado Metodo
    {
        get => _metodo;
        set { SetField(ref _metodo, value); OnPropertyChanged(nameof(MetodoIdx)); OnPropertyChanged(nameof(VisibilidadeTMax)); OnPropertyChanged(nameof(VisibilidadeTempera)); }
    }

    public int MetodoIdx
    {
        get => (int)_metodo;
        set => Metodo = (MetodoSelecionado)value;
    }

    public Visibility VisibilidadeTMax =>
        Metodo == MetodoSelecionado.SubidaDeEncostaComTentativas ? Visibility.Visible : Visibility.Collapsed;

    public Visibility VisibilidadeTempera =>
        Metodo == MetodoSelecionado.TemperaSimulada ? Visibility.Visible : Visibility.Collapsed;

    public string TMaxTexto { get => _tMaxTexto; set => SetField(ref _tMaxTexto, value); }
    public string TempInicialTexto { get => _tempInicialTexto; set => SetField(ref _tempInicialTexto, value); }
    public string TempFinalTexto { get => _tempFinalTexto; set => SetField(ref _tempFinalTexto, value); }
    public string FatorRedutorTexto { get => _fatorRedutorTexto; set => SetField(ref _fatorRedutorTexto, value); }

    public string Saida { get => _saida; set => SetField(ref _saida, value); }

    public RelayCommand ExecutarCommand { get; }
    public RelayCommand AnaliseComparativaCommand { get; }

    public MetodosViewModel(ConfiguracaoViewModel configVm)
    {
        _configVm = configVm;
        ExecutarCommand = new RelayCommand(_ => Executar());
        AnaliseComparativaCommand = new RelayCommand(_ => ExecutarAnalise());
    }

    private void Executar()
    {
        var problema = _configVm.ProblemaAtual;
        if (problema == null)
        {
            Saida = "Gere um problema na tela de Configurações antes de executar.";
            return;
        }

        try
        {
            IOptimizador opt = Metodo switch
            {
                MetodoSelecionado.SubidaDeEncosta => new SubidaDeEncosta(),
                MetodoSelecionado.SubidaDeEncostaComTentativas => new SubidaDeEncostaComTentativas(LerInt(TMaxTexto, "TMAX")),
                MetodoSelecionado.TemperaSimulada => new TemperaSimulada(
                    LerDouble(TempInicialTexto, "Temperatura Inicial"),
                    LerDouble(TempFinalTexto, "Temperatura Final"),
                    LerDouble(FatorRedutorTexto, "Fator Redutor")),
                _ => throw new InvalidOperationException("Método desconhecido."),
            };

            var res = opt.Executar(problema);
            Saida = FormatarResultado(res);
        }
        catch (Exception ex)
        {
            Saida = "Erro: " + ex.Message;
        }
    }

    private void ExecutarAnalise()
    {
        var problema = _configVm.ProblemaAtual;
        if (problema == null)
        {
            Saida = "Gere um problema na tela de Configurações antes da análise.";
            return;
        }

        var cfg = new ConfiguracaoComparativa
        {
            TMax = LerInt(TMaxTexto, "TMAX"),
            TemperaturaInicial = LerDouble(TempInicialTexto, "Temperatura Inicial"),
            TemperaturaFinal = LerDouble(TempFinalTexto, "Temperatura Final"),
            FatorRedutor = LerDouble(FatorRedutorTexto, "Fator Redutor"),
        };

        var svc = new AnaliseComparativaService(cfg);
        var resultados = svc.Executar(problema);

        var sb = new StringBuilder();
        sb.AppendLine("=== ANÁLISE COMPARATIVA ===");
        sb.AppendLine($"N={problema.N}  Capacidade={problema.Capacidade.ToString(CultureInfo.InvariantCulture)}");
        sb.AppendLine();
        foreach (var r in resultados)
            sb.Append(FormatarResultado(r)).AppendLine();

        ExportarResultados(resultados, problema);
        Saida = sb.ToString();
    }

    private static void ExportarResultados(IReadOnlyList<ResultadoExecucao> resultados, ProblemaMochila p)
    {
        try
        {
            var dir = Path.Combine(Directory.GetCurrentDirectory(), "results");
            Directory.CreateDirectory(dir);
            var csvPath = Path.Combine(dir, $"analise_{DateTime.Now:yyyyMMdd_HHmmss}.csv");
            var sb = new StringBuilder();
            sb.AppendLine("Metodo;ValorFinal;PesoFinal;Capacidade;ItensSelecionados;Iteracoes;TempoMs;Valido;Parametros");
            foreach (var r in resultados)
            {
                sb.AppendLine(string.Join(';', new[]
                {
                    r.Metodo,
                    Num(r.ValorFinal), Num(r.PesoFinal), Num(r.Capacidade),
                    r.ItensSelecionados.ToString(CultureInfo.InvariantCulture),
                    r.Iteracoes.ToString(CultureInfo.InvariantCulture),
                    r.TempoMs.ToString(CultureInfo.InvariantCulture),
                    r.Valido ? "sim" : "nao",
                    string.Join(",", r.Parametros),
                }));
            }
            File.WriteAllText(csvPath, sb.ToString());
        }
        catch
        {
        }
    }

    private static string Num(double v) => v.ToString("F4", CultureInfo.InvariantCulture);

    private static string FormatarResultado(ResultadoExecucao r)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"--- {r.Metodo} ---");
        sb.AppendLine($"Valor final:          {Num(r.ValorFinal)}");
        sb.AppendLine($"Peso total:           {Num(r.PesoFinal)}");
        sb.AppendLine($"Capacidade:           {Num(r.Capacidade)}");
        sb.AppendLine($"Itens selecionados:   {r.ItensSelecionados}");
        sb.AppendLine($"Iterações:            {r.Iteracoes}");
        sb.AppendLine($"Tempo (ms):           {r.TempoMs}");
        sb.AppendLine($"Solução válida:       {(r.Valido ? "SIM" : "NÃO")}");
        sb.AppendLine($"Parâmetros:           {string.Join(", ", r.Parametros)}");
        if (r.MelhorSolucao != null)
        {
            var indices = Enumerable.Range(0, r.MelhorSolucao.Vetor.Length)
                .Where(i => r.MelhorSolucao.Vetor[i]);
            sb.AppendLine($"Itens escolhidos:     [{string.Join(", ", indices)}]");
        }
        sb.AppendLine();
        return sb.ToString();
    }

    private static int LerInt(string s, string nome)
    {
        if (!int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out int v) || v < 1)
            throw new ArgumentException($"{nome} deve ser inteiro >= 1.");
        return v;
    }

    private static double LerDouble(string s, string nome)
    {
        if (!double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out double v))
            throw new ArgumentException($"{nome} deve ser numérico válido.");
        return v;
    }
}