#include "plugin_editor.h"

namespace opcoda {
namespace {

constexpr int kHeaderHeight {54};
constexpr int kMinDisplayHeight {130};

// Os seis parametros da Tabela 8, e nada mais. Os modulos STATE FILTER e
// MOD & OUTPUT do mock ficaram de fora porque biquad, envelope e dry/wet nao
// existem no nucleo.
const std::array<Knob::Spec, 6> kSpecs {
    Knob::Spec {"grain", "SIZE", "Tamanho de grao, em milissegundos", 40.0f,
                [](float v) { return juce::String {v, 0} + " ms"; }},
    Knob::Spec {"density", "DENSITY", "Densidade, em graos por segundo", 20.0f,
                [](float v) { return juce::String {v, 0} + " /s"; }},
    Knob::Spec {"position", "POSITION", "Posicao de leitura no material", 0.5f,
                [](float v) { return juce::String {v * 100.0f, 0} + " %"; }},
    Knob::Spec {"spray", "SPRAY", "Spray, dispersao da posicao de inicio", 0.0f,
                [](float v) { return juce::String {v * 100.0f, 0} + " %"; }},
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
    setResizeLimits(560, 380, 4096, 4096);
    setSize(820, 470);
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

    for (auto* component : {static_cast<juce::Component*>(&tabs_),
                            static_cast<juce::Component*>(&curve_),
                            static_cast<juce::Component*>(&voicesLed_),
                            static_cast<juce::Component*>(&selector_)}) {
        addAndMakeVisible(*component);
    }

    // O seletor escreve no Processor, que valida e publica na fila. Devolver
    // true impede que o Processor reaja ao que ele proprio acabou de fazer.
    selector_.onRangeChanged = [this](std::uint64_t start, std::uint64_t end) {
        owner_.setByteRange(start, end);
    };

    // O duplo clique e a tecla Enter alternam entre a regiao exacta e a secao
    // alinhada. O metodo do Processor e' quem decide o sentido da alternancia,
    // porque e' ele quem guarda a regiao exacta anterior.
    selector_.onSnapRequested = [this] {
        static_cast<void>(owner_.snapByteRangeToSection());
    };

    for (auto* readout : {&entropyReadout_, &offsetReadout_, &peakReadout_, &rateReadout_,
                          &voicesReadout_}) {
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

    powerLed_.setBounds(juce::Rectangle<int> {x, header.getY(), 16, header.getHeight()}
                            .withSizeKeepingCentre(14, 14));
    x += 16 + 6;

    const int identityWidth = 180;
    title_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, identityWidth, 19});
    subtitle_.setBounds(juce::Rectangle<int> {x, header.getY() + 29, identityWidth, 13});
    x += identityWidth + 12;

    // Lado direito: VOICES colado na borda, DRAG & DROP antes dele.
    const int voicesRight = header.getRight();
    voices_.setBounds(juce::Rectangle<int> {voicesRight - 78, header.getY(), 78, 16}
                          .withSizeKeepingCentre(78, 16));
    dropHint_.setBounds(juce::Rectangle<int> {voicesRight - 78 - 8 - 140, header.getY(),
                                             140, 14}
                            .withSizeKeepingCentre(140, 14));

    // Faixa de arquivo na ordem do mock: botao, nome, formato, tamanho. A
    // esquerda dela comeca depois do wordmark, e nao em x, senao o wordmark fica
    // por baixo da faixa.
    const int stripRight = dropHint_.getX() - 10;
    fileSize_.setBounds(juce::Rectangle<int> {stripRight - 58, header.getY() + 17, 58, 16});
    formatTag_.setBounds(juce::Rectangle<int> {fileSize_.getX() - 4 - 78, header.getY() + 17,
                                              78, 16});
    // O botao LOAD leva a peca quadrada mais o texto, entao precisa de ser mais
    // largo que a peca. A 50 px a peca ocupava quase tudo e o texto saia em
    // "LO", cortado.
    loadButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, 76, 28});

    const int nameLeft = loadButton_.getRight() + 6;
    fileName_.setBounds(juce::Rectangle<int> {nameLeft, header.getY() + 14,
                                              juce::jmax(40, formatTag_.getX() - 6 - nameLeft),
                                              24});

    // ---- display ----
    const auto displayHeight = juce::jlimit(kMinDisplayHeight, 210, getHeight() / 2);
    const auto displayBounds = area.removeFromTop(displayHeight);
    display_.setBounds(displayBounds);

    // Seletor de bytes: 30 px logo abaixo do topo do display. A pega fisica, o
    // mapa de secoes e a barra da regiao precisam de tres faixas distintas;
    // em 22 px as tres colavam-se e o cursor sumia dentro da barra.
    selector_.setBounds(displayBounds.reduced(1, 1)
                            .withHeight(30)
                            .withY(displayBounds.getY() + 1));

    // Abas de secao na faixa de 18 px, logo abaixo do seletor.
    tabs_.setBounds(displayBounds.reduced(1, 1)
                        .withHeight(18)
                        .withY(selector_.getBottom() + 1));

    // A curva ocupa o resto, com 4 px de folga para a linha do topo e 24 px para
    // o rodape de telemetria.
    curve_.setBounds(displayBounds.reduced(5, 30)
                         .withTrimmedBottom(24));

    const int statusY = tabs_.getBottom() + 4;
    statusLed_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12, statusY, 16, 20}
                             .withSizeKeepingCentre(14, 14));
    status_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12 + 16 + 8, statusY + 3,
                                            displayBounds.getWidth() - 36 - 12, 14});

    // Rodape de telemetria, empilhado da direita para a esquerda com largura fixa
    // por caixa. A versao anterior media cada caixa a partir da margem esquerda
    // a cada passo, o que fazia todas ocuparem o mesmo intervalo e se
    // sobreporem: so a ultima pintada aparecia, e o pico, a entropia e o offset
    // ficavam escondidos debaixo das outras.
    const int footerY = displayBounds.getBottom() - 20;
    constexpr int footerHeight {16};
    const int margin = displayBounds.getX() + 12;
    int edge = displayBounds.getRight() - 12;

    voicesLed_.setBounds(juce::Rectangle<int> {edge - 12, footerY + 3, 12, 12});
    edge -= 12 + 8;

    const auto put = [&edge, footerY](juce::Component& target, int width) {
        target.setBounds(juce::Rectangle<int> {edge - width, footerY + 2, width, footerHeight});
        edge -= width + 8;
    };

    put(voicesReadout_, 74);
    put(rateReadout_, 78);
    put(peakReadout_, 86);
    put(offsetReadout_, 116);
    entropyReadout_.setBounds(juce::Rectangle<int> {margin, footerY + 2,
                                                    juce::jmax(0, edge - margin), footerHeight});

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

    // Chip vazio e' um retangulo cinza sem texto, que parece defeito. So
    // aparece depois que ha algo para mostrar.
    formatTag_.setVisible(info.formatTag.isNotEmpty());
    fileSize_.setVisible(info.sizeBytes > 0);

    // Sem ficheiro nao ha secoes para listar. A moldura vazia no topo do display
    // lia-se como elemento partido, entao a faixa so aparece com material
    // carregado. A posicao da linha de estado nao muda: um alvo que salta
    // quando se carrega um binario e' pior do que um espaco constante.
    tabs_.setVisible(owner_.hasSource());
    selector_.setVisible(owner_.hasSource());

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

// Abas e curva so sao reenviadas quando o conteudo muda. Reconstruir o Path a
// 20 Hz sem necessidade seria trabalho inutil no thread de interface, e o
// rebuild de Path e' a parte mais cara do display.
void PluginEditor::refreshTelemetry(const PluginProcessor::SourceInfo& info) {
    const auto signature = info.name + juce::String {info.sizeBytes};
    if (loadedSignature_ != signature) {
        loadedSignature_ = signature;
        tabs_.setSections(info.sections);
        curve_.setCurve(info.entropyCurve);
        selector_.setFileSize(static_cast<std::uint64_t>(info.sizeBytes));
        selector_.setSections(info.sections);
        curve_.repaint();
    }

    // O seletor e' a vista da regiao do Processor. showRange nao emite o
    // callback, entao esta linha nao pode gerar um ciclo com o seletor.
    const auto range = owner_.byteRange();
    selector_.showRange(range.start, range.end);

    // O mapa de secoes mostra onde a regiao ativa caiu, e so essa marca e'
    // persistente: a aba clicada deixava um estado visual que nao controlava
    // nada, e isso era pior do que nao ter selecao nenhuma.
    tabs_.setSoundingRange(range.start, range.end);

    // A curva mostra a regiao selecionada, e nao o ficheiro inteiro. Mostrar
    // sempre o ficheiro inteiro ao lado de uma regiao estreita mentiria sobre o
    // que esta a soar.
    if (info.sizeBytes > 0 && !range.empty()) {
        curve_.setVisibleWindow(static_cast<float>(static_cast<double>(range.start) /
                                                   static_cast<double>(info.sizeBytes)),
                                static_cast<float>(static_cast<double>(range.end) /
                                                   static_cast<double>(info.sizeBytes)));
    } else {
        curve_.setVisibleWindow(0.0f, 1.0f);
    }

    // As abas sao um mapa, e a marca segue a regiao ativa. A regiao pode
    // atravessar secoes, por isso a marca e' por intersecao e nao por aba
    // clicada.
    tabs_.setSoundingRange(range.start, range.end);

    const auto& telemetry = owner_.telemetry();
    const auto peak = telemetry.peakDb.load(std::memory_order_relaxed);
    const auto activeVoices = telemetry.activeVoices.load(std::memory_order_relaxed);
    const auto sounding = telemetry.sounding.load(std::memory_order_relaxed);

    // A leitura de entropia segue o centro da regiao selecionada. O valor vem da
    // curva ja recortada para a janela ativa, por isso continua a ser so uma
    // fracao: a conversao para bytes da regiao vive na curva, e nao aqui.
    const auto fraction = 0.5f;
    curve_.setCursorFraction(fraction);

    setIfChanged(entropyReadout_,
                 info.sections.empty()
                     ? juce::String {}
                     : juce::String {curve_.valueAt(fraction), 2} + " bits/byte");

    // O rodape escreve o offset real da regiao, que e' o que o seletor esta' a
    // escolher. E' hex porque o utilizador esta' dentro de um binario, e o
    // offset em hexadecimal e' o que aparece no resto das ferramentas.
    if (range.empty()) {
        setIfChanged(offsetReadout_, juce::String {});
    } else {
        setIfChanged(offsetReadout_,
                     "0x" + juce::String::formatted("%08X", range.start) + "-" +
                         juce::String::formatted("%08X", range.end));
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
    entropyReadout_.setVisible(entropyReadout_.getText().isNotEmpty());
    offsetReadout_.setVisible(offsetReadout_.getText().isNotEmpty());

    // O LED de vozes nunca e' a unica pista: o numero ao lado e' o mesmo dado
    // em texto, que e' o que o criterio 1.4.1 do WCAG pede.
    voicesLed_.setState(sounding && activeVoices > 0 ? Led::State::ready : Led::State::off);
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