using System.Globalization;
using System.Text;
using OpcodaOtimizacao.Models;
using OpcodaOtimizacao.Services;

namespace OpcodaOtimizacao.ViewModels;

public sealed class ConfiguracaoViewModel : ViewModelBase
{
    private TipoExecucao _tipo = TipoExecucao.Fixo;
    private string _tamanhoTexto = "30";
    private string _saida = "";
    private ProblemaMochila? _problemaAtual;

    public TipoExecucao Tipo
    {
        get => _tipo;
        set => SetField(ref _tipo, value);
    }

    public int TipoIdx
    {
        get => (int)_tipo;
        set => Tipo = (TipoExecucao)value;
    }

    public string TamanhoTexto
    {
        get => _tamanhoTexto;
        set => SetField(ref _tamanhoTexto, value);
    }

    public string Saida
    {
        get => _saida;
        set => SetField(ref _saida, value);
    }

    public ProblemaMochila? ProblemaAtual
    {
        get => _problemaAtual;
        private set => SetField(ref _problemaAtual, value);
    }

    public RelayCommand GerarProblemaCommand { get; }

    public ConfiguracaoViewModel()
    {
        GerarProblemaCommand = new RelayCommand(_ => GerarProblema());
    }

    public void GerarProblema()
    {
        try
        {
            int tamanho = 0;
            if (Tipo == TipoExecucao.Aleatorio)
            {
                if (!int.TryParse(TamanhoTexto, NumberStyles.Integer, CultureInfo.InvariantCulture, out tamanho) || tamanho < 1)
                {
                    Saida = "Erro: tamanho do problema deve ser um inteiro >= 1.";
                    ProblemaAtual = null;
                    return;
                }
            }

            ProblemaAtual = GeradorProblema.Gerar(Tipo, tamanho);
            Saida = Formatar(ProblemaAtual, Tipo.ToString().ToUpperInvariant());
        }
        catch (Exception ex)
        {
            Saida = "Erro ao gerar problema: " + ex.Message;
            ProblemaAtual = null;
        }
    }

    internal static string Formatar(ProblemaMochila p, string cabecalho)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"=== Problema gerado ({cabecalho}) ===");
        sb.AppendLine($"Quantidade de itens: {p.N}");
        sb.AppendLine($"Capacidade total:    {p.Capacidade.ToString(CultureInfo.InvariantCulture)}");
        sb.AppendLine();
        sb.AppendLine("Índice | Peso   | Valor  | Entropia | %Zero  | TamBytes");
        sb.AppendLine("-------|--------|--------|----------|--------|---------");
        foreach (var it in p.Itens)
        {
            sb.AppendLine(string.Format(CultureInfo.InvariantCulture,
                "{0,6} | {1,6:F2} | {2,6:F4} | {3,8:F4} | {4,6:F4} | {5,8}",
                it.Index, it.Peso, it.Valor, it.Entropia, it.ProporcaoZero, it.TamanhoBytes));
        }
        return sb.ToString();
    }
}