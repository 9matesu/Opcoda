#include "plugin_editor.h"

#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

constexpr int kHeaderHeight {54};

// O display passou de 130-210 px para 200-320 px. Uma grelha com endereco,
// dezasseis colunas e ASCII precisa de 14 px de cabecalho mais seis linhas de
// 24 px, e a faixa de 14 px do estado e os 16 px do rodape. Com a altura de
// antes a grelha ficava com tres linhas e meia, e o endianco do ficheiro ficava
// fora do ecra quase sempre.
constexpr int kMinDisplayHeight {200};
constexpr int kMaxDisplayHeight {320};

// Janela minima e' 560x420. A altura subiu de 380 porque uma grelha com
// endereco, dezasseis colunas e ASCII precisa de 14 px de cabecalho mais seis
// linhas de 24 px, e o painel de parametros precisa de 124 px para o titulo e os
// seis knobs.
//
// A largura desce a 480 por causa do header: a cadeia de pecas sobe da direita
// para a esquerda e, a 820, o chip de formato comecava antes de o botao LOAD
// acabar. E a 480 a grelha deixa de ter espaco para a coluna ASCII e passa a
// omite-la, o que torna o comportamento do criterio 1.4.10 um caso real e nao um
// ramo morto. Com ficheiro carregado a 480 ficam o LOAD, o nome e o contador de
// vozes, e nada mais.
constexpr int kMinWidth {480};
constexpr int kMinHeight {420};

constexpr int kAddressFieldWidth {132};
constexpr int kSnapButtonWidth {72};
constexpr int kStatusHeight {14};
constexpr int kFooterHeight {16};

// Os seis parametros da Tabela 8, e nada mais. Os modulos STATE FILTER e
// MOD & OUTPUT do mock ficaram de fora porque biquad, envelope e dry/wet nao
// existem no nucleo.
// Inteiro para uma caixa de valor.
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
    // if e formata em fixo. Ficam como estava para nao mexer no que funciona.
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
    : juce::AudioProcessorEditor(&processor),
      owner_(processor),
      fileName_(palette::subPanel, palette::chassisBorder),
      formatTag_(palette::subPanel, palette::chassisBorder),
      fileSize_(palette::subPanel, palette::chassisBorder) {
    setLookAndFeel(&lookAndFeel_);

    makePlainLabel(title_, "Opcoda", palette::textDark, BoxedLabel::sansFont(14.0f, true));
    makePlainLabel(subtitle_, "Granular Synthesizer", palette::textSub,
                   BoxedLabel::sansFont(10.0f));
    makePlainLabel(dropHint_, "DRAG & DROP BINARY", palette::textField,
                   BoxedLabel::monoFont(9.0f));
    makePlainLabel(voices_, "VOICES  8", palette::textSub, BoxedLabel::monoFont(10.0f));
    makePlainLabel(status_, "", palette::textOnDark, BoxedLabel::monoFont(10.0f));
    makePlainLabel(engineTitle_, "1 - GRANULAR ENGINE", palette::textDark,
                   BoxedLabel::sansFont(10.0f, true));

    fileName_.setText("-", juce::dontSendNotification);

    buildHeader();
    buildParameterPanel();

    // A thread de interface e' a dona da memoria de amostra, entao e' ela que
    // libera o que a thread de audio ja devolveu. 20 Hz nao competem com a
    // audio e bastam para a leitura parecer viva.
    startTimerHz(20);

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, 4096, 4096);
    setSize(900, 540);
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

    loadButton_.setButtonText("LOAD");
    loadButton_.setTooltip("Escolher um binario para sintetizar");
    loadButton_.setName("Carregar binario");
    loadButton_.onClick = [this] { chooseFile(); };
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
    dropHint_.setVisible(headerFitsHint_);
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

void PluginEditor::buildParameterPanel() {
    addAndMakeVisible(engineTitle_);

    for (const auto& spec : kSpecs) {
        knobs_.push_back(std::make_unique<Knob>(owner_.parameters(), spec));
        addAndMakeVisible(*knobs_.back());
    }
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::chassis);

    // Faixa do header: um tom acima do chassis, com a borda de 1 px que separa
    // as tres faixas.
    g.setColour(palette::header);
    g.fillRect(getLocalBounds().withHeight(kHeaderHeight));
    g.setColour(palette::chassisBorder);
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(getWidth()));

    // Realce de 1 px no topo: e' o que da sensacao de superficie em vez de
    // retangulo liso.
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void PluginEditor::resized() {
    auto area = getLocalBounds();

    // ---- header ----
    // Cursor explicito em vez de encadear removeFromLeft: aquele metodo devolve
    // um retangulo novo e NAO consome o original, entao encadear sobrepoe as
// pecas em vez de avanca-las.
    const auto header = area.removeFromTop(kHeaderHeight).reduced(10, 0);
    int x = header.getX();

    // As medidas do header em constantes com nome. O layout e' uma cadeia de
    // larguras que tem de fechar, e numeros soltos nao se somam a olho.
    constexpr int kLedWidth {16};
    constexpr int kGap {6};
    constexpr int kIdentityWidth {180};
    constexpr int kIdentityGap {12};
    constexpr int kLoadWidth {76};
    constexpr int kNameGap {6};
    constexpr int kMinNameWidth {40};
    constexpr int kChipGap {4};
    constexpr int kFormatWidth {78};
    constexpr int kSizeWidth {58};
    constexpr int kStripGap {10};
    constexpr int kHintGap {8};
    constexpr int kHintWidth {140};
    constexpr int kVoicesWidth {78};

    powerLed_.setBounds(juce::Rectangle<int> {x, header.getY(), kLedWidth, header.getHeight()}
                             .withSizeKeepingCentre(14, 14));
    x += kLedWidth + kGap;

    title_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, kIdentityWidth, 19});
    subtitle_.setBounds(juce::Rectangle<int> {x, header.getY() + 29, kIdentityWidth, 13});
    x += kIdentityWidth + kIdentityGap;

    // Lado direito: VOICES colado a borda e sempre visivel. E' leitura de estado,
    // e leitura de estado nao se esconde por falta de espaco.
    voices_.setBounds(juce::Rectangle<int> {header.getRight() - kVoicesWidth, header.getY(),
                                             kVoicesWidth, 16}
                          .withSizeKeepingCentre(kVoicesWidth, 16));

    // Tudo o que fica a esquerda de VOICES tem de caber a serio. `leftLimit` e' o
    // fim do bloco fixo: LED, wordmark, botao LOAD e o nome do ficheiro no
    // minimo. E' contra ele que cada peca decide se cabe.
    const int leftLimit = x + kLoadWidth + kNameGap + kMinNameWidth;

    // `stripEdge` e' o cursor que desce da direita para a esquerda. Nem `cursor`
    // nem `edge`: `cursor` e' um membro de juce::Component e o /W4 trata a
    // ocultacao como erro, e `edge` ja e' o cursor do rodape mais abaixo, na
    // mesma funcao. O /W4 ja apanhou este mesmo tropeco duas vezes neste
    // ficheiro.
    int stripEdge = header.getRight() - kVoicesWidth;

    // A ordem em que se sacrifica e' a ordem de importancia invertida.
    //
    // 1. A dica de arrasto primeiro, porque so interessa enquanto nao ha ficheiro
    //    carregado: depois de carregar, o nome do ficheiro diz o essencial e a
    //    dica e' redundante.
    // 2. O formato a seguir, e o tamanho em ultimo: ambos sao informacao sobre o
    //    material, mas menos importante do que o nome.
    //
    // Sem esta regra o chip de formato comeca em 174 px numa janela de 560 e o
    // botao LOAD acaba em 300 — 126 px de sobreposicao que so nao se via porque
    // os chips vazios ja se escondem, e por isso so aparecia depois de carregar
    // um ficheiro.
    headerFitsHint_ = !owner_.hasSource() && stripEdge - kHintGap - kHintWidth >= leftLimit;
    dropHint_.setVisible(headerFitsHint_);
    if (headerFitsHint_) {
        stripEdge -= kHintGap;
        dropHint_.setBounds(juce::Rectangle<int> {stripEdge - kHintWidth, header.getY(),
                                                  kHintWidth, 14});
        stripEdge -= kHintWidth + kStripGap;
    }

    // 2. O tamanho, na ponta direita da faixa.
    headerFitsSize_ = stripEdge - kSizeWidth >= leftLimit;
    if (headerFitsSize_) {
        fileSize_.setBounds(juce::Rectangle<int> {stripEdge - kSizeWidth, header.getY() + 17,
                                                  kSizeWidth, 16});
        stripEdge -= kSizeWidth + kChipGap;
    }

    // 3. O formato, a seguir.
    headerFitsFormat_ = stripEdge - kFormatWidth >= leftLimit;
    if (headerFitsFormat_) {
        formatTag_.setBounds(juce::Rectangle<int> {stripEdge - kFormatWidth, header.getY() + 17,
                                                   kFormatWidth, 16});
        stripEdge -= kFormatWidth + kChipGap;
    }

    // 4. O nome cresce com o que sobrou, e nunca fica abaixo do minimo.
    loadButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, kLoadWidth, 28});
    fileName_.setBounds(juce::Rectangle<int> {x + kLoadWidth + kNameGap, header.getY() + 14,
                                              juce::jmax(kMinNameWidth,
                                                         stripEdge - (x + kLoadWidth + kNameGap)),
                                              24});

    updateHeaderVisibility();

// ---- display ----
    const auto displayHeight =
        juce::jlimit(kMinDisplayHeight, kMaxDisplayHeight, getHeight() / 2);
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
    statusLed_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12, statusY, 16, 20}
                             .withSizeKeepingCentre(14, kStatusHeight));
    status_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12 + 16 + 8, statusY,
                                            displayBounds.getWidth() - 36 - 12, kStatusHeight});

    const auto footerY = displayBounds.getBottom() - kFooterHeight - 2;

    // A altura da grelha e' a diferenca entre o fim da faixa de estado e o
    // inicio do rodape. Passar footerY como altura — que e' uma coordenada
    // absoluta e nao um tamanho — esticava a grelha 54 px para baixo, e a ultima
    // linha de bytes ficava por cima das leituras.
    const auto gridTop = statusY + kStatusHeight + 1;
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
    constexpr int footerHeight {16};
    const int margin = displayBounds.getX() + 12;
    const int footerRight = displayBounds.getRight() - 12;

    constexpr int kLedBoxWidth {12};
    constexpr int kBoxGap {8};
    constexpr int kVoicesBoxWidth {74};
    constexpr int kRateWidth {78};
    constexpr int kPeakWidth {86};
    constexpr int kPositionWidth {116};
    constexpr int kRegionWidth {116};
    constexpr int kMinEntropyWidth {96};

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
    // grupo: quando uma caixa e' omitida, nao ha um intervalo vazio onde
    // poutineira estar.
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

    // A entropia ocupa o que sobra a esquerda, e por isso fica colada ao
    // indicador de vozes: e' a unica das tres que cresce em vez de ter largura
    // fixa, porque o numero que escreve — de 0 a 8 bits por byte — e' o mais
    // variavel das leituras.
    entropyReadout_.setBounds(juce::Rectangle<int> {margin, footerY + 2,
                                                     juce::jmax(0, edge - margin), footerHeight});

    updateFooterVisibility();

    area.removeFromTop(10);

    // ---- painel de parametros ----
    engineTitle_.setBounds(area.removeFromTop(16).reduced(2, 0));

    // Os seis knobs ficam sempre em uma linha, como no mock. Quebrar em duas
    // linhas foi tentado e e' pior: a altura que sobra nao comporta um knob com
    // titulo e valor, e os knobs despencam para poucos pixels. O diametro do
    // knob e' limitado dentro do componente, entao encolhe com a janela sem
    // precisar de um segundo layout.
    const auto knobArea = area.reduced(8, 2);
    const int cellWidth = knobArea.getWidth() / static_cast<int>(knobs_.size());

    // O painel fecha o conteudo (titulo, knob, valor) e fica centrado na
    // vertical da area. Preencher a celula inteira deixaria um monte de caixa
    // cinza vazia embaixo do knob.
    constexpr int knobContentHeight {13 + 1 + 62 + 4 + 18};
    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        const auto cell = juce::Rectangle<int> {knobArea.getX()
                                                     + (static_cast<int>(i) * cellWidth),
                                                 knobArea.getY(),
                                                 cellWidth,
                                                 knobArea.getHeight()};
        knobs_[i]->setBounds(cell.reduced(4).withHeight(
            juce::jmin(knobContentHeight, cell.getHeight() - 8)));
    }
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
    status_.setColour(juce::Label::textColourId,
                      ledState == Led::State::fault ? palette::error : palette::textOnDark);

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
                     ? juce::String {entropyAtReadHead(readHead, range), 2} + " bits/byte"
                     : juce::String {});

    // A cabeca de leitura em endereco. E' a leitura que responde a pergunta "o
    // que estou a ouvir", e o que o clique na grelha acabou de escolher.
    if (owner_.hasSource() && !range.empty()) {
        setIfChanged(positionReadout_,
                     "POS 0x" + juce::String::formatted(
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
                 peak < -0.5f ? juce::String {"PK -inf dB"}
                              : juce::String {"PK "} + juce::String {peak, 1} + " dB");
    setIfChanged(rateReadout_,
                 juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
                     + " kHz");
    setIfChanged(voicesReadout_,
                 juce::String {"VOICES "} + juce::String {activeVoices});

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
