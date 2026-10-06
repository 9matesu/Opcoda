#include "byte_display.h"

#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda {
namespace {

// Preenchimento do envelope.
// **O min/max quase nao serve para mostrar bytes, e e' por isso que o rms e' que
// desenha o corpo.** O texto de um .exe tem quase todos os valores de 0x00 a 0xFF,
// e portanto o minimo e o maximo de uma coluna estao perto de -1 e de +1 em
// praticamente todas. Com o envelope a 55 % o display ficava um bloco laranja
// solido de altura quase inteira: era truthful e nao parecia uma forma de onda.
//
// Duas camadas resolvem. A de fora e' o min/max, fraca, e diz ate onde o material
// se estica. A de dentro e' o rms, forte, e e' o que varia: um material aleatorio
// fica perto de 0,577, um filler de zero cai a zero e um filler de 0xFF sobe a 1.
// Sao tres alturas muito diferentes, e e' delas que se le se o material e' denso ou
// repetitivo.
constexpr float kSwingFill {0.16f};
constexpr float kCoreFill {0.62f};

} // namespace

ByteDisplay::ByteDisplay() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    // **Focalizavel, e e' o que retira a excecao ao criterio 2.5.8.** A excecao
    // estava escrita em hex_grid.h e em docs/10 porque as celulas eram pintadas e
    // nao componentes, e um Component so nao tem filhos acessiveis. Isso continua
    // verdadeiro: nao ha 200 componentes. O que passou a existir e' um cursor de
    // navegacao com nome e valor, e com ele a operacao completa por teclado que o
    // portao E exige.
    //
    // Nao ha setFocusable: no JUCE 8 a focalizabilidade e' implicita e o que se
    // escreve e' que o componente quer o foco do teclado. E' setWantsKeyboardFocus.
    setWantsKeyboardFocus(true);

    monoFont_ = BoxedLabel::monoFont(10.0f);

    // 4.1.2. O nome diz o que e' e o texto de ajuda diz o que se pode fazer, porque
    // um componente que so se opera com o rato e' um buraco sem nome para quem
    // navega por leitor de tela.
    setName("Display do material binario");
    setHelpText(
        "Tres vistas do material: forma de onda, grelha de bytes e curva de "
        "entropia. Clique para mover a cabeca de leitura. Com o teclado: setas "
        "movem o cursor, Enter ativa o byte, Esc volta a cabeca de leitura, "
        "espaco liga e desliga a reproducao, 1, 2 e 3 mudam de vista e A alinha a "
        "regiao a uma secao PE.");

    hex_.onCellActivated = [this](std::uint64_t address) {
        if (onAddressActivated != nullptr) {
            onAddressActivated(address);
        }
    };
    hex_.onSnapRequested = [this] {
        if (onSnapRequested != nullptr) {
            onSnapRequested();
        }
    };

    addAndMakeVisible(hex_);

    // **A visibilidade do hex tem de sair do modo, e nao de uma mudanca de modo.**
    // A primeira versao escondia o HexGrid no construtor e so o mostrava dentro de
    // setMode(), e o setMode nao corre quando o modo pedido ja e' o corrente. Com o
    // modo inicial em hex, a vista ficava permanentemente vazia: um visor em branco
    // com um botao marcado como activo. applyModeVisibility e' chamado aqui e em
    // setMode, para o estado derivado nunca depender de qual dos dois correu.
    applyModeVisibility();
}

void ByteDisplay::applyModeVisibility() {
    const auto hexMode = mode_ == ViewMode::hex;

    hex_.setVisible(hexMode);
    hex_.setInterceptsMouseClicks(hexMode, hexMode);
}

void ByteDisplay::setSource(const std::vector<std::uint8_t>* bytes,
                            std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    // A cache e' invalidada aqui e nao em setRegion: os bytes mudam com o ingest e
    // a regiao pode ser a mesma. Confundir as duas dava uma curva de entropia do
    // ficheiro anterior desenhada sobre o ficheiro novo.
    cacheValid_ = false;

    hex_.setSource(bytes, fileSize);
    repaint();
}

void ByteDisplay::setRegion(std::uint64_t start, std::uint64_t end) {
    const auto clampedEnd = juce::jmax(end, start);
    if (clampedEnd == regionEnd_ && start == regionStart_) {
        return;
    }

    regionStart_ = start;
    regionEnd_ = clampedEnd;

    if (clampedEnd != cachedRegionEnd_ || start != cachedRegionStart_) {
        cacheValid_ = false;
    }

    hex_.setRegion(regionStart_, regionEnd_);
    repaint();
}

void ByteDisplay::setOutputLevel(float decibels) {
    // O piso e' -60 dB e nao -inf. Um medidor que desaparece quando o sinal e'
    // pequeno deixa de dizer "ha sinal fraco" e passa a dizer "nao ha sinal", que
    // sao coisas diferentes. O texto do rodape ja distingue os dois com PK -inf.
    const auto clamped = std::clamp(decibels, kLevelFloorDb, 0.0f);

    // **A primeira leitura e' um salto, as seguintes deslizam.** O alvo e' uma
    // posicao na barra e o valor esta' no mesmo espaco, mas animar desde zero no
    // primeiro bloco faria a barra varrer o ecra de uma vez.
    if (!levelAnimated_) {
        outputLevel_.jumpTo(normalisedLevel(clamped));
        levelAnimated_ = true;
    } else {
        outputLevel_.set(normalisedLevel(clamped));
    }

    repaint();
}

float ByteDisplay::normalisedLevel(float decibels) noexcept {
    const auto span = 0.0f - kLevelFloorDb;
    return std::clamp((decibels - kLevelFloorDb) / span, 0.0f, 1.0f);
}

void ByteDisplay::tickAnimation(float deltaSeconds) {
    if (!playheadVisible_) {
        playheadFraction_.jumpTo(0.0f);
    }

    const auto before = readHeadFraction_.value() + playheadFraction_.value() +
                        modeFade_.value();

    readHeadFraction_.tick(deltaSeconds, kReadHeadHalfLife);
    playheadFraction_.tick(deltaSeconds, kPlayheadHalfLife);
    modeFade_.tick(deltaSeconds, kModeHalfLife);
    outputLevel_.tick(deltaSeconds, kLevelHalfLife);

    // Repintar so quando algo se mexeu de verdade. Um repaint por quadro sem
    // diferenca visivel e' o preco de uma animacao mal feita, e o editor tem mais
    // coisas para pintar a 60 Hz.
    const auto after = readHeadFraction_.value() + playheadFraction_.value() +
                       modeFade_.value() + outputLevel_.value();
    if (std::abs(after - before) > 1.0e-4f) {
        repaint();
    }
}

void ByteDisplay::setMode(ViewMode mode) {
    if (mode == mode_) {
        return;
    }
    mode_ = mode;

    // O hex e' o unico modo com um Component proprio, porque o seu desenho tem
    // metricas derivadas da fonte e areas de clique por celula. Nos outros dois o
    // desenho e' um Path por coluna e nao ha filhos.
    applyModeVisibility();

    repaint();
}

juce::String ByteDisplay::viewName(ViewMode mode) {
    switch (mode) {
        case ViewMode::waveform: return "FORMA DE ONDA";
        case ViewMode::hex: return "HEX";
        case ViewMode::entropy: return "ENTROPIA";
    }
    return "DESCONHECIDO";
}

void ByteDisplay::resized() {
    // O hex ocupa tudo. Nas outras vistas e' invisivel mas continua a receber o
    // layout, para que a troca de vista nao mude a geometria do ecra.
    hex_.setBounds(getLocalBounds());

    // O numero de colunas depende da largura E do zoom, e a cache esta' indexada
    // pelo numero. O zoom vive no componente e nao no tamanho: mudar o tamanho
    // fightaria com o resized do editor, que e' o dono da geometria.
    const auto wanted = columnCountForWidth();
    if (wanted != cachedColumnCount_) {
        cacheValid_ = false;
    }
}

std::uint32_t ByteDisplay::columnCountForWidth() const noexcept {
    const auto width = getLocalBounds().toFloat().reduced(kPadding).getWidth();
    // Uma coluna por pixel e' o limite util: mais colunas que pixels e' trabalho
    // que nao aparece, menos e' perda de resolucao sem ganho. O zoom move o limite
    // dentro de uma faixa de quatro-octavas em cada sentido.
    const auto scaled = static_cast<float>(width) * columnScale_;
    return static_cast<std::uint32_t>(std::clamp(scaled, 16.0f, 8192.0f));
}

void ByteDisplay::rebuildColumnsIfNeeded() {
    const auto wanted = columnCountForWidth();
    if (cacheValid_ && wanted == cachedColumnCount_ && regionStart_ == cachedRegionStart_ &&
        regionEnd_ == cachedRegionEnd_) {
        return;
    }

    columns_.clear();

    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(),
                                              regionStart_, regionEnd_, wanted, columns_));
    }

    cachedColumnCount_ = wanted;
    cachedRegionStart_ = regionStart_;
    cachedRegionEnd_ = regionEnd_;
    cacheValid_ = true;
}

double ByteDisplay::fractionFor(std::uint64_t address) const noexcept {
    return pe::positionForByte(address, pe::ByteRange {regionStart_, regionEnd_});
}

float ByteDisplay::fractionOf(std::uint64_t address) const noexcept {
    return static_cast<float>(fractionFor(address));
}

void ByteDisplay::setReadHead(std::uint64_t address) {
    readHead_ = address;
    hex_.setReadHead(address);

    // A caret segue a cabeca de leitura ate o utilizador lhe tocar. O editor
    // reenvia a cabeca a cada tique, e sem esta guarda uma volta de 60 Hz repunha
    // a caret no meio e as setas nao mexiam em nada.
    if (!caretMoved_) {
        caret_ = address;
    }

    // A primeira chamada e' um salto, as seguintes deslizam. Sem esta distincao o
    // primeiro desenho animava desde zero e a cabeca de leitura varreria o ecra
    // inteiro ao carregar um ficheiro.
    const auto fraction = fractionOf(address);
    if (!readHeadAnimated_) {
        readHeadFraction_.jumpTo(fraction);
        readHeadAnimated_ = true;
    } else {
        readHeadFraction_.set(fraction);
    }

    repaint();
}

void ByteDisplay::setCaret(std::uint64_t address) {
    caret_ = std::clamp(address, regionStart_, highestUsableAddress());
    caretMoved_ = caret_ != readHead_;
    repaint();
}

std::uint64_t ByteDisplay::highestUsableAddress() const noexcept {
    // Com menos de dois bytes nao ha nenhum endereco que produza som, e o clamp de
    // juce::jmax abaixo mantem a caret dentro da regiao vazia.
    return regionEnd_ >= regionStart_ + 2 ? regionEnd_ - 2 : regionStart_;
}

void ByteDisplay::focusGained(juce::Component::FocusChangeType) {
    repaint();
}

void ByteDisplay::focusLost(juce::Component::FocusChangeType) {
    repaint();
}

void ByteDisplay::handleNavigationKey(const juce::KeyPress& key) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    // O comprimento da regiao e' a unidade de uma linha. Nas vistas novas nao ha
    // linha, e um salto de 16 e' a granularidade que o hex usa e a que um
    // utilizador de teclado espera: um byte de cada vez em 12 MB e' impraticavel.
    constexpr std::uint64_t kStep {1};
    constexpr std::uint64_t kRow {16};

    // Uma "pagina" e' uma regiao de 1024 bytes, que e' a janela que o rodape mede a
    // entropia. Ligar o salto a um numero que ja existe no codigo e' melhor do que
    // inventar um em proportion a altura, que mudaria com a janela e tornaria a
    // mesma tecla um salto diferente em cada tamanho de janela.
    constexpr std::uint64_t kPage {1024};

    const auto upper = highestUsableAddress();
    const auto shift = key.getModifiers().isShiftDown();
    const auto code = key.getKeyCode();
    std::uint64_t target = caret_;
    bool handled = true;

    // **`if` e nao `switch`.** Os codigos do KeyPress sao `static const int`
    // inicializados no .cpp da biblioteca, e nao constantes de compilacao: um
    // `case juce::KeyPress::leftKey` da "a expressao nao foi avaliada como uma
    // constante". Foi o que a primeira versao usou e o que o compilador apanhou.
    if (code == juce::KeyPress::leftKey) {
        target = caret_ - std::min(caret_, shift ? kRow : kStep);
    } else if (code == juce::KeyPress::rightKey) {
        target = caret_ + (shift ? kRow : kStep);
    } else if (code == juce::KeyPress::upKey) {
        target = caret_ - std::min(caret_, kRow);
    } else if (code == juce::KeyPress::downKey) {
        target = caret_ + kRow;
    } else if (code == juce::KeyPress::pageUpKey) {
        target = caret_ - std::min(caret_, kPage);
    } else if (code == juce::KeyPress::pageDownKey) {
        target = caret_ + kPage;
    } else if (code == juce::KeyPress::homeKey) {
        target = regionStart_;
    } else if (code == juce::KeyPress::endKey) {
        target = upper;
    } else if (code == juce::KeyPress::returnKey) {
        // Ativar e' o que o clique faz. E' a unica tecla que escreve: as setas
        // movem a caret e nao mudam o som, e e' por isso que navegar e inofensivo.
        //
        // Depois de ativar, a caret volta a seguir a cabeca de leitura. Sem isto, a
        // proxima volta do timer mantinha a caret onde o utilizador a deixou e as
        // setas recomecavam de um sitio que ja nao e' o sitio da leitura.
        caretMoved_ = false;
        if (onAddressActivated != nullptr) {
            onAddressActivated(caret_);
        }
        return;
    } else {
        handled = false;
    }

    if (!handled) {
        return;
    }

    // O clamp e' a unica proteccao contra transbordo. Um `caret_ - 16` sem isto
    // daria um numero enorme e o Editor escreveria uma posicao absurda.
    setCaret(std::clamp(target, regionStart_, upper));
}

bool ByteDisplay::handleTransportKey(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        if (onToggleTransport != nullptr) {
            onToggleTransport();
            return true;
        }
        return false;
    }

    if (isTypedCharacter(key, 'A') && onSnapRequestedFromKey != nullptr) {
        onSnapRequestedFromKey();
        return true;
    }

    return false;
}

bool ByteDisplay::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::escapeKey) {
        // Devolve a caret a cabeca de leitura: e' a forma de desfazer uma
        // navegacao sem passar pelo knob.
        setCaret(readHead_);
        return true;
    }

    // As teclas de transporte sao respondidas aqui e nao so no editor, porque o
    // display e' o unico componente que fica com foco quando nao ha campo de
    // endereco com foco. Devolvem o resultado para que o editor saiba que ja foram
    // consumidas.
    if (handleTransportKey(key)) {
        return true;
    }

    if (isTypedCharacter(key, '1')) {
        setMode(ViewMode::waveform);
        return true;
    }
    if (isTypedCharacter(key, '2')) {
        setMode(ViewMode::hex);
        return true;
    }
    if (isTypedCharacter(key, '3')) {
        setMode(ViewMode::entropy);
        return true;
    }

    handleNavigationKey(key);
    return true;
}

void ByteDisplay::paintFocusRing(juce::Graphics& g) {
    // 2.4.7 Foco visivel.
    //
    // O anel e' o sinal de que as setas mexem na caret. Sem ele, quem esta' a
    // navegar por teclado nao sabe que o display tem o foco, e as setas parecem nao
    // fazer nada — que e' a falha que o portao E descreve.
    if (!hasKeyboardFocus(true)) {
        return;
    }

    g.setColour(palette::accent);
    g.drawRect(getLocalBounds().toFloat().reduced(1.0f), 2.0f);
}

void ByteDisplay::paint(juce::Graphics& g) {
    g.fillAll(palette::displayPanel);

    if (bytes_ == nullptr || bytes_->empty() || regionEnd_ <= regionStart_) {
        g.setColour(palette::textOnDarkSub);
        g.setFont(monoFont_);
        g.drawText("SEM BINARIO", getLocalBounds().toFloat(), juce::Justification::centred,
                   false);
        return;
    }

    rebuildColumnsIfNeeded();

    switch (mode_) {
        case ViewMode::waveform: paintWaveform(g); break;
        case ViewMode::entropy: paintEntropy(g); break;
        case ViewMode::hex: break; // o HexGrid pinta-se a si proprio
    }

    // **Nao ha banda da regiao nas vistas novas, e a diferenca e' intentional.**
    // No hex a regiao e' uma faixa sobre celulas de largura fixa, porque o que se
    // ve e' o ficheiro inteiro. Na forma de onda e na curva o que se ve ja e' a
    // regiao, do primeiro ao ultimo pixel, e uma banda a marcar o que ocupa tudo
    // seria ruido. A regiao continua marcada pelas duas coisas que importam: o
    // endereco no gutter do hex e o intervalo no rodape, como TP e REG.
    if (mode_ != ViewMode::hex) {
        paintSectionTicks(g);
    }
}

void ByteDisplay::paintSectionTicks(juce::Graphics& g) {
    // Marcas dos limites de secao sobre a curva e a forma de onda.
    //
    // **Sao marcas e nao bandas**, por duas razoes: a regiao ja ocupa tudo o ecra,
    // e uma banda de 12 MB de largura nao diria nada; e o criterio 1.4.1 pede um
    // sinal que nao seja a cor, e um filete de 1 px e' um sinal. O gutter do hex
    // mantem o nome da secao; aqui o nome nao cabe e o que interessa e' onde a
    // secao acaba.
    if (sections_.empty() || regionEnd_ <= regionStart_) {
        return;
    }

    const auto area = getLocalBounds().toFloat().reduced(kPadding);
    if (area.getWidth() <= 0.0f) {
        return;
    }

    for (const auto& section : sections_) {
        if (section.rawSize == 0) {
            continue;
        }
        const auto start = static_cast<std::uint64_t>(section.rawOffset);
        if (start < regionStart_ || start >= regionEnd_) {
            continue;
        }

        const auto x = area.getX() + fractionOf(start) * area.getWidth();

        g.setColour(palette::alpha(palette::chassisBorder, 0.8f));
        g.fillRect(juce::Rectangle<float> {x, area.getY(), 1.0f, area.getHeight()});
    }
}

void ByteDisplay::paintOverChildren(juce::Graphics& g) {
    if (bytes_ == nullptr || bytes_->empty()) {
        return;
    }

    const auto area = getLocalBounds().toFloat().reduced(kPadding);

    // A cabeca de leitura so e' desenhada aqui nas vistas novas. No hex o
    // HexGrid desenha a sua, dentro da celula, porque a cabe a linha e a coluna da
    // celula e nao a uma fracao da largura.
    if (mode_ != ViewMode::hex) {
        paintReadHead(g, area);
    }

    // A cabeca de reproducao esta' nas tres, incluindo no hex. E' a unica peca que
    // o HexGrid nao conhece, e sem ela o botao PLAY nao tinha nada que mostrar
    // na vista dos bytes.
    paintPlayhead(g, area);

    // A caret e' so desenhada com o foco do teclado. Sem foco ela seria uma marca
    // a mais entre a cabeca de leitura e a de reproducao, e tres barras verticais
    // no mesmo ecra nao se distinguem.
    if (hasKeyboardFocus(true) && mode_ != ViewMode::hex) {
        const auto caretX = area.getX() + fractionOf(caret_) * area.getWidth();
        g.setColour(palette::accentSoft);
        g.drawRect(juce::Rectangle<float> {caretX - 3.0f, area.getY() + 2.0f, 6.0f,
                                           area.getHeight() - 4.0f}, 1.0f);
    }

    paintFocusRing(g);

    paintOutputLevel(g);
}

void ByteDisplay::paintOutputLevel(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(kPadding);
    const auto barHeight = kLevelHeight;

    // No topo do display, e nao em baixo: o rodape e' do editor e esta la a telemetria
    // escrita, e um medidor ao pe de uma lista de leituras confundia-se com mais uma
    // leitura. Aqui em cima ele responde ao que se ouve sem competir com nada.
    const auto bar = juce::Rectangle<float> {bounds.getX(), bounds.getY() + 2.0f,
                                             bounds.getWidth(), barHeight};

    // A calha, sempre visivel. Um medidor sem calha desaparece quando o sinal
    // desaparece, e sem ele nao se sabe se o medidor esta' a trabalhar.
    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.fillRect(bar);

    const auto level = outputLevel_.value();
    const auto filled = bar.withWidth(bar.getWidth() * level);
    if (filled.getWidth() > 0.0f) {
        // A cor muda perto do topo: o medidor tem de avisar antes do limitador
        // meter, e so a cor e o que diz. Por isso o valor em texto no rodape
        // importa — sem ele, quem nao distingue o verde do laranja nao sabe.
        const auto hot = level > 0.85f;
        g.setColour(palette::alpha(hot ? palette::accentWarn : palette::okBright, 0.9f));
        g.fillRect(filled);
    }
}

void ByteDisplay::paintWaveform(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto bounds = getLocalBounds().toFloat().reduced(kPadding);
    const auto centreY = bounds.getCentreY();
    const auto halfHeight = bounds.getHeight() * 0.5f;
    const auto columnWidth = bounds.getWidth() / static_cast<float>(columns_.size());

    // Minimo e maximo num Path so, com dois movimentos: subir pela coluna maxima e
    // descer pela minima. Um Path por coluna custava sete vezes mais e nao mudava o
    // desenho.
    juce::Path swing;
    swing.startNewSubPath(bounds.getX(), centreY);

    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const auto x = bounds.getX() + static_cast<float>(i) * columnWidth;
        const auto top = centreY - std::clamp(columns_[i].maximum, -1.0f, 1.0f) * halfHeight;
        swing.lineTo(x, top);
    }

    for (std::size_t i = columns_.size(); i-- > 0;) {
        const auto x = bounds.getX() + static_cast<float>(i) * columnWidth;
        const auto bottom =
            centreY - std::clamp(columns_[i].minimum, -1.0f, 1.0f) * halfHeight;
        swing.lineTo(x, bottom);
    }

    swing.closeSubPath();

    g.setColour(palette::alpha(palette::accent, kSwingFill));
    g.fillPath(swing);

    // O zero. Um envelope sem linha de zero nao diz qual e' o silencio, e o desenho
    // da grelha tem essa linha ha muito tempo.
    g.setColour(palette::alpha(juce::Colours::white, 0.16f));
    g.drawHorizontalLine(juce::roundToInt(centreY), bounds.getX(), bounds.getRight());

    // Nucleo de rms, por cima e com peso. E' a parte que varia entre materiais
    // diferentes, e por isso e' a que o olho segue.
    juce::Path core;
    core.startNewSubPath(bounds.getX(), centreY);
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const auto x = bounds.getX() + static_cast<float>(i) * columnWidth;
        core.lineTo(x, centreY - std::clamp(columns_[i].rms, 0.0f, 1.0f) * halfHeight);
    }
    for (std::size_t i = columns_.size(); i-- > 0;) {
        const auto x = bounds.getX() + static_cast<float>(i) * columnWidth;
        core.lineTo(x, centreY + std::clamp(columns_[i].rms, 0.0f, 1.0f) * halfHeight);
    }
    core.closeSubPath();

    g.setColour(palette::alpha(palette::accent, kCoreFill));
    g.fillPath(core);

    // Contorno do nucleo em amarelo. Um preenchimento sem bordo perde o valor maximo
    // na altura em que a curva esta' no pico, e o pico e' a leitura.
    g.setColour(palette::alpha(palette::accentSoft, 0.9f));
    g.strokePath(core, juce::PathStrokeType {1.0f});
}

void ByteDisplay::paintEntropy(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto bounds = getLocalBounds().toFloat().reduced(kPadding);
    const auto plot = bounds.withTrimmedLeft(kAxisInset).withTrimmedBottom(kAxisHeight);

    // A escala e' 0 a 8 bits por byte, e o eixo escreve os tres valores. Uma curva
    // sem eixo nao tem leitura: 7,5 e' o valor de um .exe normal e 3,5 o de um
    // repositorio de bytes iguais, e sem numeros nao se sabe qual dos dois se esta
    // a ver.
    const auto yFor = [&plot](float bits) {
        return plot.getBottom() - std::clamp(bits, 0.0f, 8.0f) / 8.0f * plot.getHeight();
    };

    g.setColour(palette::alpha(palette::displayGrid, 1.4f));
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawHorizontalLine(juce::roundToInt(yFor(bits)), plot.getX(), plot.getRight());
    }

    g.setFont(monoFont_);
    g.setColour(palette::textOnDarkSub);
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawText(juce::String::formatted("%.0f", static_cast<double>(bits)),
                   juce::Rectangle<float> {bounds.getX(), yFor(bits) - 7.0f, kAxisInset - 6.0f,
                                           14.0f},
                   juce::Justification::centredRight, false);
    }
    g.drawText("bits/byte", juce::Rectangle<float> {plot.getX(), plot.getBottom() + 1.0f,
                                                    plot.getWidth(), kAxisHeight - 2.0f},
               juce::Justification::centredRight, false);

    const auto columnWidth = plot.getWidth() / static_cast<float>(columns_.size());

    juce::Path curve;
    curve.startNewSubPath(plot.getX(), plot.getBottom());
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const auto x = plot.getX() + static_cast<float>(i) * columnWidth;
        curve.lineTo(x, yFor(columns_[i].entropyBits));
    }
    curve.lineTo(plot.getRight(), plot.getBottom());
    curve.closeSubPath();

    g.setColour(palette::alpha(palette::accent, 0.42f));
    g.fillPath(curve);

    // A linha por cima, porque um preenchimento sem bordo nao tem o valor maximo
    // legivel: a curva preenchida em 42 % perde-se no topo.
    juce::Path line;
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const auto x = plot.getX() + static_cast<float>(i) * columnWidth;
        const auto y = yFor(columns_[i].entropyBits);
        if (i == 0) {
            line.startNewSubPath(x, y);
        } else {
            line.lineTo(x, y);
        }
    }
    g.setColour(palette::accent);
    g.strokePath(line, juce::PathStrokeType {1.0f});
}

void ByteDisplay::paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    const auto fraction = readHeadFraction_.value();
    if (fraction < 0.0f || fraction > 1.0f) {
        return;
    }

    const auto x = area.getX() + fraction * area.getWidth();

    // Barra de 2 px a toda a altura, com um realce mais largo por baixo. O realce
    // sem a barra e' uma celula a tremer; a barra sem realce desaparece sobre um
    // envelope claro — que e' exactamente o que uma regiao de bytes 0xFF produz.
    g.setColour(palette::alpha(juce::Colours::white, 0.16f));
    g.fillRect(juce::Rectangle<float> {x - 2.0f, area.getY(), 4.0f, area.getHeight()});
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float> {x - 1.0f, area.getY(), 2.0f, area.getHeight()});
}

void ByteDisplay::paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    if (!playheadVisible_ || columns_.empty()) {
        return;
    }

    const auto fraction = std::clamp(playheadFraction_.value(), 0.0f, 1.0f);
    const auto x = area.getX() + fraction * area.getWidth();

    // Triangulo em cima e barra em baixo. A barra e' o mesmo sinal que a cabeca de
    // leitura e com a mesma forma, e por isso os dois se confundem no ecra.
    //
    // O triangulo e' a distincao: e' o unico elemento com uma forma diferente no
    // display inteiro, e por isso da para dizer "isto anda sozinho" sem escrever
    // uma legenda. O 1.4.1 pede nome ou padrao alem da cor, e um triangulo e' um
    // padrao.
    juce::Path marker;
    marker.addTriangle(x - 4.0f, area.getY(), x + 4.0f, area.getY(), x, area.getY() + 5.0f);
    g.setColour(palette::accentSoft);
    g.fillPath(marker);

    g.setColour(palette::alpha(palette::accentSoft, 0.55f));
    g.fillRect(juce::Rectangle<float> {x - 0.5f, area.getY() + 5.0f, 1.0f,
                                        area.getHeight() - 5.0f});
}

std::uint64_t ByteDisplay::addressAt(juce::Point<float> position) const {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();

    const auto area = getLocalBounds().toFloat().reduced(kPadding);
    if (bytes_ == nullptr || area.getWidth() <= 0.0f || regionEnd_ <= regionStart_) {
        return none;
    }

    const auto fraction = (position.x - area.getX()) / area.getWidth();
    if (fraction < 0.0f || fraction >= 1.0f) {
        return none;
    }

    // A fracao 1,0 e' silencio no motor, entao o ultimo endereco util e' end - 2.
    // E' a mesma regra de pe::positionForByte, e o caminho inverso e' byteForPosition.
    const auto address = pe::byteForPosition(static_cast<double>(fraction),
                                             pe::ByteRange {regionStart_, regionEnd_});
    return address < regionEnd_ ? address : none;
}

void ByteDisplay::mouseDown(const juce::MouseEvent& event) {
    // **O clique da o foco do teclado ao display.** Sem isto, um utilizador que
    // clica no display e depois carrega em Espaco veria o transporte nao responder, e
    // a unica pista seria o anel de foco que so aparece depois de um Tab. E' o mesmo
    // cuidado que o campo de endereco tem em grabFocusOnField.
    grabKeyboardFocus();

    if (mode_ == ViewMode::hex) {
        return; // o HexGrid trata do seu
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onAddressActivated != nullptr) {
        caretMoved_ = false;
        onAddressActivated(address);
    }
}

void ByteDisplay::mouseDoubleClick(const juce::MouseEvent& event) {
    if (mode_ == ViewMode::hex) {
        return;
    }

    if (addressAt(event.position) == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }
    if (onSnapRequested != nullptr) {
        onSnapRequested();
    }
}

void ByteDisplay::mouseWheelMove(const juce::MouseEvent& event,
                                 const juce::MouseWheelDetails& wheel) {
    // Nas vistas novas a roda muda a resolucao, que e' quantos bytes cada coluna
    // representa. E' o unico gesto que faz sentido aqui: a regiao ja esta' toda no
    // ecra, e nao ha lista para percorrer.
    if (mode_ != ViewMode::hex) {
        const auto delta = static_cast<float>(wheel.deltaY);
        columnScale_ = std::clamp(columnScale_ * std::pow(1.15f, delta), 0.25f, 8.0f);
        cacheValid_ = false;
        repaint();
    }

    // No hex a roda e' do HexGrid, que a scroll por linhas. O accumulator dele
    // trata da fracao de entalhe, e um delta aqui viraria duas rolagens por entalhe.
    static_cast<void>(event);
}

} // namespace opcoda