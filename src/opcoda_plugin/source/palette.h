#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

// **tokens de duas chapas.** o instrumento tem dois temas: DARK, a chapa de
// grafite em que o sinal e' branco, e CLASSIC, a chapa creme com laranja que o
// instrumento usou ate' 13/11/2026. os dois partilham os mesmos nomes: quem
// pinta pede `palette::accent` e nao sabe em que tema esta'.
//
// **por referencias e nao por constantes.** os tokens eram `inline const` e o
// tema era uma decisao de compilacao. passa a haver um `Tokens` activo e cada
// nome e' uma referencia para o membro — o call site nao muda, e trocar de tema
// e' uma atribuicao. as cores ficam nao-const de proposito: um `Colour` nao
// guarda o seu dono, e o que muda e' o valor para onde a referencia aponta.
//
// o tema escolhe-se em SETTINGS > THEMES e fica guardado em
// `%APPDATA%/Opcoda/theme.txt`, por isso o instrumento abre sempre como foi
// deixado. um tema novo entra aqui como mais uma funcao `Tokens ...Tokens()`.
namespace opcoda::palette {

enum class Theme { dark, classic };

struct Tokens {
    juce::Colour chassis, chassisDark, chassisBorder, header, faceplate, faceplateLight,
        faceplateDark, charcoal, charcoalDeep, panelBg, subPanel, valBox;
    juce::Colour knobTop, knobBottom, knobEdge, brass, rust, sage, ivory;
    juce::Colour display, displayPanel, displayBorder, displayGrid;
    juce::Colour textDark, textSub, textMuted, textField, textOnDark, textOnDarkSub;
    juce::Colour focusRing;
    juce::Colour accent, accentSoft, accentWarn, ok, okBright, error, errorLight;
    juce::Colour waveLow, waveHigh;
    juce::Colour meterBed, meterGreen, meterAmber, meterRed;
    juce::Colour rail;
};

// -- o tema escuro, monocromatico --
// **um so' material e uma so' tinta.** nao ha' matiz em lado nenhum: o que era
// verde-agua passou a branco, e o que era aviso ou erro distingue-se por
// luminosidade (branco no tecto) e nunca por cor. o texto e a forma dizem o
// estado; a cor so' diz o quanto.
inline Tokens darkTokens() {
    Tokens t {};
    t.chassis = juce::Colour {0xff0e0e0e};
    t.chassisDark = juce::Colour {0xff070707};
    t.chassisBorder = juce::Colour {0xff2c2c2c};
    t.header = juce::Colour {0xff151515};
    t.faceplate = juce::Colour {0xff171717};
    t.faceplateLight = juce::Colour {0xff1e1e1e};
    t.faceplateDark = juce::Colour {0xff0b0b0b};
    t.charcoal = juce::Colour {0xff202020};
    t.charcoalDeep = juce::Colour {0xff080808};
    t.panelBg = juce::Colour {0xff1c1c1c};
    t.subPanel = juce::Colour {0xff232323};
    t.valBox = juce::Colour {0xff2a2a2a};

    t.knobTop = juce::Colour {0xff3a3a3a};
    t.knobBottom = juce::Colour {0xff111111};
    t.knobEdge = juce::Colour {0xff5c5c5c};
    t.brass = juce::Colour {0xff9a9a9a};
    // o arco do valor e' a leitura mais forte do painel: branco, um passo abaixo
    // do marfim para o ponteiro continuar a ser o ponto mais claro.
    t.rust = juce::Colour {0xffededed};
    t.sage = juce::Colour {0xff7a7a7a};
    t.ivory = juce::Colour {0xfff4f4f4};

    t.display = juce::Colour {0xff000000};
    t.displayPanel = juce::Colour {0xff050505};
    t.displayBorder = juce::Colour {0xff262626};
    t.displayGrid = juce::Colour {0x16ffffff};

    t.textDark = juce::Colour {0xfff1f1f1};
    t.textSub = juce::Colour {0xffb8b8b8};
    t.textMuted = juce::Colour {0xff8a8a8a};
    t.textField = juce::Colour {0xffd6d6d6};
    t.textOnDark = juce::Colour {0xffededed};
    t.textOnDarkSub = juce::Colour {0xffa6a6a6};

    t.focusRing = juce::Colour {0xffe6e6e6};

    t.accent = juce::Colour {0xffe8e8e8};
    t.accentSoft = juce::Colour {0xfff7f7f7};
    t.accentWarn = juce::Colour {0xffffffff};
    t.ok = juce::Colour {0xffbfbfbf};
    t.okBright = juce::Colour {0xfff2f2f2};
    t.error = juce::Colour {0xffffffff};
    t.errorLight = juce::Colour {0xffffffff};

    t.waveLow = juce::Colour {0xff8c8c8c};
    t.waveHigh = juce::Colour {0xffe8e8e8};

    t.meterBed = juce::Colour {0xff080808};
    t.meterGreen = juce::Colour {0xff8a8a8a};
    t.meterAmber = juce::Colour {0xffc4c4c4};
    t.meterRed = juce::Colour {0xffffffff};

    t.rail = juce::Colour {0xff121212};
    return t;
}

// -- o tema classico, de 2025 a 13/11/2026 --
// chapa creme, carvao nas janelas e laranja queimado como sinal. fica inteiro,
// valor por valor, para o instrumento poder voltar a' cara com que foi desenhado
// sem ninguem reescrever a paleta de memoria.
inline Tokens classicTokens() {
    Tokens t {};
    t.chassis = juce::Colour {0xff171512};
    t.chassisDark = juce::Colour {0xff0d0c0a};
    t.chassisBorder = juce::Colour {0xff6b5f4c};
    t.header = juce::Colour {0xffd6c9ae};
    t.faceplate = juce::Colour {0xffd6c9ae};
    t.faceplateLight = juce::Colour {0xffe4d9c3};
    t.faceplateDark = juce::Colour {0xff9e8e73};
    t.charcoal = juce::Colour {0xff211d18};
    t.charcoalDeep = juce::Colour {0xff100e0c};
    t.panelBg = juce::Colour {0xff26231e};
    t.subPanel = juce::Colour {0xff302c25};
    t.valBox = juce::Colour {0xff38332b};

    t.knobTop = juce::Colour {0xff3b352c};
    t.knobBottom = juce::Colour {0xff161310};
    t.knobEdge = juce::Colour {0xff7d7059};
    t.brass = juce::Colour {0xffc09552};
    t.rust = juce::Colour {0xffe2661c};
    t.sage = juce::Colour {0xff7e9a63};
    t.ivory = juce::Colour {0xffeee4cf};

    t.display = juce::Colour {0xff000000};
    t.displayPanel = juce::Colour {0xff070706};
    t.displayBorder = juce::Colour {0xff2b251d};
    t.displayGrid = juce::Colour {0x14ffd9a0};

    t.textDark = juce::Colour {0xff1c1913};
    t.textSub = juce::Colour {0xff4a4237};
    t.textMuted = juce::Colour {0xff6c6353};
    t.textField = juce::Colour {0xff453f35};
    t.textOnDark = juce::Colour {0xffe9dfc9};
    t.textOnDarkSub = juce::Colour {0xffb3a794};

    t.focusRing = juce::Colour {0xfff2b34d};

    t.accent = juce::Colour {0xfff2740d};
    t.accentSoft = juce::Colour {0xffffc061};
    t.accentWarn = juce::Colour {0xffe3a33d};
    t.ok = juce::Colour {0xff7f9a64};
    t.okBright = juce::Colour {0xffb9d18a};
    t.error = juce::Colour {0xffc05a43};
    t.errorLight = juce::Colour {0xffd3705a};

    t.waveLow = juce::Colour {0xffa9c09a};
    t.waveHigh = juce::Colour {0xffe0703a};

    t.meterBed = juce::Colour {0xff080706};
    t.meterGreen = juce::Colour {0xff6fae5c};
    t.meterAmber = juce::Colour {0xffd9a63c};
    t.meterRed = juce::Colour {0xffd4563a};

    t.rail = juce::Colour {0xff1d1a16};
    return t;
}

// o tema activo e o anterior: o anterior e' o mapa de tradução para as cores que
// os componentes guardaram em `setColour` antes da troca (ver applyToTree).
inline Tokens active {darkTokens()};
inline Tokens previous {darkTokens()};
inline Theme currentTheme {Theme::dark};

// um nome por token, todos referencias para o tema activo. quem pinta nao sabe
// em que tema esta' e nao precisa de saber.
inline juce::Colour& chassis = active.chassis;
inline juce::Colour& chassisDark = active.chassisDark;
inline juce::Colour& chassisBorder = active.chassisBorder;
inline juce::Colour& header = active.header;
inline juce::Colour& faceplate = active.faceplate;
inline juce::Colour& faceplateLight = active.faceplateLight;
inline juce::Colour& faceplateDark = active.faceplateDark;
inline juce::Colour& charcoal = active.charcoal;
inline juce::Colour& charcoalDeep = active.charcoalDeep;
inline juce::Colour& panelBg = active.panelBg;
inline juce::Colour& subPanel = active.subPanel;
inline juce::Colour& valBox = active.valBox;

inline juce::Colour& knobTop = active.knobTop;
inline juce::Colour& knobBottom = active.knobBottom;
inline juce::Colour& knobEdge = active.knobEdge;
inline juce::Colour& brass = active.brass;
inline juce::Colour& rust = active.rust;
inline juce::Colour& sage = active.sage;
inline juce::Colour& ivory = active.ivory;

inline juce::Colour& display = active.display;
inline juce::Colour& displayPanel = active.displayPanel;
inline juce::Colour& displayBorder = active.displayBorder;
inline juce::Colour& displayGrid = active.displayGrid;

inline juce::Colour& textDark = active.textDark;
inline juce::Colour& textSub = active.textSub;
inline juce::Colour& textMuted = active.textMuted;
inline juce::Colour& textField = active.textField;
inline juce::Colour& textOnDark = active.textOnDark;
inline juce::Colour& textOnDarkSub = active.textOnDarkSub;

inline juce::Colour& focusRing = active.focusRing;

inline juce::Colour& accent = active.accent;
inline juce::Colour& accentSoft = active.accentSoft;
inline juce::Colour& accentWarn = active.accentWarn;
inline juce::Colour& ok = active.ok;
inline juce::Colour& okBright = active.okBright;
inline juce::Colour& error = active.error;
inline juce::Colour& errorLight = active.errorLight;

inline juce::Colour& waveLow = active.waveLow;
inline juce::Colour& waveHigh = active.waveHigh;

inline juce::Colour& meterBed = active.meterBed;
inline juce::Colour& meterGreen = active.meterGreen;
inline juce::Colour& meterAmber = active.meterAmber;
inline juce::Colour& meterRed = active.meterRed;

inline juce::Colour& rail = active.rail;

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

// troca de tema. guarda o anterior antes de escrever, para applyToTree poder
// traduzir as cores que os componentes ja' tinham guardado, e avisa quem ouve
// (o editor) para se redesenhar.
void setTheme(Theme theme);

// o que ja' esta' guardado em %APPDATA%/Opcoda/theme.txt, ou DARK se o ficheiro
// nao existir ou estiver estragado.
Theme loadSavedTheme();
void saveTheme(Theme theme);

// a cor antiga do mesmo papel: procura `colour` na tabela anterior e devolve o
// valor correspondente na activa. o que nao for um token fica como esta'.
juce::Colour translate(juce::Colour colour);

// percorre a arvore e traduz as cores fixadas por setColour. sem isto, um tema
// trocado a meio da sessao deixava etiquetas e combos com a tinta do tema
// anterior. os componentes que pintam a partir da paleta nao precisam disto:
// leem o valor novo no proximo paint.
void applyToTree(juce::Component& root);

} // namespace opcoda::palette
