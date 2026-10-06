using System.Windows;

namespace OpcodaOtimizacao.Views;

public partial class SobreWindow : Window
{
    public SobreWindow()
    {
        InitializeComponent();
        DataContext = new
        {
            Disciplina = "Disciplina: " + DadosAcademicos.Disciplina,
            Docente = "Docente: " + DadosAcademicos.Docente,
            Instituicao = "Instituição: " + DadosAcademicos.Instituicao,
            Semestre = "Semestre: " + DadosAcademicos.Semestre,
            Discentes = DadosAcademicos.Discentes,
        };
    }
}