using System.Windows;
using OpcodaOtimizacao.Models;
using OpcodaOtimizacao.Services;
using OpcodaOtimizacao.Utilities;
using OpcodaOtimizacao.Views;

namespace OpcodaOtimizacao.ViewModels;

public sealed class MainViewModel : ViewModelBase
{
    private string _nomeArquivo = "";
    private string _infoArquivo = "Nenhum arquivo carregado";
    private string _conteudoHex = "0x00000000  (nenhum dado)";
    private string _bitsPorByte = "Entropia: -";
    private string _frequenciaInfo = "";
    private string _posicaoInfo = "";
    private string _statusRodape = "Pronto";

    public RelayCommand MostrarSobreCommand { get; }
    public RelayCommand MostrarAlgoritmosGeneticosCommand { get; }

    public ConfiguracaoViewModel ConfiguracaoVm { get; }
    public MetodosViewModel MetodosVm { get; }

    public ProblemaMochila? ProblemaAtual => ConfiguracaoVm.ProblemaAtual;

    public string SaidaCombinada
    {
        get
        {
            var parts = new List<string>();
            if (!string.IsNullOrWhiteSpace(ConfiguracaoVm.Saida)) parts.Add(ConfiguracaoVm.Saida);
            if (!string.IsNullOrWhiteSpace(MetodosVm.Saida)) parts.Add(MetodosVm.Saida);
            return string.Join("\n", parts);
        }
    }

    public string NomeArquivo { get => _nomeArquivo; set => SetField(ref _nomeArquivo, value); }
    public string InfoArquivo { get => _infoArquivo; set => SetField(ref _infoArquivo, value); }
    public string ConteudoHex { get => _conteudoHex; set => SetField(ref _conteudoHex, value); }
    public string BitsPorByte { get => _bitsPorByte; set => SetField(ref _bitsPorByte, value); }
    public string FrequenciaInfo { get => _frequenciaInfo; set => SetField(ref _frequenciaInfo, value); }
    public string PosicaoInfo { get => _posicaoInfo; set => SetField(ref _posicaoInfo, value); }
    public string StatusRodape { get => _statusRodape; set => SetField(ref _statusRodape, value); }

    public MainViewModel()
    {
        ConfiguracaoVm = new ConfiguracaoViewModel();
        MetodosVm = new MetodosViewModel(ConfiguracaoVm);
        MostrarSobreCommand = new RelayCommand(_ => AbrirJanela<SobreWindow>());
        MostrarAlgoritmosGeneticosCommand = new RelayCommand(_ =>
            MessageBox.Show("Módulo em Desenvolvimento", "Algoritmos Genéticos"));

        ConfiguracaoVm.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName == nameof(ConfiguracaoViewModel.Saida)) OnPropertyChanged(nameof(SaidaCombinada));
        };
        MetodosVm.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName == nameof(MetodosViewModel.Saida)) OnPropertyChanged(nameof(SaidaCombinada));
        };
    }

    public double CalcularEntropiaGlobal(byte[] dados)
    {
        return EntropiaShannon.BitsPorByte(dados.AsSpan());
    }

    private static void AbrirJanela<T>() where T : Window, new()
    {
        var w = new T();
        w.Owner = Application.Current.MainWindow;
        w.ShowDialog();
    }
}