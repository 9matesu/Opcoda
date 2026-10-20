#include "plugin_editor.h"

#include "assets.h"
#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <array>

namespace opcoda {
namespace {

constexpr int kHeaderHeight {64};

// O display passou de 130-210 px para 200-460 px. Uma grelha com endereco,
// dezasseis colunas e ASCII precisa de 14 px de cabecalho mais seis linhas de
// 24 px, e a faixa de 14 px do estado e os 16 px do rodape. O display absorve
// o espaco livre entre o header e a faixa de macros, por isso o teto alto: com
// ele a grelha mostra uma linha e meia a mais por cada degrau de altura da
// janela, em vez de deixar faixa escura vazia.
constexpr int kMinDisplayHeight {200};
constexpr int kMaxDisplayHeight {460};

// Janela minima. Header, display, faixa de macro knobs e rodape de saida:
// 560 px de altura comportam os quatro com a grelha nas seis linhas.
constexpr int kMinWidth {720};
constexpr int kMinHeight {560};

constexpr int kAddressFieldWidth {132};
constexpr int kSnapButtonWidth {72};
constexpr int kStatusHeight {18};
constexpr int kFooterHeight {18};

// Alturas das faixas de cima para baixo, com piso e teto para caberem os
// conteudos: macro (titulo + knob 64 + valor) e rodape (LED strip de saida).
// A faixa de macros subiu 8 px para o knob de 64 px do mock caber com titulo e
// valor; o display absorve a diferenca, porque e' ele que tem folga.
constexpr int kMacroBandHeight {120};
constexpr int kOutputBandHeight {30};

// Os seis parametros da Tabela 8, e nada mais. Nada aqui e' cena: um comando
// sem nucleo atraz nao vira botao.
//
// **juce::String {valor, 0} nao arredonda.** Em juce_String.cpp, writeDouble so
// aplica a precisao ao stream quando numDecPlaces > 0; com zero o `o << n` sai
// com a precisao por omissao, que e' 6 digitos significativos. O resultado e'
// "28.756" onde se queria "29", e Nobody reparava porque 28.756 e' um valor
// plausivel de densidade.
//
// O JUCE nao tem construtor de String a partir de float arredondado, portanto o
// arredondamento tem de ser explicito. Helper em vez de repetir roundToInt nas
// seis lambdas, para a razao ficar escrita uma vez.
juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

// Cores das colunas do mock. Sao a unica decoracao do painel: nao mapeiam
// estado nem parametro, so' identificam a coluna, e por isso nao substituem o
// nome escrito ao lado.
const std::array<juce::Colour, 6> kSwatches {
    juce::Colour {0xffb91c1c}.withAlpha(0.85f), // vermelho
    juce::Colour {0xfff59e0b},                  // ambar
    juce::Colour {0xff84cc16},                  // lima
    juce::Colour {0xff3b82f6},                  // azul
    juce::Colour {0xff14b8a6},                  // turquesa
    juce::Colour {0xffa855f7},                  // purpura
};

const std::array<Knob::Spec, 6> kSpecs {
    Knob::Spec {"grain", "SIZE", "Tamanho de grao, em milissegundos", 40.0f,
                [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {"density", "DENSITY", "Densidade, em graos por segundo", 20.0f,
                [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {"position", "POSITION", "Posicao de leitura no material", 0.5f,
                [](float v) { return asInteger(v * 100.0f) + " %"; }},
    Knob::Spec {"spray", "SPRAY", "Spray, dispersao da posicao de inicio", 0.0f,
                [](float v) { return asInteger(v * 100.0f) + " %"; }},
    // Estas duas ja estavam certas: com uma casa decimal o writeDouble entra no
    // if e formata em fixo.
    Knob::Spec {"pitch", "PITCH", "Afinacao, em semitons", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"volume", "VOLUME", "Volume, em decibels", 0.0f,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
};

} // namespace

// Trocar o texto de um Label dispara um evento de acessibilidade. A 20 Hz isso
// vira ruido para quem usa leitor de tela, entao so se escreve quando muda.
void setIfChanged(juce::Label& label, const juce::String& text) {
    if (label.getText() != text) {
        label.setText(text, juce::dontSendNotification);
        label.setName(text);
    }
}

PluginEditor::PluginEditor(PluginProcessor& processor)
    : juce::AudioProcessorEditor(&processor), owner_(processor) {
    setLookAndFeel(&lookAndFeel_);

    makePlainLabel(title_, "OPCODA", palette::textDark, assets::fonts::tech(26.0f));
    makePlainLabel(subtitle_, "PE GRANULAR SYNTHESIZER", palette::subtitleOnMetal,
                   BoxedLabel::sansFont(10.0f, true).withExtraKerningFactor(0.12f));
    makePlainLabel(dropHint_, "DRAG & DROP BINARY", palette::textField,
                   BoxedLabel::monoFont(9.0f));
    makePlainLabel(voices_, "VOICES 00 / 08", palette::textSub,
                   BoxedLabel::monoFont(10.0f, true));
    makePlainLabel(status_, "", palette::textOnDark, BoxedLabel::monoFont(10.0f));
    makePlainLabel(outputLabel_, "OUTPUT", palette::textOnDarkSub,
                   BoxedLabel::monoFont(10.0f, true));

    fileName_.setText("-", juce::dontSendNotification);
    fileName_.setJustificationType(juce::Justification::centredLeft);
    fileName_.setBorderSize(juce::BorderSize<int> {0, 7, 0, 5});
    fileName_.setFont(BoxedLabel::monoFont(11.0f, true));
    fileName_.setColour(juce::Label::textColourId, palette::amberText);

    formatTag_.setFont(BoxedLabel::monoFont(9.0f));
    formatTag_.setColour(juce::Label::textColourId, palette::chipText);

    fileSize_.setFont(BoxedLabel::monoFont(10.0f));
    fileSize_.setColour(juce::Label::textColourId, palette::chipText);

    buildHeader();
    buildMacroBand();
    buildFooterBand();

    // A thread de interface e' a dona da memoria de amostra, entao e' ela que
    // libera o que a thread de audio ja devolveu. 20 Hz nao competem com a
    // audio e bastam para a leitura parecer viva.
    startTimerHz(20);

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, 4096, 4096);
    setSize(1040, 690);
    refresh();
}

void PluginEditor::makePlainLabel(juce::Label& label,
                                  const juce::String& text,
                                  juce::Colour colour,
                                  juce::Font font) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, colour);
    label.setFont(font);
    label.setInterceptsMouseClicks(false, false);
}

void PluginEditor::buildHeader() {
    for (auto* component : {static_cast<juce::Component*>(&powerLed_),
                            static_cast<juce::Component*>(&title_),
                            static_cast<juce::Component*>(&subtitle_),
                            static_cast<juce::Component*>(&dropHint_),
                            static_cast<juce::Component*>(&voices_),
                            static_cast<juce::Component*>(&loadButton_),
                            static_cast<juce::Component*>(&fileName_),
                            static_cast<juce::Component*>(&formatTag_),
                            static_cast<juce::Component*>(&fileSize_),
                            static_cast<juce::Component*>(&display_),
                            static_cast<juce::Component*>(&statusLed_),
                            static_cast<juce::Component*>(&status_)}) {
        addAndMakeVisible(*component);
    }

    for (auto* component : {static_cast<juce::Component*>(&grid_),
                            static_cast<juce::Component*>(&address_),
                            static_cast<juce::Component*>(&snapButton_),
                            static_cast<juce::Component*>(&voicesLed_)}) {
        addAndMakeVisible(*component);
    }

    // A grelha escreve o POSITION. O duplo clique e o botao alternam entre a
    // regiao exacta e a secao PE mais proxima; o metodo do Processor e' quem
    // decide o sentido da alternancia, porque e' ele quem guarda a regiao
    // exacta anterior.
    grid_.onCellActivated = [this](std::uint64_t address) { activateByte(address); };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
    address_.onAddressEntered = [this](std::uint64_t address) { moveRegionTo(address); };

    snapButton_.setButtonText("ALINHAR");
    snapButton_.setTooltip(
        "Alterna entre a regiao exacta e a secao PE mais proxima. Substitui a "
        "tecla Enter que o seletor de bytes usava para o mesmo.");
    snapButton_.setName("Alinhar a regiao a uma secao PE");
    snapButton_.onClick = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };

    for (auto* readout : {&entropyReadout_, &positionReadout_, &offsetReadout_, &peakReadout_,
                          &rateReadout_, &voicesReadout_}) {
        addAndMakeVisible(*readout);
        readout->setInterceptsMouseClicks(false, false);
        // A cor de texto por omissao do Label e' quase preta e desaparece sobre
        // o display escuro. textOnDark mantem a leitura acima de 4,5:1, que e'
        // o que o criterio 1.4.3 exige. O rodape vive no display, nao no chassis
        // claro, e por isso nao pode usar os tokens de superficie clara.
        readout->setColour(juce::Label::textColourId, palette::textOnDark);
    }

    // Tinta por leitura, como no mock: entropia em ambar, taxa em emerald, pico
    // em red-400 e o POS em ciano. Tudo continua em texto, nunca so em cor.
    entropyReadout_.setColour(juce::Label::textColourId, palette::accentSoft);
    rateReadout_.setColour(juce::Label::textColourId, palette::ok);
    peakReadout_.setColour(juce::Label::textColourId, palette::peakRed);

    // O POS e' o unico chip do rodape, como o "POS: 0x..." do mock. A fonte
    // bold separa a leitura do endereco das restantes leituras.
    positionReadout_.setFont(BoxedLabel::monoFont(10.0f, true));
    positionReadout_.setColour(juce::Label::textColourId, palette::oledCyan);

    loadButton_.setButtonText("LOAD");
    loadButton_.setTooltip("Escolher um binario para sintetizar");
    loadButton_.setName("Carregar binario");
    loadButton_.setColour(juce::TextButton::buttonColourId, palette::metalButton);
    loadButton_.setColour(juce::TextButton::textColourOffId, palette::amberText);
    loadButton_.setColour(juce::TextButton::textColourOnId, palette::amberText);
    loadButton_.onClick = [this] { chooseFile(); };

    // O ALINHAR vive dentro do display e por isso e' cinza, nao ambar: dois
    // botoes ambar em pontos distantes do painel leem-se como a mesma acao.
    snapButton_.setColour(juce::TextButton::buttonColourId, juce::Colour {0xff1a2030});
    snapButton_.setColour(juce::TextButton::textColourOffId, palette::chipText);
    snapButton_.setColour(juce::TextButton::textColourOnId, palette::chipText);
}

// Um ponto so para a visibilidade do header, porque ha duas razoes independentes
// para uma peca nao estar la e nenhum dos lados pode prevailecer sobre o outro.
//
// Chamar isto de resized() e de refresh() e' o que mantem a verdade num sitio so:
// resized() sabe a largura, refresh() sabe o conteudo, e cada um escreve a sua
// metade. Se cada um chamasse setVisible por si, o timer de 20 Hz do refresh()
// voltava a mostrar um chip que nao cabe, e a sobreposicao voltava a aparecer.
void PluginEditor::updateHeaderVisibility() {
    const auto& info = owner_.sourceInfo();

    // Duas condicoes, e ambas tem de ser verdade. A peca e' escondida quando nao
    // ha espaco OU quando nao ha conteudo — e nunca se desenha uma moldura vazia,
    // que e' o mesmo motivo pelo qual os chips ja se escondiam por vazio.
    // Sem material o chip mostra a dica de arrasto em vez de um "-" solto: o
    // "-" nao ensina nada e a dica ocupa o mesmo lugar.
    const bool hasSource = owner_.hasSource();
    fileName_.setVisible(hasSource);
    dropHint_.setVisible(headerFitsHint_ && !hasSource);
    fileSize_.setVisible(headerFitsSize_ && info.sizeBytes > 0);
    formatTag_.setVisible(headerFitsFormat_ && info.formatTag.isNotEmpty());
}

void PluginEditor::updateFooterVisibility() {
    // Mesma regra do header: a largura decide se a caixa existe, e o conteudo
    // decide se ha algo para escrever. Uma caixa a mais pequena e' pior do que
    // nenhuma, porque o texto sai cortado e nao se percebe o que e'.
    rateReadout_.setVisible(footerFitsRate_);
    peakReadout_.setVisible(footerFitsPeak_);
    entropyReadout_.setVisible(footerFitsEntropy_ && entropyReadout_.getText().isNotEmpty());
}

void PluginEditor::buildMacroBand() {
    // Os seis knobs, cada um ligado ao seu parametro real; nada aqui e'
    // decoracao, o nucleo e' quem responde ao rato.
    for (std::size_t i = 0; i < kSpecs.size(); ++i) {
        knobs_.push_back(std::make_unique<Knob>(owner_.parameters(), kSpecs[i]));
        knobs_.back()->setSwatch(kSwatches[i]);
        addAndMakeVisible(*knobs_.back());
    }
}

void PluginEditor::buildFooterBand() {
    addAndMakeVisible(footerBand_);
    addAndMakeVisible(vuMeter_);
    addAndMakeVisible(outputLabel_);
}

void PluginEditor::paint(juce::Graphics& g) {
    // Corpo escuro do frame. As duas faixas de metal (header e painel de macros)
    // sao pintadas por cima; o display e' um filho opaco que cobre o propio
    // intervalo no meio.
    const auto bounds = getLocalBounds();
    g.fillAll(palette::chassis);

    // Faixa do header: gradiente de metal escovado do mock, com a borda de 1 px
    // que separa as tres faixas.
    const auto header = bounds.withHeight(kHeaderHeight);
    juce::ColourGradient headerMetal(palette::metalTop,
                                     static_cast<float>(header.getX()),
                                     static_cast<float>(header.getY()) - 0.5f,
                                     palette::metalBottom,
                                     static_cast<float>(header.getX()),
                                     static_cast<float>(header.getY()) + static_cast<float>(kHeaderHeight) - 0.5f,
                                     false);
    headerMetal.addColour(0.5, palette::metalMiddle);
    g.setGradientFill(headerMetal);
    g.fillRect(header);

    g.setColour(palette::metalBorder);
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(getWidth()));

    // Painel de macros tambem e' metal no mock: pintar o metal da base do
    // header para baixo cobre header e rodape, e as duas faixas — display e
    // rodape de saida — sao componentes opacos que recolhem a fatia de cada
    // uma.
    juce::ColourGradient rackMetal(palette::metalTop,
                                   static_cast<float>(bounds.getX()),
                                   static_cast<float>(kHeaderHeight) - 0.5f,
                                   palette::metalBottom,
                                   static_cast<float>(bounds.getX()),
                                   static_cast<float>(bounds.getHeight()) - 0.5f, false);
    rackMetal.addColour(0.5, palette::metalMiddle);
    g.setGradientFill(rackMetal);
    g.fillRect(bounds.withTop(kHeaderHeight));

    // Realce de 1 px no topo: e' o inset 0 1px 0 rgba(255,255,255,0.8) do mock.
    // E' o que da sensacao de superficie em vez de retangulo liso.
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // A moldura e os cantos ficam em paintOverChildren: os filhos opacos
    // (display e faixas) cobrem o retangulo inteiro, e desenhar a borda aqui
    // deixava-a por baixo deles.
}

// Cantos de 8 px do frame (rounded-lg do mock). Nao ha recorte de verdade: o
// JUCE nao recorta filhos por um Path, por isso mascara-se por cima. O
// FillType odd-even pinta so' a diferenca entre o retangulo e o retangulo
// arredondado, que sao os quatro triangulos de fora do canto.
void PluginEditor::paintOverChildren(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    constexpr float radius {8.0f};

    juce::Path mask;
    mask.addRectangle(bounds);
    mask.addRoundedRectangle(bounds, radius);
    // even-odd: o contorno interior (o retangulo arredondado) fura o exterior,
    // e so' os quatro cantos ficam preenchidos.
    mask.setUsingNonZeroWinding(false);
    g.setColour(juce::Colour {0xff0b0c0e});
    g.fillPath(mask);

    g.setColour(palette::frameBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

void PluginEditor::resized() {
    auto area = getLocalBounds();

    // ---- header ----
    // Cursor explicito em vez de encadear removeFromLeft: aquele metodo devolve
    // um retangulo novo e NAO consome o original, entao encadear sobrepoe as
// pecas em vez de avanca-las.
    const auto header = area.removeFromTop(kHeaderHeight).reduced(16, 0);
    int x = header.getX();

    // As medidas do header em constantes com nome. O layout e' uma cadeia de
    // larguras que tem de fechar, e numeros soltos nao se somam a olho.
    constexpr int kIdentityWidth {214};
    constexpr int kIdentityGap {16};
    constexpr int kLedGap {8};
    constexpr int kLoadWidth {64};
    constexpr int kMinNameWidth {96};
    constexpr int kChipGap {4};
    constexpr int kFormatWidth {88};
    constexpr int kSizeWidth {62};
    constexpr int kStripGap {10};
    constexpr int kVoicesWidth {92};
    constexpr int kChipHeight {28};
    const auto chipTop = header.getY() + 18;

    // Wordmark do mock: Chakra Petch e o ponto verde logo a seguir. A posicao do
    // ponto e' medida pela largura real do texto, e nao por um offset fixo: com
    // outra fonte o ponto cairia por cima do "A" ou a meio metro do logo.
    title_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, kIdentityWidth, 26});
    const auto wordWidth =
        juce::GlyphArrangement::getStringWidth(assets::fonts::tech(26.0f), "OPCODA");
    powerLed_.setBounds(
        juce::Rectangle<int> {x + juce::roundToInt(wordWidth) + kLedGap, header.getY() + 10, 14,
                              26}
            .withSizeKeepingCentre(12, 12));
    subtitle_.setBounds(juce::Rectangle<int> {x, header.getY() + 38, kIdentityWidth, 12});
    x += kIdentityWidth + kIdentityGap;

    // Lado direito: VOICES colado a borda e sempre visivel. E' leitura de estado,
    // e leitura de estado nao se esconde por falta de espaco.
    voices_.setBounds(juce::Rectangle<int> {header.getRight() - kVoicesWidth, header.getY(),
                                             kVoicesWidth, kHeaderHeight}
                          .withSizeKeepingCentre(kVoicesWidth, 16));

    // O LOAD fica reservado a direita, antes de VOICES. O chip do ficheiro
    // encosta a esquerda e tem largura propria: esticado ate' ao LOAD virava
    // uma barra escura de 600 px com o nome encostado a uma ponta, que nao e'
    // o chip compacto do mock.
    const int loadX = header.getRight() - kVoicesWidth - kStripGap - kLoadWidth;
    loadButton_.setBounds(juce::Rectangle<int> {loadX, chipTop, kLoadWidth, kChipHeight});

    const int nameRight = juce::jmin(loadX - kStripGap, x + 380);
    const auto nameBounds =
        juce::Rectangle<int> {x, chipTop, juce::jmax(kMinNameWidth, nameRight - x), kChipHeight};
    fileName_.setBounds(nameBounds);
    dropHint_.setBounds(nameBounds);
    headerFitsHint_ = nameBounds.getWidth() >= 140;

    // Tags dentro do mesmo chip, encostadas a direita, como no mock. Sao
    // desenhadas depois do fileName_ (a ordem de buildHeader), por isso ficam
    // por cima do fundo escuro sem precisar de um container.
    const int tagsWidth = kSizeWidth + kChipGap + kFormatWidth;
    headerFitsSize_ = nameBounds.getWidth() >= kMinNameWidth + tagsWidth + 12;
    headerFitsFormat_ = nameBounds.getWidth() >= kMinNameWidth + tagsWidth + 12;

    // Ordem dentro do chip: nome, [formato], tamanho, como no mock.
    int tagRight = nameBounds.getRight() - 6;
    if (headerFitsSize_) {
        fileSize_.setBounds(juce::Rectangle<int> {tagRight - kSizeWidth, chipTop + 6, kSizeWidth,
                                                  kChipHeight - 12});
        tagRight -= kSizeWidth + kChipGap;
    }
    if (headerFitsFormat_) {
        formatTag_.setBounds(juce::Rectangle<int> {tagRight - kFormatWidth, chipTop + 6,
                                                   kFormatWidth, kChipHeight - 12});
    }

    // O texto do nome tem de parar antes das tags; sem isto um nome comprido
    // desenhava por baixo do "[64-bit PE]".
    const int textRightBorder = (headerFitsSize_ || headerFitsFormat_) ? tagsWidth + 12 : 5;
    fileName_.setBorderSize(juce::BorderSize<int> {0, 7, 0, textRightBorder});
    dropHint_.setBorderSize(juce::BorderSize<int> {0, 7, 0, textRightBorder});

    updateHeaderVisibility();

// ---- display ----
    // O display e' a superficie maior do frame e absorve o espaco livre entre
    // o header e o rodape: o calculo desconta a faixa de macros e a faixa de
    // saida, que tem altura fixa, e clamp no intervalo do conteudo da grelha.
    constexpr int kFixedBands {kMacroBandHeight + kOutputBandHeight};
    const auto displayHeight =
        juce::jlimit(kMinDisplayHeight, kMaxDisplayHeight,
                     getHeight() - kHeaderHeight - kFixedBands);
    const auto displayBounds = area.removeFromTop(displayHeight);
    display_.setBounds(displayBounds);

    // A linha de estado ocupa uma faixa propria no topo do display, e a grelha
    // comeca abaixo dela. Fica no topo porque e' onde o erro tem de aparecer: o
    // criterio 3.3.1 pede identificacao de erro, e um E_BAD_PE no rodape, em mono
    // de 10 px, ao lado do pico e das vozes, e' uma coisa que passa sem ser lida.
    //
    // A faixa e' reservada, e nao sobreposta a texto solto: com o texto a 7 px do
    // topo do display ele ficava dentro da area da grelha, e o fundo opaco da
    // grelha tapava o "PRONTO" e o "RECUSADO". Tambem so uma captura mostra.
    const auto statusY = displayBounds.getY();
    statusLed_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12, statusY, 16,
                                               kStatusHeight}
                             .withSizeKeepingCentre(14, kStatusHeight));

    const auto footerY = displayBounds.getBottom() - kFooterHeight - 2;

    // A altura da grelha e' a diferenca entre o fim da faixa de estado e o
    // inicio do rodape. Passar footerY como altura — que e' uma coordenada
    // absoluta e nao um tamanho — esticava a grelha 54 px para baixo, e a ultima
    // linha de bytes ficava por cima das leituras.
    const auto gridTop = statusY + kStatusHeight + 4;
    grid_.setBounds(juce::Rectangle<int> {displayBounds.getX(), gridTop,
                                          displayBounds.getWidth(), footerY - gridTop});

    // O campo de endereco e o botao de alinhar ficam na linha do estado, a
    // direita. Ao lado do texto e nao no rodape porque sao controles: e' a zona
    // do display que ja tem moldura, e um campo de texto dentro do rodape de
    // leituras seria indistinguivel de uma leitura.
    const int controlY = displayBounds.getY() + 2;
    snapButton_.setBounds(juce::Rectangle<int> {displayBounds.getRight() - 12 - kSnapButtonWidth,
                                                controlY, kSnapButtonWidth, kStatusHeight + 2});
    address_.setBounds(juce::Rectangle<int> {snapButton_.getX() - 6 - kAddressFieldWidth,
                                             controlY, kAddressFieldWidth, kStatusHeight + 2});

    // O status ocupa o que sobra entre o LED e o campo de endereco. E' o texto
    // mais longo do painel (o nome do ficheiro), por isso e' ele que cede espaco
    // para os controles, e nao o contrario.
    const int statusLeft = displayBounds.getX() + 12 + 16 + 8;
    status_.setBounds(juce::Rectangle<int> {statusLeft, statusY,
                                            juce::jmax(0, address_.getX() - statusLeft - 8),
                                            kStatusHeight});

    // Rodape de telemetria, empilhado da direita para a esquerda com largura fixa
    // por caixa. A versao anterior media cada caixa a partir da margem esquerda
    // a cada passo, o que fazia todas ocuparem o mesmo intervalo e se
    // sobreporem: so a ultima pintada aparecia, e o pico, a entropia e o offset
    // ficavam escondidos debaixo das outras.
    //
    // E pela mesma razao que o header, o rodape degrada em vez de espremer: as
    // caixas tem largura fixa, e a 480 nao cabem todas. Sem isso a entropia fica
    // com zero de largura e o REG sai cortado a meio, que e' pior do que nao
    // mostrar nada. A ordem de sacrificio e' a do valor: primeiro a taxa, que e'
    // constante durante a sessao e por isso a menos informativa; depois o pico;
    // a entropia e' a ultima a cair. POS, REG e VOICES nunca saem, porque sao o
    // que o painel existe para mostrar.
    constexpr int footerHeight {18};
    const int margin = displayBounds.getX() + 12;
    const int footerRight = displayBounds.getRight() - 12;

    constexpr int kLedBoxWidth {12};
    constexpr int kBoxGap {8};
    constexpr int kVoicesBoxWidth {84};
    constexpr int kRateWidth {76};
    constexpr int kPeakWidth {80};
    constexpr int kPositionWidth {112};
    constexpr int kRegionWidth {160};
    constexpr int kMinEntropyWidth {100};

    // O que nunca sai: o LED e o numero de vozes, o endereco da cabeca de leitura
    // e o intervalo da regiao. E' o que o painel existe para mostrar, e sao as
    // tres leituras que respondem a pergunta "o que estou a ouvir".
    const int essential = kLedBoxWidth + kBoxGap + kVoicesBoxWidth + kPositionWidth + kRegionWidth;
    const auto room = footerRight - margin - essential;

    // A partir daqui decide-se por ordem de prioridade, e a ordem esta' escrita
    // na cadeia e nao numa frase ao lado: se a entropia fosse decidida primeiro e
    // a taxa por ultimo, cada uma veria o que sobra depois das outras e a taxa
    // sobrevivia a expense da entropia — que e' o inverso do que se quer.
    //
    // A entropia da janela da cabeca de leitura e' o dado que justifica o rodape,
    // por isso tem prioridade absoluta. O pico e' util e sai a seguir. A taxa de
    // amostragem e' constante durante a sessao e e' a leitura menos informativa de
    // todas, por isso e' a primeira a cair.
    footerFitsEntropy_ = room >= kMinEntropyWidth;
    footerFitsPeak_ = room - (footerFitsEntropy_ ? kMinEntropyWidth + kBoxGap : 0) >= kPeakWidth;
    footerFitsRate_ = room - (footerFitsEntropy_ ? kMinEntropyWidth + kBoxGap : 0) -
                          (footerFitsPeak_ ? kPeakWidth + kBoxGap : 0) >=
                      kRateWidth;

    // A partir da direita, so com o que cabe. Um salto de kBoxGap entre cada
    // grupo: quando uma caixa e' omitida, nao sobra um intervalo vazio onde
    // nada devia estar.
    int edge = footerRight;
    const auto put = [&edge, footerY](juce::Component& target, int width, bool visible) {
        if (!visible) {
            return;
        }
        target.setBounds(juce::Rectangle<int> {edge - width, footerY + 2, width, footerHeight});
        edge -= width + kBoxGap;
    };

    voicesLed_.setBounds(juce::Rectangle<int> {edge - kLedBoxWidth, footerY + 3, kLedBoxWidth, 12});
    put(voicesReadout_, kVoicesBoxWidth, true);
    put(positionReadout_, kPositionWidth, true);
    put(offsetReadout_, kRegionWidth, true);
    put(rateReadout_, kRateWidth, footerFitsRate_);
    put(peakReadout_, kPeakWidth, footerFitsPeak_);

    // A entropia fecha o grupo pela esquerda, como o primeiro item da faixa de
    // estado do mock. Largura fixa e nao o resto da linha: esticada, o texto
    // ficava centrado a meio do display, longe das outras leituras.
    put(entropyReadout_, kMinEntropyWidth, footerFitsEntropy_);

    updateFooterVisibility();

    // ---- faixa de macros ----
    // Os seis knobs ficam sempre em uma linha, dentro da faixa de metal que o
    // paint() do editor ja pintou (o display opaco recolhe a sua fatia, e o
    // rodape de saida tem faixa opaca propria). Quebrar em duas
    // linhas foi tentado e e' pior: a altura que sobra nao comporta um knob com
    // titulo e valor, e os knobs despencam para poucos pixels. O diametro do
    // knob e' limitado dentro do componente, entao encolhe com a janela sem
    // precisar de um segundo layout.
    const auto macroBand = area.removeFromTop(kMacroBandHeight);
    const auto knobArea = macroBand.reduced(20, 12);

    // gap-3 do mock entre as seis colunas.
    constexpr int kCellGap {12};
    const int cellWidth = (knobArea.getWidth()
                           - kCellGap * (static_cast<int>(knobs_.size()) - 1))
                          / static_cast<int>(knobs_.size());

    // O knob fecha o conteudo (titulo, corpo, valor) e fica centrado na
    // vertical da celula. Preencher a celula inteira deixaria os rotulos
    // flutuando desalinhados, e o mock alinha titulo, corpo e caixa na mesma
    // coluna.
    constexpr int knobContentHeight {12 + 2 + 64 + 2 + 14};
    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        const auto cell = juce::Rectangle<int> {knobArea.getX()
                                                     + (static_cast<int>(i)
                                                        * (cellWidth + kCellGap)),
                                                 knobArea.getY(),
                                                 cellWidth,
                                                 knobArea.getHeight()};
        knobs_[i]->setBounds(cell.withSizeKeepingCentre(
            juce::jmin(cell.getWidth(), 110),
            juce::jmin(knobContentHeight, cell.getHeight())));
    }

    // ---- rodape de saida ----
    // O rodape e' o que sobra da janela a partir daqui. A faixa escura cobre o
    // metal pintado por baixo, e o VU le o pico que a telemetria publica.
    footerBand_.setBounds(area);

    const auto footerCell = area.reduced(16, 4);
    outputLabel_.setBounds(juce::Rectangle<int> {footerCell.getX(), footerCell.getY(), 64,
                                                 footerCell.getHeight()}
                               .withSizeKeepingCentre(64, footerCell.getHeight()));
    vuMeter_.setBounds(juce::Rectangle<int> {footerCell.getRight() - 110, footerCell.getY(),
                                             110, footerCell.getHeight()});
}

void PluginEditor::timerCallback() {
    refresh();
}

void PluginEditor::refresh() {
    // A thread de interface e' a dona da memoria de amostra: so agora o ponteiro
    // devolvido pela thread de audio pode ser liberado com seguranca.
    owner_.releaseReturnedBuffers();

    // Os CCs recebidos sao aplicados aqui, e nao na thread de audio. Ver
    // PluginProcessor::applyPendingControllerChanges para o porque.
    owner_.applyPendingControllerChanges();

    const auto& info = owner_.sourceInfo();


    setIfChanged(fileName_, info.name.isNotEmpty() ? info.name : juce::String {"-"});
    setIfChanged(formatTag_, info.formatTag);
    setIfChanged(fileSize_,
                 info.sizeBytes > 0
                     ? juce::String {info.sizeBytes / (1024.0 * 1024.0), 2} + " MB"
                     : juce::String {});

    // A visibilidade dos chips do header mora em updateHeaderVisibility(), que
    // combina a largura com o conteudo. Escrever setVisible aqui desfazia a
    // regra de largura ao fim de um tique do timer.
    updateHeaderVisibility();

    // Sem ficheiro a grelha mostra "SEM BINARIO" e nao uma moldura vazia. A linha
    // de estado nao muda de sitio com o material a carregar: um alvo que salta
    // quando se carrega um binario e' pior do que um espaco constante.
    grid_.setVisible(owner_.hasSource());
    address_.setVisible(owner_.hasSource());
    snapButton_.setVisible(owner_.hasSource());

    Led::State ledState = Led::State::off;
    juce::String message;

    if (owner_.lastError().isNotEmpty()) {
        ledState = Led::State::fault;
        message = "RECUSADO / " + owner_.lastError();
    } else if (owner_.hasSource()) {
        ledState = Led::State::ready;
        message = "PRONTO / " + info.name;
    } else {
        message = "ARRASTE UM .EXE, .DLL OU .BIN PARA DENTRO DA JANELA";
    }

    setIfChanged(status_, message);
    // RECUSADO em vermelho, PRONTO em ambar e o resto em slate, que e' a leitura
    // da linha de estado do mock. Tres cores, nada de verde/amarelo adjacente
    // sem texto a differenca.
    status_.setColour(juce::Label::textColourId,
                      ledState == Led::State::fault
                          ? palette::error
                          : (ledState == Led::State::ready ? palette::accent
                                                           : palette::textOnDark));

    statusLed_.setState(ledState);
    powerLed_.setState(ledState == Led::State::off ? Led::State::off : Led::State::ready);

    refreshTelemetry(info);
}

// Abas e grelha so sao reenviadas quando o conteudo muda. Reconstruir o desenho
// da grelha a 20 Hz sem necessidade seria trabalho inutil na thread de interface.
void PluginEditor::refreshTelemetry(const PluginProcessor::SourceInfo& info) {
    const auto signature = info.name + juce::String {info.sizeBytes};
    if (loadedSignature_ != signature) {
        loadedSignature_ = signature;

        // O endereco do topo a zero e' decisão do proprio componente: um
        // ficheiro novo recomeca no inicio, senao o segundo abria a meio do
        // primeiro porque a viewport so e' nossa.
        grid_.setSource(&owner_.sourceBytes(), static_cast<std::uint64_t>(info.sizeBytes));
        grid_.setSections(info.sections);
        address_.setHighestAddress(info.sizeBytes > 0
                                       ? static_cast<std::uint64_t>(info.sizeBytes) - 1
                                       : 0);
    }

    const auto range = owner_.byteRange();
    grid_.setRegion(range.start, range.end);

    // O endereco e' o que o Processor tem, e nao uma copia: showAddress nao emite
    // o callback, entao esta linha nao pode gerar um ciclo com o campo.
    address_.showAddress(range.start);

    // A cabeca de leitura vem do parametro. Se o host a moveu por automacao, o
    // display segue o host e nao o contrario.
    const auto position = readPosition();
    const auto readHead = pe::byteForPosition(static_cast<double>(position), range);
    grid_.setReadHead(readHead);

    const auto& telemetry = owner_.telemetry();
    const auto peak = telemetry.peakDb.load(std::memory_order_relaxed);
    const auto activeVoices = telemetry.activeVoices.load(std::memory_order_relaxed);
    const auto sounding = telemetry.sounding.load(std::memory_order_relaxed);

    setIfChanged(entropyReadout_,
                 owner_.hasSource() && !range.empty()
                     ? juce::String {"ENTROPY "}
                           + juce::String {entropyAtReadHead(readHead, range), 2}
                     : juce::String {});

    // A cabeca de leitura em endereco. E' a leitura que responde a pergunta "o
    // que estou a ouvir", e o que o clique na grelha acabou de escolher. O
    // formato "POS: 0x..." e' o do mock; o chip e' o unico do rodape.
    if (owner_.hasSource() && !range.empty()) {
        setIfChanged(positionReadout_,
                     "POS: 0x" + juce::String::formatted(
                                     "%08X", static_cast<unsigned long long>(readHead)));
    } else {
        setIfChanged(positionReadout_, juce::String {});
    }

    // O rodape escreve o intervalo real da regiao, que e' o material que alimenta
    // o motor. E' hex porque o utilizador esta' dentro de um binario, e o offset
    // em hexadecimal e' o que aparece no resto das ferramentas.
    if (range.empty()) {
        setIfChanged(offsetReadout_, juce::String {});
    } else {
        setIfChanged(offsetReadout_,
                     juce::String::formatted("REG 0x%08X-0x%08X",
                                             static_cast<unsigned long long>(range.start),
                                             static_cast<unsigned long long>(range.end)));
    }

    setIfChanged(peakReadout_,
                 peak < -0.5f ? juce::String {"PEAK -inf"}
                              : juce::String {"PEAK "} + juce::String {peak, 1});
    setIfChanged(rateReadout_,
                 juce::String {"SR "}
                     + juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f,
                                     1}
                     + " kHz");
    setIfChanged(voicesReadout_,
                 juce::String {"VOICES "} + juce::String {activeVoices});

    // O header repete o numero de vozes que o rodape mostra, como o "VOICES 08"
    // do mock, mas com o limite real do motor (8). Duas casas para o numero nao
    // fazer o texto saltar de largura entre 1 e 10.
    setIfChanged(voices_, juce::String {"VOICES "}
                              + juce::String {activeVoices}.paddedLeft('0', 2)
                              + " / 08");

    // Sem material nao ha entropia nem offset para ler. Uma caixa vazia e' um
    // retangulo com moldura e nada dentro, que se le como defeito, e' o mesmo
    // motivo que esconde os chips vazios do cabecalho.
    //
    // A entropia tem a sua regra propria, em updateFooterVisibility, porque
    // depende tambem da largura. As outras duas so dependem do conteudo, e sao
    // sempre largas o suficiente para escrever o que tem.
    updateFooterVisibility();
    positionReadout_.setVisible(positionReadout_.getText().isNotEmpty());
    offsetReadout_.setVisible(offsetReadout_.getText().isNotEmpty());

    // O LED de vozes nunca e' a unica pista: o numero ao lado e' o mesmo dado
    // em texto, que e' o que o criterio 1.4.1 do WCAG pede.
    voicesLed_.setState(sounding && activeVoices > 0 ? Led::State::ready : Led::State::off);
}

float PluginEditor::readPosition() const {
    // O parametro e' a fonte da verdade. Guardar a fracao numa variavel do editor
    // e' o caminho que faz o display e o host falarem um com o outro em zigue
    // zague: o host mexe, o display reescreve, e o host volta a mexer.
    //
    // Neste JUCE, getValue() devolve a fracao normalizada. POSITION tem faixa
    // 0 a 1, em que normalizado e' o valor, por isso a distincao nao aparece
    // aqui — mas nao se pode estender a leitura aos outros cinco parametros sem
    // converter, e por isso a conversao fica escrita.
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

double PluginEditor::entropyAtReadHead(std::uint64_t address,
                                       const PluginProcessor::ByteRange& range) const {
    // Janela de 256 bytes a partir da cabeca de leitura, recortada pela regiao.
    // Recortar pela regiao e' obrigatorio: bytes de fora do material nao sao
    // material, e medi-los era o mesmo erro que a curva do ficheiro inteiro ao
    // lado de uma regiao estreita.
    constexpr std::size_t kWindow {256};

    const auto& bytes = owner_.sourceBytes();
    if (bytes.empty() || address < range.start || address >= range.end) {
        return 0.0;
    }

    const auto available = static_cast<std::uint64_t>(bytes.size()) - address;
    const auto length =
        juce::jmax<std::uint64_t>(1, juce::jmin<std::uint64_t>(kWindow, available));

    return entropy::shannonBitsPerByte(bytes.data(), bytes.size(),
                                       static_cast<std::size_t>(address),
                                       static_cast<std::size_t>(length));
}

void PluginEditor::activateByte(std::uint64_t address) {
    // O clique pode cair fora da regiao, e entao o material e' puxado para la
    // primeiro. A ordem nao e' arbitraria: o POSITION e' uma fracao da regiao, e
    // escrevelo antes de a regiao mudar apontaria para o sitio errado.
    const auto range = owner_.byteRange();
    if (address < range.start || address >= range.end) {
        moveRegionTo(address);
    }

    const auto updated = owner_.byteRange();

    // setValueNotifyingHost, e nao setValue: e' o que notifica o host, faz o
    // knob seguir e fica no historico de automacao. E' o mesmo caminho que
    // applyPendingControllerChanges usa para os CCs.
    //
    // Neste JUCE o valor e' normalizado. POSITION tem faixa 0 a 1, e
    // positionForByte devolve uma fracao de 0 a 1, entao os dois espacos
    // coincidem — e nao coincidir era um erro silencioso em vez de um visivel.
    if (auto* parameter = owner_.parameters().getParameter("position")) {
        parameter->setValueNotifyingHost(
            static_cast<float>(pe::positionForByte(address, updated)));
    }
}

void PluginEditor::moveRegionTo(std::uint64_t address) {
    // O comprimento da regiao preserva-se, que e' a regra que o seletor de 30 px
    // usava: mudar o inicio nao muda o tamanho da janela. O Processor valida e
    // recorta o fim se o inicio novo empurrar a janela para fora do ficheiro.
    const auto range = owner_.byteRange();
    const auto length = juce::jmax<std::uint64_t>(range.length(), 64);
    static_cast<void>(owner_.setByteRange(address, address + length));
}

void PluginEditor::chooseFile() {
    chooser_ = std::make_unique<juce::FileChooser>(
        "Escolha um binario", juce::File::getCurrentWorkingDirectory(),
        "*.exe;*.dll;*.bin;*.sys");

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& finished) {
            if (finished.getResults().size() > 0) {
                owner_.ingest(finished.getResults().getReference(0).getFullPathName());
                refresh();
            }
        });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

} // namespace opcoda
