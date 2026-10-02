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
    loadButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 14, 50, 24});

    const int nameLeft = x + 50 + 6;
    fileName_.setBounds(juce::Rectangle<int> {nameLeft, header.getY() + 14,
                                              juce::jmax(40, formatTag_.getX() - 6 - nameLeft),
                                              24});

    // ---- display ----
    const auto displayHeight = juce::jlimit(kMinDisplayHeight, 210, getHeight() / 2);
    const auto displayBounds = area.removeFromTop(displayHeight);
    display_.setBounds(displayBounds);

    const int statusY = displayBounds.getY();
    statusLed_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12, statusY, 16, 20}
                             .withSizeKeepingCentre(14, 14));
    status_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12 + 16 + 8, statusY + 3,
                                            displayBounds.getWidth() - 36 - 12, 14});

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

    const auto setIfChanged = [](juce::Label& label, const juce::String& text) {
        // Trocar o texto dispara um evento de acessibilidade. A 20 Hz isso vira
        // ruido para quem usa leitor de tela, entao so quando muda de fato.
        if (label.getText() != text) {
            label.setText(text, juce::dontSendNotification);
            label.setName(text);
        }
    };

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