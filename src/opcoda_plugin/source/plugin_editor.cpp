#include "plugin_editor.h"

#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

// 44 px e nao 54: sem caixa no nome do ficheiro nem palavra nos botoes, a
// faixa nao precisa de ar. O LOAD de 28 px entra a y+10 com 6 de respiro, e o
// nome a y+14 com 24 — tudo com folga de 6 px em baixo.
constexpr int kHeaderHeight {44};

// O display passou de 130-210 px para 200-320 px. Uma grelha com endereco,
// dezasseis colunas e ASCII precisa de 14 px de cabecalho mais seis linhas de
// 24 px, e a faixa de 14 px do estado e os 16 px do rodape. Com a altura de
// antes a grelha ficava com tres linhas e meia, e o endianco do ficheiro ficava
// fora do ecra quase sempre.
constexpr int kMinDisplayHeight {200};
constexpr int kMaxDisplayHeight {320};

// Janela minima e' 480x410. A altura desceu de 420 com o header de 44 e a faixa
// de vistas de 24: grelha com endereco, dezasseis colunas e ASCII precisa de 14
// px de cabecalho mais seis linhas de 24 px, e o painel de parametros precisa de
// espaco para o titulo e os seis knobs.
//
// A largura desce a 480 por causa do header: a cadeia de pecas sobe da direita
// para a esquerda e, a 820, o chip de formato comecava antes de o botao LOAD
// acabar. E a 480 a grelha deixa de ter espaco para a coluna ASCII e passa a
// omite-la, o que torna o comportamento do criterio 1.4.10 um caso real e nao um
// ramo morto. Com ficheiro carregado a 480 ficam o LOAD, o nome e o contador de
// vozes, e nada mais.
constexpr int kMinWidth {480};
constexpr int kMinHeight {410};

constexpr int kAddressFieldWidth {132};
constexpr int kPlayButtonWidth {60};
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
      owner_(processor) {
    setLookAndFeel(&lookAndFeel_);

    makePlainLabel(title_, "Opcoda", palette::textDark, fonts().sans(13.0f, true));
    // **O subtítulo, o contador de vozes do cabeçalho, a dica de arrasto e o título do
    // módulo saíram todos, e cada um tinha um duplo algures no ecrã.**
    //
    //   - "Granular Synthesizer" dizia o que o título "Opcoda" ja diz ao lado;
    //   - "VOICES 8" no cabeçalho duplicava o rodapé, que escreve VOICES a cada
    //     tique com o número real e não o máximo;
    //   - "DRAG & DROP BINARY" duplicava a linha de estado, que escreve a mesma
    //     instrução por extenso enquanto não há material;
    //   - "1 - GRANULAR ENGINE" nomeava um módulo que é o único, e o "1" prometia
    //     um segundo que nunca existiu.
    //
    // Nenhum deles levava informacao que nao estivesse escrita noutro sitio. O que
    // fica no cabeçalho é o LED, o nome, o LOAD, o nome do ficheiro, o formato e o
    // tamanho.
    makePlainLabel(status_, "", palette::textOnDark, fonts().sans(10.0f));

    buildHeader();
    buildViewButtons();
    buildParameterPanel();

    // A thread de interface e' a dona da memoria de amostra, entao e' ela que
    // libera o que a thread de audio ja devolveu.
    //
    // **60 Hz e nao 20 Hz, e a razao e' a animacao.** A 20 Hz cada quadro dura
    // 50 ms, e uma meia-vida de 60 ms da menos de dois quadros por meia-vida: isso
    // e' degrau e nao suavizacao. O custo da subida e' um repaint do display a mais
    // por segundo, e o desenho custa O(colunas) e nao O(bytes), porque a reducao
    // esta' memorizada.
    //
    // O texto nao vira ruido a 60 Hz porque setIfChanged so escreve quando o texto
    // muda, e trocar o texto de um Label dispara um evento de acessibilidade.
    startTimerHz(60);

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
                            static_cast<juce::Component*>(&playButton_),
                            static_cast<juce::Component*>(&voicesLed_)}) {
        addAndMakeVisible(*component);
    }

    // A grelha escreve o POSITION. O duplo clique e a tecla A alternam entre a
    // regiao exacta e a secao PE mais proxima; o metodo do Processor e' quem
    // decide o sentido da alternancia, porque e' ele quem guarda a regiao
    // exacta anterior.
    grid_.onAddressActivated = [this](std::uint64_t address) { activateByte(address); };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
    address_.onAddressEntered = [this](std::uint64_t address) { moveRegionTo(address); };

    // O display e' o componente que fica com o foco quando nao ha campo de endereco
    // com foco, e por isso e' ele que responde ao espaco e ao A. O editor responde
    // tambem, para o caso de o foco estar noutro sitio — um botao, por exemplo.
    grid_.onToggleTransport = [this] {
        owner_.toggleTransport();
        refreshPlayButton();
    };
    grid_.onSnapRequestedFromKey = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };

    // Tecla A e duplo-clique na grelha alinham a regiao; nao ha botao porque o
    // terceiro controle na linha de estado empurrava o texto para debaixo do
    // campo. A dica vive no help text do display e em docs/10 — setHelpText e
    // nao setTooltip, pelo mesmo motivo do HexGrid: o Component so expoe ajuda
    // pelo AccessibilityHandler.
    grid_.setHelpText("Duplo-clique ou tecla A: alinha a regiao a secao PE mais proxima.");

    // Botao de transporte. E' um toggle, e a forma troca com o estado (▶/■): e'
    // a segunda pista de estado, e a que funciona para quem nao distingue cores.
    // Sem texto e sem cromo — so o glifo, desenhado pelo LookAndFeel.
    playButton_.getProperties().set("uiIcon", "transport");
    playButton_.setButtonText("");
    playButton_.setClickingTogglesState(true);
    playButton_.setTooltip(
        "Percorre a regiao seleccionada de inicio a fim, sem nota MIDI. Tambem na "
        "barra de espaco.");
    playButton_.setName("Reproduzir a regiao");
    playButton_.onClick = [this] {
        owner_.toggleTransport();
        refreshPlayButton();
    };

    for (auto* readout : {&entropyReadout_, &positionReadout_, &offsetReadout_,
                           &transportReadout_, &peakReadout_, &voicesReadout_}) {
        addAndMakeVisible(*readout);
        makePlainLabel(*readout, "", palette::textOnDark, fonts().sans(10.0f));
        // Centrado como antes, quando havia caixa: a posicao do texto na faixa
        // nao muda, so some a moldura. Mudar o alinhamento junto seria duas
        // mudancas visuais numa.
        readout->setJustificationType(juce::Justification::centred);
        readout->setInterceptsMouseClicks(false, false);
        // A cor de texto por omissao do Label e' quase preta e desaparece sobre
        // o display escuro. textOnDark mantem a leitura acima de 4,5:1, que e'
        // o que o criterio 1.4.3 exige. O rodape vive no display, nao no chassis
        // claro, e por isso nao pode usar os tokens de superficie clara.
    }

    makePlainLabel(fileName_, "-", palette::textDark, fonts().sans(10.0f));
    makePlainLabel(formatTag_, "", palette::textDark, fonts().sans(10.0f));
    makePlainLabel(fileSize_, "", palette::textDark, fonts().sans(10.0f));
    fileName_.setJustificationType(juce::Justification::centred);
    formatTag_.setJustificationType(juce::Justification::centred);
    fileSize_.setJustificationType(juce::Justification::centred);

    loadButton_.getProperties().set("uiIcon", "folder");
    loadButton_.setButtonText("");
    loadButton_.setTooltip("Escolher um binario para sintetizar (tecla L)");
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
void PluginEditor::buildViewButtons() {
    // Os rotulos sao curtos de proposito. O nome accessivel de cada um e' a
    // descricao completa, e o texto de ajuda explica a tecla: um rotulo de 15
    // caracteres num botao de 60 px sairia cortado, e um rotulo cortado e' pior do
    // que um rotulo curto com um nome accessivel por tras.
    struct Spec {
        juce::TextButton* button;
        const char* shortLabel;
        const char* spokenName;
        const char* hint;
        ByteDisplay::ViewMode mode;
    };

    const Spec specs[] = {
        {&viewWaveButton_, "WAV", "Forma de onda do material", "Tecla 1", ByteDisplay::ViewMode::waveform},
        {&viewHexButton_, "HEX", "Grelha de bytes do material", "Tecla 2", ByteDisplay::ViewMode::hex},
        {&viewEntropyButton_, "ENT", "Curva de entropia do material", "Tecla 3", ByteDisplay::ViewMode::entropy},
    };

    for (auto* button : {static_cast<juce::Component*>(&viewWaveButton_),
                         static_cast<juce::Component*>(&viewHexButton_),
                         static_cast<juce::Component*>(&viewEntropyButton_)}) {
        addAndMakeVisible(*button);
    }

    for (const auto& spec : specs) {
        spec.button->setButtonText(spec.shortLabel);
        spec.button->setClickingTogglesState(true);
        spec.button->setTooltip(juce::String {spec.spokenName} + ". " + spec.hint);
        spec.button->setName(spec.spokenName);
        // Aba sem cromo: so a palavra e o filete do estado. O desenho e' do
        // LookAndFeel, que trata "tab" sem fundo nem bisel.
        spec.button->getProperties().set("uiIcon", "tab");
    }

    // Os tres botoes nao estao em grupo: cada um e' independente e e' o LookAndFeel
    // que desenha o estado ligado. Um juce::ButtonGroup daria exclusao mutua de
    // graca, mas traria um listener que so serve para reescrever o que
    // updateViewButtons ja escreve a partir do modo.
    viewWaveButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::waveform);
        updateViewButtons();
    };
    viewHexButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::hex);
        updateViewButtons();
    };
    viewEntropyButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::entropy);
        updateViewButtons();
    };

    updateViewButtons();
}

void PluginEditor::updateViewButtons() {
    const auto mode = grid_.mode();

    const auto mark = [&mode](juce::TextButton& button, ByteDisplay::ViewMode candidate) {
        const auto on = mode == candidate;
        if (button.getToggleState() != on) {
            button.setToggleState(on, juce::dontSendNotification);
        }
    };

    mark(viewWaveButton_, ByteDisplay::ViewMode::waveform);
    mark(viewHexButton_, ByteDisplay::ViewMode::hex);
    mark(viewEntropyButton_, ByteDisplay::ViewMode::entropy);
}

void PluginEditor::updateHeaderVisibility() {
    const auto& info = owner_.sourceInfo();

    // Duas condicoes, e ambas tem de ser verdade. A peca e' escondida quando nao
    // ha espaco OU quando nao ha conteudo — e nunca se desenha uma moldura vazia,
    // que e' o mesmo motivo pelo qual os chips se escondem por vazio.
    fileSize_.setVisible(headerFitsSize_ && info.sizeBytes > 0);
    formatTag_.setVisible(headerFitsFormat_ && info.formatTag.isNotEmpty());
}

void PluginEditor::updateFooterVisibility() {
    // Mesma regra do header: a largura decide se a caixa existe, e o conteudo
    // decide se ha algo para escrever. Uma caixa a mais pequena e' pior do que
    // nenhuma, porque o texto sai cortado e nao se percebe o que e'.
    peakReadout_.setVisible(footerFitsPeak_);
    entropyReadout_.setVisible(footerFitsEntropy_ && entropyReadout_.getText().isNotEmpty());

    // A leitura de transporte so aparece a tocar, pela mesma razao que os chips
    // vazios do header se escondem: uma moldura vazia le-se como defeito. Durante
    // a reproducao e' a unica pista textual da cabeca, e sem ela a barra animada
    // seria a unica forma de saber o que se ouve — o que o criterio 1.4.1 nao
    // permite.
    transportReadout_.setVisible(footerFitsTransport_ &&
                                 transportReadout_.getText().isNotEmpty());
}

void PluginEditor::updateStatusVisibility() {
    // O botao de reproducao nunca desaparece por falta de espaco: e' a unica
    // forma de ouvir o material sem teclado MIDI. O que se sacrifica e' o campo
    // de endereco, que escreve um sitio que o POSITION e o clique na grelha
    // escrevem tambem.
    playButton_.setVisible(owner_.hasSource());
    address_.setVisible(owner_.hasSource() && statusFitsAddress_);
}

void PluginEditor::refreshPlayButton() {
    const bool playing = owner_.isTransportPlaying();

    // A forma e' a pista de estado que sobrevive a quem nao ve cor: ▶ parado,
    // ■ a tocar. O nome accessivel troca junto para o leitor de tela dizer o
    // que o glifo mostra.
    if ((playButton_.getName() == "Parar a reproducao") != playing) {
        playButton_.setName(playing ? "Parar a reproducao" : "Reproduzir a regiao");
    }

    if (playButton_.getToggleState() != playing) {
        playButton_.setToggleState(playing, juce::dontSendNotification);
    }

    // **Desativado e nao invisivel, e nao activo-a-fingir.** Sem material nao ha
    // regiao para percorrer; com uma regiao de um ou dois bytes nao ha posicao que
    // produza som, porque o motor precisa de `index + 1` e `hasRegion()` e' `>= 3`;
    // e com o host parado nao ha callback que produza o som. Um botao que aceita o
    // toque e nao faz nada e' pior do que um botao que recusa, porque o primeiro faz
    // o utilizador achar que o plugin avariou.
    const auto canPlay = owner_.hasSource() && owner_.hasPlayableRegion() &&
                         !owner_.hostTransportIsKnownToBeStopped();
    playButton_.setEnabled(canPlay);
}

void PluginEditor::pushTransportAnchor() {
    // A ancora so e' escrita quando o utilizador mexeu em POSITION. Escrever a cada
    // quadro faria a geracao do nucleo subir a cada quadro, e a cabeca nunca
    // passaria do ponto de ancoragem — que e' o modo de falha que o teste
    // Transport.SeekIsAppliedOnceAndThenTheHeadAdvanced cobre do outro lado.
    if (!positionChangedSinceLastAnchor_) {
        return;
    }
    positionChangedSinceLastAnchor_ = false;
    lastAnchoredPosition_ = readPosition();
    owner_.seekTransportTo(lastAnchoredPosition_);
}

void PluginEditor::buildParameterPanel() {
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

    // Sombra de 1 px sob a costura do header: a faixa de cima assenta sobre o
    // resto do chassi em vez de estar pintada nele. Preta a 0,15 — sombra e nao
    // linha, porque linha seria mais um divisor e a costura ja divide.
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawHorizontalLine(kHeaderHeight + 1, 0.0f, static_cast<float>(getWidth()));

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
    // O wordmark e' a unica coisa com largura propria que nao e' uma cadeia: o nome
    // do plugin nao cresce com a janela, e 180 px era a largura que o subtitulo
    // "Granular Synthesizer" exigia. Com ele fora, o titulo precisa de metade.
    constexpr int kIdentityWidth {96};
    constexpr int kIdentityGap {12};
    // 40 px porque e' um icone de pasta, nao a palavra LOAD: os 36 px que
    // sobram vao para o nome do ficheiro, que e' quem precisa deles.
    constexpr int kLoadWidth {40};
    constexpr int kNameGap {6};
    constexpr int kMinNameWidth {40};
    constexpr int kChipGap {4};
    constexpr int kFormatWidth {78};
    constexpr int kSizeWidth {58};

    powerLed_.setBounds(juce::Rectangle<int> {x, header.getY(), kLedWidth, header.getHeight()}
                             .withSizeKeepingCentre(14, 14));
    x += kLedWidth + kGap;

    // O titulo centrado na altura do cabeçalho. Estava a 10 px do topo porque
    // tinha um subtitulo por baixo; sem ele, centrar vertical e' o que evita o
    // wordmark parecer colado ao topo da faixa.
    title_.setBounds(juce::Rectangle<int> {x, header.getY(), kIdentityWidth, header.getHeight()}
                         .withSizeKeepingCentre(kIdentityWidth, 19));
    x += kIdentityWidth + kIdentityGap;

    // Tudo o que fica a partir daqui tem de caber a serio. `leftLimit` e' o fim do
    // bloco fixo: LED, wordmark, botao LOAD e o nome do ficheiro no minimo. E'
    // contra ele que cada peca decide se cabe.
    const int leftLimit = x + kLoadWidth + kNameGap + kMinNameWidth;

    // `stripEdge` e' o cursor que desce da direita para a esquerda. Nem `cursor`
    // nem `edge`: `cursor` e' um membro de juce::Component e o /W4 trata a
    // ocultacao como erro, e `edge` ja e' o cursor do rodape mais abaixo, na
    // mesma funcao. O /W4 ja apanhou este mesmo tropeco duas vezes neste
    // ficheiro.
    //
    // **A borda direita e' a borda do cabecalho.** Antes havia um bloco VOICES de 78
    // px a que a cadeia se encostava, e ele saiu por repetir o rodapé.
    int stripEdge = header.getRight();

    // A ordem em que se sacrifica e' a ordem de importancia invertida: primeiro o
    // formato, e o tamanho em ultimo. Ambos sao informacao sobre o material, mas
    // menos importante do que o nome.
    //
    // Sem esta regra o chip de formato comeca em 174 px numa janela de 560 e o
    // botao LOAD acaba em 300 — 126 px de sobreposicao que so nao se via porque
    // os chips vazios ja se escondem, e por isso so aparecia depois de carregar
    // um ficheiro.

    // 1. O tamanho, na ponta direita da faixa.
    headerFitsSize_ = stripEdge - kSizeWidth >= leftLimit;
    if (headerFitsSize_) {
        fileSize_.setBounds(juce::Rectangle<int> {stripEdge - kSizeWidth, header.getY() + 17,
                                                  kSizeWidth, 16});
        stripEdge -= kSizeWidth + kChipGap;
    }

    // 2. O formato, a seguir.
    headerFitsFormat_ = stripEdge - kFormatWidth >= leftLimit;
    if (headerFitsFormat_) {
        formatTag_.setBounds(juce::Rectangle<int> {stripEdge - kFormatWidth, header.getY() + 17,
                                                   kFormatWidth, 16});
        stripEdge -= kFormatWidth + kChipGap;
    }

    // 3. O nome cresce com o que sobrou, e nunca fica abaixo do minimo.
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

    const auto footerY = displayBounds.getBottom() - kFooterHeight - 2;

    // A altura da grelha e' a diferenca entre o fim da faixa de estado e o
    // inicio do rodape. Passar footerY como altura — que e' uma coordenada
    // absoluta e nao um tamanho — esticava a grelha 54 px para baixo, e a ultima
    // linha de bytes ficava por cima das leituras.
    const auto gridTop = statusY + kStatusHeight + 1;
    grid_.setBounds(juce::Rectangle<int> {displayBounds.getX(), gridTop,
                                          displayBounds.getWidth(), footerY - gridTop});

    // O campo de endereco e o botao de reproducao ficam na linha do estado, a
    // direita. Ao lado do texto e nao no rodape porque sao controles: e' a zona
    // do display que ja tem moldura, e um campo de texto dentro do rodape de
    // leituras seria indistinguivel de uma leitura.
    //
    // A cadeia desce da direita para a esquerda em largura fixa e degrada em vez
    // de espremer, com o botao de reproducao a ser o ultimo a cair. Sem o
    // alinhar, a 480 px de janela sobra espaco para o texto de estado e para o
    // campo — o que se sacrifica primeiro continua a ser o campo, e nao a dica.
    const int controlY = displayBounds.getY() + 2;
    const int controlHeight = kStatusHeight + 2;
    const int controlGap = 6;

    playButton_.setBounds(juce::Rectangle<int> {displayBounds.getRight() - 12 - kPlayButtonWidth,
                                                controlY, kPlayButtonWidth, controlHeight});
    const int afterPlay = playButton_.getX() - controlGap;

    statusFitsAddress_ = afterPlay - kAddressFieldWidth >= displayBounds.getX() + 150;
    address_.setBounds(juce::Rectangle<int> {afterPlay - kAddressFieldWidth, controlY,
                                             kAddressFieldWidth, controlHeight});

    // O texto de estado e' medido depois dos controlos e nao antes, porque e' a
    // largura deles que diz onde o texto acaba. Com uma largura fixa, o texto
    // escrevia por baixo do campo de endereco e do botao — e o fundo opaco do
    // campo tapava o "PRONTO", que e' o que o criterio 3.3.1 exige que se veja.
    //
    // A decisao vem das flags que acabaram de ser calculadas e nao de isVisible():
    // nesta passagem do layout a visibilidade ainda e' a da janela anterior, e o
    // texto saltava uma largura para a esquerda e para a direita enquanto a
    // janela era redimensionada.
    const int statusLeft = displayBounds.getX() + 12 + 16 + 8;
    const auto hasSource = owner_.hasSource();
    const int statusRight =
        juce::jmax(statusLeft + 40,
                   (hasSource && statusFitsAddress_) ? address_.getX() - controlGap
                                                    : afterPlay);
    status_.setBounds(juce::Rectangle<int> {statusLeft, statusY, statusRight - statusLeft,
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
    // mostrar nada. A ordem de sacrificio e' a do valor: primeiro o pico, a
    // entropia e' a ultima a cair. POS, REG e VOICES nunca saem, porque sao o
    // que o painel existe para mostrar. A taxa nem entra na ordem: saiu para o
    // tooltip do titulo.
    constexpr int footerHeight {16};
    const int margin = displayBounds.getX() + 12;
    const int footerRight = displayBounds.getRight() - 12;

constexpr int kLedBoxWidth {12};
    constexpr int kBoxGap {8};
    constexpr int kVoicesBoxWidth {74};
    constexpr int kPeakWidth {86};
    constexpr int kTransportWidth {128};
    constexpr int kPositionWidth {116};
    constexpr int kRegionWidth {116};
    constexpr int kMinEntropyWidth {96};

    // O que nunca sai: o LED e o numero de vozes, o endereco da cabeca de leitura
    // e o intervalo da regiao. E' o que o painel existe para mostrar, e sao as
    // tres leituras que respondem a pergunta "o que estou a ouvir".
    const int essential = kLedBoxWidth + kBoxGap + kVoicesBoxWidth + kPositionWidth + kRegionWidth;
    const auto room = footerRight - margin - essential;

    // A partir daqui decide-se por ordem de prioridade, e a ordem esta' escrita
    // na cadeia e nao numa frase ao lado. A taxa ja saiu daqui — constante na
    // sessao, foi para o tooltip do titulo — por isso a cadeia agora e' curta:
    // entropia, transporte, pico.
    //
    // A ordem e' a do valor: **a leitura de transporte vem antes do pico.** E' a
    // unica caixa que muda durante a sessao para alem do pico, e sem ela o
    // display mostra uma barra a andar sem nenhum numero que a confirme.
    footerFitsEntropy_ = room >= kMinEntropyWidth;
    const auto afterEntropy = room - (footerFitsEntropy_ ? kMinEntropyWidth + kBoxGap : 0);
    footerFitsTransport_ = afterEntropy >= kTransportWidth;
    const auto afterTransport =
        afterEntropy - (footerFitsTransport_ ? kTransportWidth + kBoxGap : 0);
    footerFitsPeak_ = afterTransport >= kPeakWidth;

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
    put(transportReadout_, kTransportWidth, footerFitsTransport_);
    put(peakReadout_, kPeakWidth, footerFitsPeak_);

    // A entropia ocupa o que sobra a esquerda, e por isso fica colada ao
    // indicador de vozes: e' a unica das tres que cresce em vez de ter largura
    // fixa, porque o numero que escreve — de 0 a 8 bits por byte — e' o mais
    // variavel das leituras.
    entropyReadout_.setBounds(juce::Rectangle<int> {margin, footerY + 2,
                                                     juce::jmax(0, edge - margin), footerHeight});

    updateFooterVisibility();

    area.removeFromTop(6);

    // ---- selector de vista ----
    //
    // Fica em cima do painel de parametros e nao dentro do display: e' um
    // instrumento de navegacao, e um instrumento que fica dentro da coisa que ele
    // instrumenta desaparece quando a coisa muda. Tres palavras sem cromo, 46 px
    // cada com 4 de vao; a 480 de janela ainda sobra para metade delas.
    //
    // A altura e' 22 px e nao 16: alvo de toque perto dos 24 do 2.5.8, com as
    // teclas 1, 2 e 3 como caminho garantido. A aba activa leva peso forte e o
    // filete — o texto carrega o estado, o filete e' redundancia.
    {
        constexpr int kViewButtonWidth {46};
        constexpr int kViewButtonHeight {22};
        constexpr int kViewGap {4};
        const auto viewRow = area.removeFromTop(kViewButtonHeight + 2);

        struct View {
            juce::TextButton* button;
            ByteDisplay::ViewMode mode;
        };
        const View views[] = {
            {&viewWaveButton_, ByteDisplay::ViewMode::waveform},
            {&viewHexButton_, ByteDisplay::ViewMode::hex},
            {&viewEntropyButton_, ByteDisplay::ViewMode::entropy},
        };

        const int stripWidth = (kViewButtonWidth + kViewGap) * 3 - kViewGap;
        const auto active = grid_.mode();
        const bool fitsAll = viewRow.getWidth() >= stripWidth;

        // **Quando nao cabem os tres, mostra so o da vista activa.** Um conjunto
        // parcial e' pior que um botao unico: com dois dos tres no ecra, o que
        // falta e' uma vista que o utilizador nao sabe que existe, e o botao que
        // sobra e' um comando que muda de vista sem explicar para que. Com um so,
        // o selector continua verdadeiro — diz em que vista se esta — e a troca
        // continua a fazer-se pelas teclas 1, 2 e 3, que nao dependem de largura.
        int viewX = viewRow.getX();
        for (const auto& view : views) {
            const auto isActive = view.mode == active;
            view.button->setVisible(fitsAll || isActive);
            if (!fitsAll && !isActive) {
                continue;
            }
            view.button->setBounds(juce::Rectangle<int> {viewX, viewRow.getY() + 1,
                                                         kViewButtonWidth, kViewButtonHeight});
            viewX += kViewButtonWidth + kViewGap;
        }
    }

    // ---- painel de parametros ----
    //
    // Nao ha titulo de modulo. Era "1 - GRANULAR ENGINE", e o "1" prometia um
    // segundo modulo que nunca existiu; o que preenchia aquela linha era um titulo
    // de uma secção que e' a unica do painel.
    //
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
    // O delta e' medido e nao assumido. Assumir 1/60 numa animacao que depende do
    // tempo deixa a velocidade depender da taxa de quadros real, que no Windows
    // varia entre 60 e 144 Hz conforme a ligacao do monitor.
    //
    // `getMillisecondCounterHiRes` e nao `getMillisecondCounter`: o contador
    // inteiro tem 15 ms de resolucao a 144 Hz, e um quadro de 144 Hz dura 7 ms. Com
    // o contador inteiro o delta seria 0 ou 15 ms alternadamente, e a animacao
    // dava um soluço a cada dois quadros.
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastTick_ > 0.0 ? static_cast<float>((now - lastTick_) / 1000.0)
                                        : 1.0f / 60.0f;
    lastTick_ = now;

    grid_.tickAnimation(delta);

    refresh();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
    // O display ja consumiu o que lhe competia. Aqui so chegam as teclas de
    // transporte, e so quando o foco nao estava no display.
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        owner_.toggleTransport();
        refreshPlayButton();
        return true;
    }

    // Letras e digitos nao tem codigo no JUCE; ver isTypedCharacter em
    // byte_display.h.
    if (isTypedCharacter(key, 'A')) {
        static_cast<void>(owner_.snapByteRangeToSection());
        return true;
    }

    if (isTypedCharacter(key, 'L')) {
        chooseFile();
        return true;
    }

    if (isTypedCharacter(key, '1')) {
        grid_.setMode(ByteDisplay::ViewMode::waveform);
        updateViewButtons();
        return true;
    }

    if (isTypedCharacter(key, '2')) {
        grid_.setMode(ByteDisplay::ViewMode::hex);
        updateViewButtons();
        return true;
    }

if (isTypedCharacter(key, '3')) {
        grid_.setMode(ByteDisplay::ViewMode::entropy);
        updateViewButtons();
        return true;
    }

    return false;
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

    const auto hostStopped = owner_.hostTransportIsKnownToBeStopped();

    Led::State ledState = Led::State::off;
    juce::String message;

    if (owner_.lastError().isNotEmpty()) {
        ledState = Led::State::fault;
        message = "RECUSADO / " + owner_.lastError();
    } else if (!owner_.hasSource()) {
        message = "ARRASTE UM .EXE, .DLL OU .BIN PARA DENTRO DA JANELA";
    } else if (hostStopped) {
        // O botao esta' desativado, e dizer porquê e' o que evita que o utilizador
        // ache que o plugin avariou. Sem host nenhum esta' desligado nao se sabe,
        // e nao se diz nada — ver hostTransportIsKnownToBeStopped.
        ledState = Led::State::ready;
        message = "PRONTO / " + info.name + " / TRANSPORTE DO HOST PARADO";
    } else {
        ledState = Led::State::ready;
        message = "PRONTO / " + info.name;
    }

    setIfChanged(status_, message);
    status_.setColour(juce::Label::textColourId,
                      ledState == Led::State::fault ? palette::error : palette::textOnDark);

    statusLed_.setState(ledState);
    powerLed_.setState(ledState == Led::State::off ? Led::State::off : Led::State::ready);

    // O botao e' atualizado depois da mensagem porque a mensagem e' que depende do
    // estado que ele produz.
    updateStatusVisibility();
    refreshPlayButton();

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

    // A ancora de busca do transporte so e' escrita quando o POSITION mudou. E' o
    // que impede o editor de reancorar a cada quadro, o que prenderia a cabeca de
    // reproducao no ponto onde o knob estava.
    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

    // A telemetria de reproducao e' lida do que a thread de audio ja publicou, e
    // nao do transporte: o display mostra onde a cabeca de audio esta', e ler o
    // nucleo de lado mostraria o ultimo bloco, nao este.
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

    // A leitura de transporte: posicao da cabeca de reproducao em endereco, o
    // instante da volta e a duracao de uma volta. E' a confirmacao textual da barra
    // que a forma de onda vai mostrar.
    const auto transportPlaying = telemetry.playing.load(std::memory_order_relaxed);
    if (transportPlaying) {
        const auto transportFraction =
            telemetry.playheadFraction.load(std::memory_order_relaxed);
        const auto duration =
            telemetry.playDurationSeconds.load(std::memory_order_relaxed);
        const auto transportHead =
            pe::byteForPosition(static_cast<double>(transportFraction), range);
        const auto elapsed = duration * static_cast<double>(transportFraction);

// A leitura de transporte so e' lida do que a thread de audio ja publicou, e
    // nao do transporte: o display mostra onde a cabeca de audio esta', e ler o
    // nucleo de lado mostraria o ultimo bloco, nao este.
    grid_.setPlayhead(telemetry.playheadFraction.load(std::memory_order_relaxed),
                      transportPlaying);
    grid_.setOutputLevel(peak);

    setIfChanged(transportReadout_,
                     juce::String::formatted("TP 0x%08X  %.1f/%.1fs",
                                             static_cast<unsigned long long>(transportHead),
                                             elapsed, duration));
    } else {
        setIfChanged(transportReadout_, juce::String {});
    }

    // A taxa mora no tooltip do titulo: e' constante na sessao e nao precisa de
    // faixa propria. Quem procura um numero fixo le sem pressa, e a faixa do
    // rodape fica para o que muda. So escreve quando muda, pela mesma razao do
    // setIfChanged nos Labels.
    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (title_.getTooltip() != rateText) {
        title_.setTooltip(rateText);
    }
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

    // Os graos, na mesma leitura de 60 Hz. E' leitura de lado, tal como o resto da
    // telemetria: quem publica e' a thread de audio, uma vez por bloco, e o
    // display mostra o ultimo bloco publicado e nao um estado inventado aqui.
    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);
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
