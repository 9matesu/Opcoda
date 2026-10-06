using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using Microsoft.Win32;
using OpcodaOtimizacao.ViewModels;

namespace OpcodaOtimizacao.Views;

public partial class MainWindow : Window
{
    private readonly MainViewModel _vm;

    public MainWindow()
    {
        InitializeComponent();
        _vm = new MainViewModel();
        DataContext = _vm;
    }

    private void OnLoadClick(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFileDialog { Filter = "Executáveis (*.exe)|*.exe|Todos os arquivos (*.*)|*.*" };
        if (dlg.ShowDialog(this) != true) return;

        _vm.NomeArquivo = Path.GetFileName(dlg.FileName);
        var info = new FileInfo(dlg.FileName);
        _vm.InfoArquivo = $"PE | {(info.Length / 1024.0):F1} KB";

        var dados = File.ReadAllBytes(dlg.FileName);
        int maxBytes = Math.Min(dados.Length, 4096);
        _vm.ConteudoHex = FormatarHex(dados.AsSpan(0, maxBytes));
        _vm.BitsPorByte = $"Entropia: {_vm.CalcularEntropiaGlobal(dados):F3} bits/byte";
        _vm.StatusRodape = $"Arquivo carregado: {info.Name}";
    }

    private void OnOptionsClick(object sender, RoutedEventArgs e)
    {
        if (sender is not Button btn) return;
        var popup = FindName("OptionsPopup") as Popup;
        if (popup == null) return;
        popup.PlacementTarget = btn;
        popup.Placement = System.Windows.Controls.Primitives.PlacementMode.Bottom;
        popup.IsOpen = !popup.IsOpen;
    }

    private void OnMenuSobreClick(object sender, RoutedEventArgs e)
    {
        if (FindName("OptionsPopup") is Popup p) p.IsOpen = false;
        _vm.MostrarSobreCommand.Execute(null);
    }

    private void OnMenuAGClick(object sender, RoutedEventArgs e)
    {
        if (FindName("OptionsPopup") is Popup p) p.IsOpen = false;
        _vm.MostrarAlgoritmosGeneticosCommand.Execute(null);
    }

    private static string FormatarHex(ReadOnlySpan<byte> data)
    {
        var sb = new StringBuilder();
        for (int off = 0; off < data.Length; off += 16)
        {
            int len = Math.Min(16, data.Length - off);
            sb.Append($"0x{off:X8}  ");
            var ascii = new StringBuilder();
            for (int i = 0; i < 16; i++)
            {
                if (i < len)
                {
                    sb.Append($"{data[off + i]:X2} ");
                    char c = (char)data[off + i];
                    ascii.Append(c is >= ' ' and <= '~' ? c : '.');
                }
                else
                {
                    sb.Append("   ");
                }
                if (i == 7) sb.Append(' ');
            }
            sb.Append($" {ascii}");
            sb.AppendLine();
        }
        return sb.ToString();
    }
}