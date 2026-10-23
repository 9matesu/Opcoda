#include "byte_display.h"

#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda {
namespace {

// min/max quase nao diz nada sobre bytes: o texto de um .exe cobre 0x00 a 0xFF em
// quase todas as colunas. por isso o rms desenha o corpo — aleatorio fica perto de
// 0,577, filler de zero ou de 0xFF estica a 1.

} // namespace

ByteDisplay::ByteDisplay() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    // focalizavel: e' o que retira a excecao ao criterio 2.5.8. nao ha 200 componentes,
    // as celulas sao pintadas, mas ha um cursor de navegacao com nome e valor, e com ele
    // a operacao por teclado que o portao E exige.
    // no JUCE 8 a focalizabilidade e' implicita: escreve-se setWantsKeyboardFocus.
    setWantsKeyboardFocus(true);

    font_ = fonts().sans(10.0f);

    // 4.1.2: o nome diz o que e', o texto de ajuda diz o que se pode fazer. um componente
    // que so se opera com o rato e' um buraco sem nome para quem usa leitor de tela.
    setName("Display do material binario");
    setHelpText(
        "Tres vistas do material: forma de onda, grelha de bytes e curva de "
        "entropia. Clique para mover a cabeca de leitura. Com o teclado: setas "
        "movem o cursor, Enter ativa o byte, Esc volta a cabeca de leitura, "
        "espaco liga e desliga a reproducao, 1, 2 e 3 mudam de vista, + e - "
        "zoom, 0 enquadra a regiao e A alinha a "
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

    // o primeiro gesto na grelha corta o seguimento dos graos. ate' la' a janela segue o
    // material; depois manda a pessoa.
    hex_.onUserScrolled = [this] { grainFollow_ = false; };

    addAndMakeVisible(hex_);

    // a visibilidade do hex sai do modo, nao de uma mudanca de modo. setMode nao corre
    // quando o modo pedido ja' e' o corrente, e a vista ficava vazia. applyModeVisibility
    // e' chamado aqui e em setMode.
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

    // a cache invalida-se aqui e nao em setRegion: os bytes mudam com o ingest e a
    // regiao pode ser a mesma. confundir as duas desenhava a entropia do ficheiro
    // anterior sobre o novo.
    cacheValid_ = false;
    overviewValid_ = false;

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
    overviewValid_ = false;

    // regiao nova, viewport cheio: o zoom anterior media-se da regiao antiga e escondia
    // 7/8 do som sem sinal nenhum.
    viewStart_ = 0.0f;
    viewLength_ = 1.0f;

    hex_.setRegion(regionStart_, regionEnd_);

    // a regiao mudou, a fracao mudou de significado: 0,9 na regiao antiga e' outro sitio
    // na nova. sem esvaziar, o rastro velho aparecia sobre o material novo.
    grainTrails_.clear();
    repaint();
}

void ByteDisplay::setOutputLevel(float decibels) {
    // piso de -60 dB e nao -inf: um medidor que desaparece diz "nao ha sinal", que e'
    // outra coisa. o rodape distingue os dois com PK -inf.
    const auto clamped = std::clamp(decibels, kLevelFloorDb, 0.0f);

    // primeira leitura salta, as seguintes deslizam. animar desde zero no primeiro
    // bloco fazia a barra varrer o ecra.
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
    // descarrega o arrasto pendente antes de animar, para a cabeca deslizar logo para o
    // alvo mais recente. uma escrita por quadro e' o tecto: cada uma notifica o host.
    if (pendingScrub_ != std::numeric_limits<std::uint64_t>::max() &&
        onAddressActivated != nullptr) {
        onAddressActivated(pendingScrub_);
        pendingScrub_ = std::numeric_limits<std::uint64_t>::max();
    }

    if (!playheadVisible_) {
        playheadFraction_.jumpTo(0.0f);
    }

    const auto before = readHeadFraction_.value() + playheadFraction_.value() +
                        modeFade_.value();

    readHeadFraction_.tick(deltaSeconds, kReadHeadHalfLife);
    playheadFraction_.tick(deltaSeconds, kPlayheadHalfLife);
    modeFade_.tick(deltaSeconds, kModeHalfLife);
    outputLevel_.tick(deltaSeconds, kLevelHalfLife);

    // repinta so quando algo se mexeu. um repaint por quadro sem diferenca visivel e' o
    // preco de uma animacao mal feita.
    const auto after = readHeadFraction_.value() + playheadFraction_.value() +
                       modeFade_.value() + outputLevel_.value();
    if (std::abs(after - before) > 1.0e-4f) {
        repaint();
    }
}

void ByteDisplay::setGrains(
    const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views, int count) {
    // as posicoes sao fracoes da regiao, e a regiao e' a mesma nas tres vistas. no hex a
    // fracao vira endereco dentro do push, porque o hex mostra o ficheiro inteiro.
    // sem material nao ha regiao: um rastro sobre ecra vazio seria ruido.
    const bool drawable = bytes_ != nullptr && !bytes_->empty() &&
                          regionEnd_ > regionStart_;
    const auto toRead = drawable ? std::clamp(count, 0, dsp::GrainTelemetry::kMaxVoices) : 0;

    for (int i = 0; i < toRead; ++i) {
        const auto& view = views[static_cast<std::size_t>(i)];

        // a identidade e' view.voice, nunca o indice i: o publish compacta os graos
        // activos, e i diz a ordem de chegada. um rastro ligado ao slot saltava de grao a
        // cada bloco.
        grainTrails_.push(view.voice, view.position);
    }

    // no hex a janela segue os graos. ver followGrainsInHex.
    if (toRead > 0 && mode_ == ViewMode::hex) {
        followGrainsInHex();
    }

    // os rastros que nao receberam nada ainda estao a desvanecer; sem repaint ficavam
    // congelados a meio.
    repaint();
}

void ByteDisplay::followGrainsInHex() noexcept {
    if (!grainFollow_ || regionEnd_ <= regionStart_) {
        return;
    }

    // a regiao tem de caber na janela, senao nao ha para onde ir. ancorar no grao mais
    // baixo fazia a janela saltar conforme o agendador sorteia as vozes; ancorar no
    // primeiro endereco da regiao mantem-na quieta e mostra onde o transporte comeca.
    hex_.scrollToAddress(regionStart_);
}

void ByteDisplay::setMode(ViewMode mode) {
    if (mode == mode_) {
        return;
    }

    // a fracao continua a valer nas tres vistas, mas o desenho muda e um rastro antigo
    // sobre o hex novo e' ruido.
    grainTrails_.clear();
    mode_ = mode;

    // o hex e' o unico modo com Component proprio: metricas derivadas da fonte e areas de
    // clique por celula. nos outros o desenho e' um Path por coluna. a faixa de entropia
    // so existe no hex, e so ela mexe na geometria — a excepcao ao resized().
    applyModeVisibility();
    layoutHex();

    repaint();
}

int ByteDisplay::entropyBucket(float bits) noexcept {
    if (bits < 3.0f) {
        return 0;
    }
    return bits < 6.0f ? 1 : 2;
}

juce::Rectangle<int> ByteDisplay::entropyStripBounds() const noexcept {
    if (mode_ != ViewMode::hex) {
        return {};
    }

    const auto full = getLocalBounds();
    const auto stripW = full.getWidth() * 30 / 100;
    if (stripW < 120) {
        return {};
    }
    return {full.getRight() - stripW, full.getY(), stripW, full.getHeight()};
}

void ByteDisplay::layoutHex() {
    const auto strip = entropyStripBounds();
    hex_.setBounds(strip.isEmpty() ? getLocalBounds()
                                   : getLocalBounds().withTrimmedRight(
                                         strip.getWidth() +
                                         static_cast<int>(kPadding)));
}

void ByteDisplay::resized() {
    // o hex ocupa tudo menos a faixa; nas outras vistas e' invisivel mas leva layout, para
    // trocar de vista nao mudar a geometria.
    layoutHex();

    // o numero de colunas depende da largura e do zoom, e a cache esta' indexada por ele.
    // o zoom vive no componente: mudar o tamanho brigaria com o resized do editor.
    const auto wanted = columnCountForWidth();
    if (wanted != cachedColumnCount_) {
        cacheValid_ = false;
    }
}

std::uint32_t ByteDisplay::columnCountForWidth() const noexcept {
    const auto width = getLocalBounds().toFloat().reduced(kPadding).getWidth();
    // uma coluna por pixel: mais e' trabalho que nao aparece, menos e' perda de resolucao.
    // o zoom nao mexe aqui — muda viewByteRange, e a mesma densidade cobre menos material.
    const auto scaled = static_cast<float>(width);
    return static_cast<std::uint32_t>(std::clamp(scaled, 16.0f, 8192.0f));
}

void ByteDisplay::rebuildColumnsIfNeeded() {
    const auto wanted = columnCountForWidth();
    if (cacheValid_ && wanted == cachedColumnCount_ && regionStart_ == cachedRegionStart_ &&
        regionEnd_ == cachedRegionEnd_ && viewStart_ == cachedViewStart_ &&
        viewLength_ == cachedViewLength_) {
        return;
    }

    columns_.clear();

    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        const auto [viewStart, viewEnd] = viewByteRange();
        if (viewEnd > viewStart) {
            static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(),
                                                  viewStart, viewEnd, wanted, columns_));
        }
    }

    cachedColumnCount_ = wanted;
    cachedRegionStart_ = regionStart_;
    cachedRegionEnd_ = regionEnd_;
    cachedViewStart_ = viewStart_;
    cachedViewLength_ = viewLength_;
    cacheValid_ = true;
}

void ByteDisplay::rebuildOverviewIfNeeded() {
    if (overviewValid_ && overviewRegionStart_ == regionStart_ &&
        overviewRegionEnd_ == regionEnd_) {
        return;
    }
    overviewColumns_.clear();
    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        static constexpr std::uint32_t kOverviewColumns {192};
        static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(),
                                              regionStart_, regionEnd_,
                                              kOverviewColumns, overviewColumns_));
    }
    overviewRegionStart_ = regionStart_;
    overviewRegionEnd_ = regionEnd_;
    overviewValid_ = true;
}

std::pair<std::uint64_t, std::uint64_t> ByteDisplay::viewByteRange() const noexcept {
    if (regionEnd_ <= regionStart_) {
        return {regionStart_, regionStart_};
    }
    const auto length = regionEnd_ - regionStart_;
    const auto start = regionStart_ + static_cast<std::uint64_t>(viewStart_ * length);
    const auto end = regionStart_ + static_cast<std::uint64_t>(
                                          std::min(1.0f, viewStart_ + viewLength_) * length);
    return {std::min(start, regionEnd_), std::max(end, std::min(start + 1, regionEnd_))};
}

float ByteDisplay::regionFractionAt(float x, float areaX, float areaWidth) const noexcept {
    if (areaWidth <= 0.0f) {
        return -1.0f;
    }
    const auto screen = (x - areaX) / areaWidth;
    if (screen < 0.0f || screen > 1.0f) {
        return -1.0f;
    }
    return viewStart_ + screen * viewLength_;
}

void ByteDisplay::zoomAround(float factor, float centreFraction) {
    if (regionEnd_ <= regionStart_ || !(factor > 0.0f) ||
        centreFraction < 0.0f || centreFraction > 1.0f) {
        return;
    }
    const auto regionLength = regionEnd_ - regionStart_;
    const auto minLength = std::min(1.0f, static_cast<float>(kMinViewBytes) /
                                             static_cast<float>(regionLength));
    const auto anchored = std::clamp(viewStart_ + centreFraction * viewLength_, 0.0f, 1.0f);
    const auto newLength = std::clamp(viewLength_ * factor, minLength, 1.0f);
    viewStart_ = std::clamp(anchored - centreFraction * newLength, 0.0f, 1.0f - newLength);
    viewLength_ = newLength;
    cacheValid_ = false;
    repaint();
}

void ByteDisplay::zoomIn() {
    zoomAround(0.5f, 0.5f);
}

void ByteDisplay::zoomOut() {
    zoomAround(2.0f, 0.5f);
}

void ByteDisplay::fitView() {
    if (viewStart_ == 0.0f && viewLength_ == 1.0f) {
        return;
    }
    viewStart_ = 0.0f;
    viewLength_ = 1.0f;
    cacheValid_ = false;
    repaint();
}

double ByteDisplay::fractionFor(std::uint64_t address) const noexcept {
    return pe::positionForByte(address, pe::ByteRange {regionStart_, regionEnd_});
}

float ByteDisplay::fractionOf(std::uint64_t address) const noexcept {
    if (viewLength_ <= 0.0f) {
        return -1.0f;
    }
    const auto region = static_cast<float>(fractionFor(address));
    return (region - viewStart_) / viewLength_;
}

void ByteDisplay::setReadHead(std::uint64_t address) {
    readHead_ = address;
    hex_.setReadHead(address);

    // a caret segue a cabeca de leitura ate' o utilizador lhe tocar. sem esta guarda, a
    // volta de 60 Hz do editor repunha-a e as setas nao mexiam em nada.
    if (!caretMoved_) {
        caret_ = address;
    }

    // primeira chamada salta, as seguintes deslizam: animar desde zero ao carregar um
    // ficheiro varria o ecra inteiro.
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
    // com menos de dois bytes nao ha endereco que produza som; o clamp mantem a caret na
    // regiao vazia.
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

    // nas vistas novas nao ha linha; o salto de 16 e' a granularidade do hex e o que um
    // utilizador de teclado espera. um byte de cada vez em 12 MB e' impraticavel.
    constexpr std::uint64_t kStep {1};
    constexpr std::uint64_t kRow {16};

    // pagina = 1024 bytes, a janela que o rodape mede em entropia. ligar o salto a um
    // numero que ja' existe evita um que mudasse com a altura da janela.
    constexpr std::uint64_t kPage {1024};

    const auto upper = highestUsableAddress();
    const auto shift = key.getModifiers().isShiftDown();
    const auto code = key.getKeyCode();
    std::uint64_t target = caret_;
    bool handled = true;

    // if e nao switch: os codigos do KeyPress sao static const int inicializados no .cpp
    // da biblioteca, e `case juce::KeyPress::leftKey` nao e' constante de compilacao.
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
        // ativar e' o que o clique faz. e' a unica tecla que escreve: as setas movem a
        // caret sem mudar o som. depois de ativar, a caret volta a seguir a cabeca.
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

    // o clamp e' a unica proteccao contra transbordo: caret_ - 16 sem ele dava um numero
    // enorme e o editor escrevia uma posicao absurda.
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
        // devolve a caret a cabeca de leitura: desfazer a navegacao sem passar pelo knob.
        setCaret(readHead_);
        return true;
    }

    // as teclas de transporte respondem aqui e nao so no editor: o display e' o unico que
    // fica com foco quando nao ha campo de endereco. o resultado diz se ja' foram
    // consumidas.
    if (handleTransportKey(key)) {
        return true;
    }

    if (isDigitKey(key, '1')) {
        setMode(ViewMode::waveform);
        return true;
    }
    if (isDigitKey(key, '2')) {
        setMode(ViewMode::hex);
        return true;
    }
    if (isDigitKey(key, '3')) {
        setMode(ViewMode::entropy);
        return true;
    }

    if (isTypedCharacter(key, '+') || isTypedCharacter(key, '=')) {
        zoomIn();
        return true;
    }
    if (isTypedCharacter(key, '-')) {
        zoomOut();
        return true;
    }
    if (isTypedCharacter(key, '0')) {
        fitView();
        return true;
    }

    handleNavigationKey(key);
    return true;
}

void ByteDisplay::paintFocusRing(juce::Graphics& g) {
    // 2.4.7 foco visivel. o anel diz que as setas mexem na caret; sem ele as setas
    // parecem nao fazer nada.
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
        g.setFont(font_);
        g.drawText("SEM BINARIO", getLocalBounds().toFloat(), juce::Justification::centred,
                   false);
        return;
    }

    rebuildColumnsIfNeeded();
    rebuildOverviewIfNeeded();

    switch (mode_) {
        case ViewMode::waveform: paintWaveform(g); break;
        case ViewMode::entropy: paintEntropy(g); break;
        case ViewMode::hex: paintEntropyBars(g, entropyStripBounds().toFloat()); break;
    }

    // graos sobre o conteudo e por baixo das cabecas. no hex o HexGrid e' filho e pinta-se
    // depois: e' paintOverChildren que os poe por cima.
    if (mode_ == ViewMode::waveform) {
        paintGrains(g, plotBounds());
    } else if (mode_ == ViewMode::entropy) {
        paintGrains(g, entropyPlotBounds());
    }

    // nas vistas novas nao ha banda da regiao: o que se ve ja' e' a regiao, do primeiro ao
    // ultimo pixel, e marcar o que ocupa tudo seria ruido. no hex a regiao e' faixa
    // porque o que se ve e' o ficheiro inteiro.
    if (mode_ != ViewMode::hex) {
        paintSectionTicks(g);
    }
}

void ByteDisplay::paintEntropyBars(juce::Graphics& g, const juce::Rectangle<float>& strip) {
    if (strip.isEmpty() || columns_.empty()) {
        return;
    }

    // uma barra por grupo de colunas, altura do maximo. maximo e nao media pela mesma
    // razao do contorno do nucleo: o pico e' a leitura.
    // sem glow — ate' 96 barras com halo viram pasta. o glow mora na linha da onda.
    const auto plot =
        strip.reduced(kPadding * 0.5f, 4.0f);
    const int barCount = juce::jlimit(8, 96, static_cast<int>(plot.getWidth() / 3.0f));
    const auto barWidth = plot.getWidth() / static_cast<float>(barCount);

    const juce::Colour inks[3] {palette::waveLow, palette::accentSoft, palette::waveHigh};

    for (int b = 0; b < barCount; ++b) {
        const auto first =
            static_cast<std::size_t>(b) * columns_.size() / static_cast<std::size_t>(barCount);
        const auto last = static_cast<std::size_t>(b + 1) * columns_.size() /
                          static_cast<std::size_t>(barCount);

        float peak = 0.0f;
        for (auto i = first; i < last && i < columns_.size(); ++i) {
            peak = juce::jmax(peak, columns_[i].entropyBits);
        }

        const auto height = std::clamp(peak, 0.0f, 8.0f) / 8.0f * plot.getHeight();
        if (height <= 0.0f) {
            continue;
        }

        const auto x = plot.getX() + static_cast<float>(b) * barWidth;
        g.setColour(palette::alpha(inks[entropyBucket(peak)], 0.85f));
        g.fillRect(juce::Rectangle<float> {x, plot.getBottom() - height,
                                           juce::jmax(1.0f, barWidth - 1.0f), height});
    }
}

void ByteDisplay::paintGrainsInHex(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    // aqui o alvo e' a celula, nao a coluna: no hex interessa qual byte. desenhar a
    // fracao da regiao como x dava uma mancha a meio da linha em vez de uma celula.
    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        const auto cell = hex_.cellRectForAddress(grainTrails_.address(voice));
        if (cell.isEmpty()) {
            continue;
        }

        // sem rastro no hex. a cauda serve quando o grao se mexe no ecra; aqui um grao de
        // 40 ms muda de byte a cada bloco e a cauda marcaria bytes por onde ja' passou,
        // que e' informacao falsa.
        g.setColour(palette::alpha(juce::Colours::white, 0.28f));
        g.fillRect(cell.reduced(1.0f));
        g.setColour(palette::alpha(juce::Colours::white, 0.92f));
        g.drawRect(cell.reduced(1.0f), 1.5f);
    }
}

void ByteDisplay::paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area) {
    // em paint, antes das cabecas (que sao em paintOverChildren). a ordem e' a da leitura:
    // o que responde "o que estou a ouvir" fica por cima.
    // o hex tambem mostra graos — a celula e' onde a coisa acontece, e a conversao passa
    // pelo endereco.
    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        paintGrain(g, area, grainTrails_.trail(voice), 1.0f);
    }
}

void ByteDisplay::paintGrain(juce::Graphics& g,
                             const juce::Rectangle<float>& area,
                             const view::GrainTrailSet::Trail& trail,
                             float gain) {
    // so para a forma de onda e a curva: nelas a janela e' a regiao e a fracao do grao e'
    // a posicao no ecra. no hex a escala e' outra; ver paintGrainsInHex.
    const auto xOf = [this, &area](float regionFraction) {
        if (viewLength_ <= 0.0f) {
            return -1.0f;
        }
        const auto screen = (regionFraction - viewStart_) / viewLength_;
        if (screen <= 0.0f || screen >= 1.0f) {
            return -1.0f; // fora do viewport: nao desenhar em vez de adivinhar
        }
        return area.getX() + screen * area.getWidth();
    };

    // indice 0 = mais recente; o deslocamento empurra para o fim.
    const auto newestX = xOf(trail.positions[0]);
    if (newestX < 0.0f) {
        return;
    }

    // risco baixo, altura a dar o ganho, nunca a toda a altura. a toda a altura o grao
    // ficava indistinto da cabeca de leitura — duas barras brancas iguais. o 1.4.1 e'
    // sobre forma, nao so cor.
    const auto centreY = area.getCentreY();
    const auto height = juce::jmax(6.0f, area.getHeight() * 0.30f * gain);
    const auto top = centreY - height * 0.5f;

    float oldestX = newestX;
    for (int f = 1; f < trail.count; ++f) {
        const auto x = xOf(trail.positions[static_cast<std::size_t>(f)]);
        if (x >= 0.0f) {
            oldestX = std::min(oldestX, x);
        }
    }

    // o rastro e' uma pilha de retangulos com alfa crescente, nao um gradiente: o JUCE
    // quantiza a 8 bits e num curso de seis pixels cada degrau de 1/255 vira banda.
    const auto span = newestX - oldestX;
    if (trail.count > 1 && span > 0.5f) {
        // do mais antigo para o mais recente; o alfa cresce perto do grao actual e da a
        // direccao do varrimento sem seta nem legenda.
        constexpr int kSteps {6};
        const auto step = juce::jmax(1.0f, std::ceil(span / static_cast<float>(kSteps)));
        for (int s = 0; s < kSteps; ++s) {
            const auto t = static_cast<float>(s + 1) / static_cast<float>(kSteps);
            const auto x0 = oldestX + step * static_cast<float>(s);
            if (x0 >= newestX) {
                break;
            }
            const auto width = juce::jmin(step, newestX - x0);
            g.setColour(palette::alpha(juce::Colours::white, 0.42f * gain * t * t));
            g.fillRect(juce::Rectangle<float> {x0, top, width, height});
        }
    }

    // cabeca do rastro: o ponto mais recente, opaco. e' a unica peca que nao pode faltar.
    // branco e nao a cor do grao: sobre o envelope accent o grao laranja ficava em 2,9:1,
    // abaixo dos 3:1 do 1.4.11. branco da 4,52:1 sobre o envelope e 13,4:1 sobre o fundo.
    g.setColour(palette::alpha(juce::Colours::white, 0.14f * gain));
    g.fillRoundedRectangle(juce::Rectangle<float> {newestX - kGrainGlowWidth * 0.5f,
                                                   top - 1.5f, kGrainGlowWidth,
                                                   height + 3.0f},
                           kGrainGlowWidth * 0.5f);
    g.setColour(palette::alpha(juce::Colours::white, 0.9f * gain));
    g.fillRoundedRectangle(juce::Rectangle<float> {newestX - kGrainCoreWidth * 0.5f, top,
                                                   kGrainCoreWidth, height},
                           kGrainCoreWidth * 0.5f);
}

void ByteDisplay::paintSectionTicks(juce::Graphics& g) {
    // marcas de limites de secao sobre a curva e a forma de onda. marcas e nao bandas: a
    // regiao ja' ocupa o ecra, e o filete de 1 px e' sinal sem cor (1.4.1). o nome fica
    // no gutter do hex; aqui interessa onde a secao acaba.
    if (sections_.empty() || regionEnd_ <= regionStart_) {
        return;
    }

    const auto area = plotBounds();
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

        const auto screen = fractionOf(start);
        if (screen < 0.0f || screen > 1.0f) {
            continue; // fora do viewport
        }
        const auto x = area.getX() + screen * area.getWidth();

        g.setColour(palette::alpha(palette::chassisBorder, 0.8f));
        g.fillRect(juce::Rectangle<float> {x, area.getY(), 1.0f, area.getHeight()});
    }
}

void ByteDisplay::paintOverChildren(juce::Graphics& g) {
    if (bytes_ == nullptr || bytes_->empty()) {
        return;
    }

    // nas vistas novas as cabecas vivem no plot; no hex ocupam a area toda. na entropia o
    // plot recua para o eixo, como a curva.
    juce::Rectangle<float> area = getLocalBounds().toFloat().reduced(kPadding);
    if (mode_ == ViewMode::waveform) {
        area = plotBounds();
    } else if (mode_ == ViewMode::entropy) {
        area = entropyPlotBounds();
    }

    // no hex os graos entram aqui e nao em paint: o HexGrid e' filho e pinta-se depois do
    // pai, por isso uma marca em paint ficava por baixo da grelha. o alvo e' a celula.
    if (mode_ == ViewMode::hex) {
        paintGrainsInHex(g);
    }

    // no hex a cabeca de leitura e' do HexGrid, que a desenha na celula — ali e' linha e
    // coluna, nao fracao da largura.
    if (mode_ != ViewMode::hex) {
        paintReadHead(g, area);
    }

    // a cabeca de reproducao aparece nas tres, hex incluido: e' a unica peca que o
    // HexGrid nao conhece, e sem ela o PLAY nao mostrava nada na vista dos bytes.
    paintPlayhead(g, area);

    // a caret so com foco de teclado. sem foco seria mais uma barra vertical entre a
    // cabeca de leitura e a de reproducao, e tres iguais nao se distinguem.
    if (hasKeyboardFocus(true) && mode_ != ViewMode::hex) {
        const auto screen = fractionOf(caret_);
        if (screen >= 0.0f && screen <= 1.0f) {
            const auto caretX = area.getX() + screen * area.getWidth();
            g.setColour(palette::accentSoft);
            g.drawRect(juce::Rectangle<float> {caretX - 3.0f, area.getY() + 2.0f, 6.0f,
                                               area.getHeight() - 4.0f},
                       1.0f);
        }
    }

    paintFocusRing(g);

    paintOutputLevel(g);
}

void ByteDisplay::paintOutputLevel(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(kPadding);
    const auto barHeight = kLevelHeight;

    // no topo do display, nao em baixo: o rodape e' do editor e ja' tem a telemetria
    // escrita. aqui em cima responde ao que se ouve sem competir.
    const auto bar = juce::Rectangle<float> {bounds.getX(), bounds.getY() + 2.0f,
                                             bounds.getWidth(), barHeight};

    // a calha fica sempre. sem ela o medidor desaparece com o sinal e nao se sabe se
    // esta' a trabalhar.
    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.fillRect(bar);

    const auto level = outputLevel_.value();
    const auto filled = bar.withWidth(bar.getWidth() * level);
    if (filled.getWidth() > 0.0f) {
        // a cor muda perto do topo: o medidor tem de avisar antes do limitador meter. por
        // isso o valor em texto no rodape importa a quem nao distingue verde de laranja.
        const auto hot = level > 0.85f;
        g.setColour(palette::alpha(hot ? palette::accentWarn : palette::okBright, 0.9f));
        g.fillRect(filled);
    }
}

void ByteDisplay::paintWaveform(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto plot = plotBounds();
    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f) {
        return;
    }

    paintRuler(g, {full.getX(), plot.getY() - kRulerHeight - 2.0f, full.getWidth(),
                   kRulerHeight});

    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.5f;
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto columnWidth = plot.getWidth() / static_cast<float>(columns_.size());

    if (sampleDotsVisible()) {
        // zoom fundo: os bytes reais em vez da miniatura. o transiente le-se amostra a
        // amostra, sem esticar nada.
        paintSampleDots(g, plot, viewStartByte, viewEndByte);
    } else {
        // minimo e maximo num Path so, dois movimentos: subir pela coluna maxima, descer
        // pela minima. um Path por coluna custava sete vezes mais e nao mudava o desenho.
        juce::Path swing;
        swing.startNewSubPath(plot.getX(), centreY);

        for (std::size_t i = 0; i < columns_.size(); ++i) {
            const auto x = plot.getX() + static_cast<float>(i) * columnWidth;
            const auto top = centreY - std::clamp(columns_[i].maximum, -1.0f, 1.0f) * halfHeight;
            swing.lineTo(x, top);
        }

        for (std::size_t i = columns_.size(); i-- > 0;) {
            const auto x = plot.getX() + static_cast<float>(i) * columnWidth;
            const auto bottom =
                centreY - std::clamp(columns_[i].minimum, -1.0f, 1.0f) * halfHeight;
            swing.lineTo(x, bottom);
        }

        swing.closeSubPath();

        // base morna fraca: o envelope diz ate' onde o material se estica, e nada mais. o
        // corpo solido de 870 verticais de altura cheia fundia-se numa massa sem vales.
        g.setColour(palette::alpha(palette::accent, 0.10f));
        g.fillPath(swing);

        // silhueta do rms: so as arestas, interior vazio. cada aresta colorida por
        // entropia — teal no previsivel, amarelo no meio, rosa no aleatorio. a linha conta
        // duas leituras: onde o som esta' e do que o material e' feito.
        paintRmsSilhouette(g, plot, centreY, halfHeight, columnWidth);
    }

    // o zero. sem ele o envelope nao diz qual e' o silencio.
    g.setColour(palette::alpha(juce::Colours::white, 0.16f));
    g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());

    paintSourceTag(g, plot);
    paintOverview(g);
}

bool ByteDisplay::sampleDotsVisible() const noexcept {
    if (regionEnd_ <= regionStart_ || bytes_ == nullptr) {
        return false;
    }
    const auto plotWidth = plotBounds().getWidth();
    if (plotWidth <= 0.0f) {
        return false;
    }
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes =
        viewEndByte > viewStartByte ? (viewEndByte - viewStartByte) : 0;
    // <=2 bytes por pixel: ha ecra para cada amostra. acima disso os pontos colapsam e a
    // polilinha das colunas conta melhor.
    return viewBytes > 0 &&
           static_cast<float>(viewBytes) / plotWidth <= 2.0f;
}

void ByteDisplay::paintSampleDots(juce::Graphics& g,
                                  const juce::Rectangle<float>& plotArea,
                                  std::uint64_t viewStartByte,
                                  std::uint64_t viewEndByte) {
    if (bytes_ == nullptr || viewEndByte <= viewStartByte ||
        viewStartByte >= bytes_->size()) {
        return;
    }
    const auto last = std::min(viewEndByte, static_cast<std::uint64_t>(bytes_->size()));
    const auto count = last - viewStartByte;
    if (count == 0 || count > 8192) {
        return;
    }

    const auto centreY = plotArea.getCentreY();
    const auto halfHeight = plotArea.getHeight() * 0.5f;
    const auto xOf = [&](std::uint64_t byte) {
        return plotArea.getX() +
               (static_cast<float>(byte - viewStartByte) + 0.5f) /
                   static_cast<float>(count) * plotArea.getWidth();
    };
    const auto yOf = [&](std::uint8_t byte) {
        return centreY -
               ((static_cast<float>(byte) - 127.5f) / 127.5f) * halfHeight;
    };

    // a mesma conta do nucleo (byteToSample duplicada e presa por ensaio): o ponto e' o
    // byte, sem agregacao pelo meio.
    juce::Path line;
    bool started = false;
    for (auto byte = viewStartByte; byte < last; ++byte) {
        const auto x = xOf(byte);
        const auto y = yOf((*bytes_)[static_cast<std::size_t>(byte)]);
        if (!started) {
            line.startNewSubPath(x, y);
            started = true;
        } else {
            line.lineTo(x, y);
        }
    }
    g.setColour(palette::waveLow);
    g.strokePath(line, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded});

    g.setColour(palette::alpha(palette::waveLow, 0.95f));
    for (auto byte = viewStartByte; byte < last; ++byte) {
        const auto x = xOf(byte);
        const auto y = yOf((*bytes_)[static_cast<std::size_t>(byte)]);
        g.fillEllipse(x - 1.5f, y - 1.5f, 3.0f, 3.0f);
    }
}

void ByteDisplay::paintRuler(juce::Graphics& g, const juce::Rectangle<float>& strip) {
    if (regionEnd_ <= regionStart_ || strip.getWidth() <= 0.0f) {
        return;
    }
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes = viewEndByte > viewStartByte ? viewEndByte - viewStartByte : 0;
    if (viewBytes == 0) {
        return;
    }

    // passo 1/2/5 x 10^n para ~90 px por tick maior.
    const auto pxPerByte = strip.getWidth() / static_cast<float>(viewBytes);
    const auto target = 90.0 / static_cast<double>(pxPerByte);
    const auto magnitude = std::pow(10.0, std::floor(std::log10(std::max(target, 1.0))));
    std::uint64_t stepBytes {1};
    for (const auto mult : {1.0, 2.0, 5.0, 10.0}) {
        if (mult * magnitude >= target) {
            stepBytes = static_cast<std::uint64_t>(mult * magnitude);
            break;
        }
        stepBytes = static_cast<std::uint64_t>(10.0 * magnitude);
    }
    if (stepBytes == 0) {
        return;
    }

    // hex de 64 bits a mao: String::toHexString trunca em 32 bits e um ficheiro passa
    // disso aos 4 GB.
    const auto hexOf = [](std::uint64_t value) {
        constexpr char digits[] {"0123456789ABCDEF"};
        juce::String text {"0x"};
        bool started = false;
        for (int shift = 60; shift >= 0; shift -= 4) {
            const auto nibble = (value >> shift) & 0xFu;
            if (nibble != 0 || started || shift == 0) {
                text += digits[nibble];
                started = true;
            }
        }
        return text;
    };

    g.setFont(fonts().sans(9.0f));
    const auto firstTick = (viewStartByte + stepBytes - 1) / stepBytes * stepBytes;
    for (auto tick = firstTick; tick < viewEndByte;) {
        const auto x = strip.getX() +
                       static_cast<float>(tick - viewStartByte) * pxPerByte;
        g.setColour(palette::alpha(palette::chassisBorder, 0.9f));
        g.fillRect(juce::Rectangle<float> {x, strip.getBottom() - 5.0f, 1.0f, 5.0f});
        g.setColour(palette::textOnDarkSub);
        g.drawText(hexOf(tick),
                   juce::Rectangle<float> {x + 3.0f, strip.getY(), 76.0f,
                                            strip.getHeight()},
                   juce::Justification::centredLeft, false);
        if (tick > viewEndByte - stepBytes) {
            break; // sem isto o ultimo passo voltava por transbordo
        }
        tick += stepBytes;
    }

    // linha de base da regua.
    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.drawHorizontalLine(juce::roundToInt(strip.getBottom()), strip.getX(),
                         strip.getRight());
}

void ByteDisplay::paintSourceTag(juce::Graphics& g,
                                 const juce::Rectangle<float>& plotArea) {
    // o que se ve nao sao bytes: e' o mapeamento bytes->amostras. sem o rotulo a forma de
    // onda apresenta bytes crus como se fossem audio; os bytes crus moram no hex.
    const juce::String tag {mode_ == ViewMode::waveform ? "MAPPED AUDIO · BYTES->SAMPLES"
                                                        : "MAPPED AUDIO · ENTROPY"};
    g.setFont(fonts().sans(9.0f));
    const auto width = 218.0f;
    // em baixo a esquerda: em cima colide com os rotulos da regua.
    const auto tagBounds = juce::Rectangle<float> {plotArea.getX() + 4.0f,
                                                 plotArea.getBottom() - 18.0f, width,
                                                  14.0f};
    g.setColour(palette::alpha(palette::display, 0.72f));
    g.fillRoundedRectangle(tagBounds, 2.0f);
    g.setColour(palette::textOnDarkSub);
    g.drawText(tag, tagBounds.reduced(6.0f, 0.0f), juce::Justification::centredLeft,
               false);
}

void ByteDisplay::paintOverview(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_ || overviewColumns_.empty()) {
        return;
    }
    const auto strip = overviewBounds();
    if (strip.getWidth() <= 0.0f || strip.getHeight() <= 0.0f) {
        return;
    }

    // calha afundada.
    g.setColour(palette::display);
    g.fillRoundedRectangle(strip, 2.0f);
    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.drawRoundedRectangle(strip, 2.0f, 1.0f);

    const auto plot = strip.reduced(3.0f, 3.0f);
    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.5f;
    const auto columnWidth = plot.getWidth() / static_cast<float>(overviewColumns_.size());

    // miniatura quieta: um envelope, sem cor por entropia e sem glow. o mapa e' contexto;
    // a leitura mora no plot.
    juce::Path mini;
    mini.startNewSubPath(plot.getX(), centreY);
    for (std::size_t i = 0; i < overviewColumns_.size(); ++i) {
        mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                    centreY - std::clamp(overviewColumns_[i].rms, 0.0f, 1.0f) *
                                  halfHeight);
    }
    for (std::size_t i = overviewColumns_.size(); i-- > 0;) {
        mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                    centreY + std::clamp(overviewColumns_[i].rms, 0.0f, 1.0f) *
                                  halfHeight);
    }
    mini.closeSubPath();
    g.setColour(palette::alpha(palette::waveLow, 0.45f));
    g.fillPath(mini);

    // fora do viewport escurece; o viewport leva moldura ambar. forma e posicao contam sem
    // cor — o 1.4.1 vale no mapa.
    const auto viewX = plot.getX() + viewStart_ * plot.getWidth();
    const auto viewW = std::max(3.0f, viewLength_ * plot.getWidth());
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    if (viewX > plot.getX()) {
        g.fillRect(
            juce::Rectangle<float> {plot.getX(), plot.getY(), viewX - plot.getX(),
                                    plot.getHeight()});
    }
    if (viewX + viewW < plot.getRight()) {
        g.fillRect(juce::Rectangle<float> {viewX + viewW, plot.getY(),
                                            plot.getRight() - (viewX + viewW),
                                            plot.getHeight()});
    }
    g.setColour(palette::accent);
    g.drawRect(juce::Rectangle<float> {viewX, plot.getY(), viewW, plot.getHeight()},
               1.0f);

    // cabecas no mapa: leitura branca, reproducao ambar com triangulo. em fracao da regiao
    // (fractionFor), nao do ecra: o mapa mostra a regiao inteira e o viewport e' o
    // retangulo.
    const auto readRegion = static_cast<float>(fractionFor(readHead_));
    if (readRegion >= 0.0f && readRegion <= 1.0f) {
        const auto x = plot.getX() + readRegion * plot.getWidth();
        g.setColour(juce::Colours::white);
        g.fillRect(juce::Rectangle<float> {x - 0.5f, plot.getY(), 1.0f,
                                            plot.getHeight()});
    }
    if (playheadVisible_) {
        const auto region = std::clamp(playheadFraction_.value(), 0.0f, 1.0f);
        const auto x = plot.getX() + region * plot.getWidth();
        g.setColour(palette::accent);
        g.fillRect(juce::Rectangle<float> {x - 0.5f, plot.getY(), 1.0f,
                                            plot.getHeight()});
    }
}

// linha colorida por entropia: usada pela forma de onda (sobre o rms, espelhado no
// centro) e pela curva (sem espelho). o dado e' columns_[i].entropyBits, escala 0-8.
// topOf da' o y da coluna; bottomOf devolve o mesmo quando nao ha espelho.
void ByteDisplay::paintEntropyLine(
    juce::Graphics& g,
    const std::vector<pe::Column>& columns,
    float columnWidth,
    float originX,
    const std::function<float(std::size_t)>& topOf,
    const std::function<float(std::size_t)>& bottomOf) {
    juce::Path low;
    juce::Path mid;
    juce::Path high;

    // ultima coluna que entrou em cada faixa. sem isto, duas colunas da mesma faixa
    // separadas por outra saiam ligadas por uma diagonal atravessada.
    std::size_t lastInBucket[3] {columns.size(), columns.size(), columns.size()};

    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto x = originX + static_cast<float>(i) * columnWidth;
        const auto b = entropyBucket(columns[i].entropyBits);

        auto& path = b == 0 ? low : b == 1 ? mid : high;
        if (path.isEmpty() || lastInBucket[b] + 1 != i) {
            path.startNewSubPath(x, topOf(i));
        } else {
            path.lineTo(x, topOf(i));
        }
        path.lineTo(x, bottomOf(i));
        lastInBucket[b] = i;
    }

    const juce::Colour inks[3] {palette::waveLow, palette::accentSoft, palette::waveHigh};
    const juce::Path* paths[3] {&low, &mid, &high};
    for (int b = 0; b < 3; ++b) {
        if (paths[b]->isEmpty()) {
            continue;
        }
        // glow contido: um passe de 3 px e fraco. dois passes largos somavam cobertura a
        // cada coluna vizinha e o contorno virava massa opaca.
        g.setColour(palette::alpha(inks[b], 0.20f));
        g.strokePath(*paths[b], juce::PathStrokeType {3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
        g.setColour(palette::alpha(inks[b], 0.95f));
        g.strokePath(*paths[b], juce::PathStrokeType {1.0f});
    }
}

void ByteDisplay::paintRmsSilhouette(juce::Graphics& g,
                                      const juce::Rectangle<float>& plotArea,
                                      float centreY,
                                      float halfHeight,
                                      float columnWidth) {
    const juce::Colour inks[3] {palette::waveLow, palette::accentSoft, palette::waveHigh};

    // uma passagem por aresta, ambas da esquerda para a direita. juntar as duas numa faixa
    // fechada pedia guardar os pontos de baixo — vector no paint, a 60 Hz.
    // started e nao Path::isEmpty(): isEmpty ignora move markers, um subpath com so o
    // move conta como vazio e o lineTo nunca corria — sem erro, sem warning, sem pixel.
    for (int edge = 0; edge < 2; ++edge) {
        juce::Path band[3];
        bool started[3] {false, false, false};
        std::size_t lastInBucket[3] {0, 0, 0};

        for (std::size_t i = 0; i < columns_.size(); ++i) {
            const auto x = plotArea.getX() + static_cast<float>(i) * columnWidth;
            const auto rms = std::clamp(columns_[i].rms, 0.0f, 1.0f) * halfHeight;
            const auto y = edge == 0 ? centreY - rms : centreY + rms;
            const auto b = entropyBucket(columns_[i].entropyBits);

            if (!started[b] || lastInBucket[b] + 1 != i) {
                band[b].startNewSubPath(x, y);
                started[b] = true;
            } else {
                band[b].lineTo(x, y);
            }
            lastInBucket[b] = i;
        }

        for (int b = 0; b < 3; ++b) {
            if (!started[b]) {
                continue;
            }
            g.setColour(palette::alpha(inks[b], 0.18f));
            g.strokePath(band[b], juce::PathStrokeType {3.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded});
            g.setColour(palette::alpha(inks[b], 0.95f));
            g.strokePath(band[b], juce::PathStrokeType {1.0f});
        }
    }
}

void ByteDisplay::paintEntropy(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto area = plotBounds();
    if (area.getWidth() <= 0.0f || area.getHeight() <= 0.0f) {
        return;
    }
    const auto plot = area.withTrimmedLeft(kAxisInset).withTrimmedBottom(kAxisHeight);

    paintRuler(g, {full.getX(), area.getY() - kRulerHeight - 2.0f, full.getWidth(),
                   kRulerHeight});

    // 0 a 8 bits por byte, com os tres valores no eixo. sem numeros nao se sabe se 7,5 e'
    // um .exe normal ou 3,5 um repositorio de bytes iguais.
    const auto yFor = [&plot](float bits) {
        return plot.getBottom() - std::clamp(bits, 0.0f, 8.0f) / 8.0f * plot.getHeight();
    };

    g.setColour(palette::alpha(palette::displayGrid, 1.4f));
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawHorizontalLine(juce::roundToInt(yFor(bits)), plot.getX(), plot.getRight());
    }

    g.setFont(font_);
    g.setColour(palette::textOnDarkSub);
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawText(juce::String::formatted("%.0f", static_cast<double>(bits)),
                   juce::Rectangle<float> {area.getX(), yFor(bits) - 7.0f, kAxisInset - 6.0f,
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

    // a linha por cima, colorida pela propria entropia que desenha: a curva que sobe
    // para o rosa diz "aqui e' aleatorio" sem eixo. sem espelho, o segundo lambda repete
    // o primeiro.
    const auto yAt = [&](std::size_t i) { return yFor(columns_[i].entropyBits); };
    paintEntropyLine(g, columns_, columnWidth, plot.getX(), yAt, yAt);

    paintSourceTag(g, area);
    paintOverview(g);
}

void ByteDisplay::paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    const auto fraction = readHeadFraction_.value();
    if (fraction < 0.0f || fraction > 1.0f) {
        return;
    }

    const auto x = area.getX() + fraction * area.getWidth();

    // barra de 2 px a toda a altura com realce por baixo. o realce sem a barra e' uma
    // celula a tremer; a barra sem realce desaparece sobre um envelope de 0xFF.
    g.setColour(palette::alpha(juce::Colours::white, 0.16f));
    g.fillRect(juce::Rectangle<float> {x - 2.0f, area.getY(), 4.0f, area.getHeight()});
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float> {x - 1.0f, area.getY(), 2.0f, area.getHeight()});
}

void ByteDisplay::paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    if (!playheadVisible_ || columns_.empty()) {
        return;
    }

    // fracao da regiao -> ecra. fora do viewport nao se prende na borda: prender mentia
    // sobre onde o som esta'.
    const auto region = std::clamp(playheadFraction_.value(), 0.0f, 1.0f);
    const auto screen = viewLength_ > 0.0f ? (region - viewStart_) / viewLength_ : -1.0f;
    if (screen < 0.0f || screen > 1.0f) {
        return;
    }
    const auto x = area.getX() + screen * area.getWidth();

    // triangulo em cima, barra em baixo. a barra e' o mesmo sinal e a mesma forma da
    // cabeca de leitura, e os dois confundem-se.
    // o triangulo distingue: e' a unica forma diferente no display e diz "isto anda
    // sozinho" sem legenda. o 1.4.1 pede padrao alem da cor.
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

    const auto area = mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
    if (bytes_ == nullptr || area.getWidth() <= 0.0f || regionEnd_ <= regionStart_) {
        return none;
    }

    const auto region = regionFractionAt(position.x, area.getX(), area.getWidth());
    if (region < 0.0f || region >= 1.0f) {
        return none;
    }

    // a fracao 1,0 e' silencio no motor, por isso o ultimo endereco util e' end - 2.
    // mesma regra de pe::positionForByte; o inverso e' byteForPosition.
    const auto address = pe::byteForPosition(static_cast<double>(region),
                                             pe::ByteRange {regionStart_, regionEnd_});
    return address < regionEnd_ ? address : none;
}

juce::Rectangle<float> ByteDisplay::plotBounds() const noexcept {
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto overviewH = overviewBounds().getHeight();
    // medidor, respiro e regua em cima; overview e respiro em baixo. onda, graos e
    // cabecas vivem todos nesta area, senao a cabeca marcava um x e a onda outro.
    return full.withTrimmedTop(kLevelHeight + 5.0f + kRulerHeight)
        .withTrimmedBottom(overviewH + 4.0f);
}

juce::Rectangle<float> ByteDisplay::entropyPlotBounds() const noexcept {
    return plotBounds().withTrimmedLeft(kAxisInset);
}

juce::Rectangle<float> ByteDisplay::overviewBounds() const noexcept {
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto height = std::min(kOverviewHeight, full.getHeight() * 0.25f);
    return {full.getX(), full.getBottom() - height, full.getWidth(), height};
}

void ByteDisplay::mouseDown(const juce::MouseEvent& event) {
    // o clique da' o foco do teclado ao display. sem isto, clicar e depois carregar em
    // espaco nao respondia, e a pista era o anel de foco que so aparece depois de um Tab.
    grabKeyboardFocus();

    if (mode_ == ViewMode::hex) {
        return; // o HexGrid trata do seu.
    }

    // overview: o clique centra o viewport, o arrasto move-o. nao varre a leitura —
    // quem clica no mapa quer navegar.
    if (mode_ != ViewMode::hex && overviewBounds().contains(event.position)) {
        const auto area = overviewBounds();
        const auto region = regionFractionAt(event.position.x, area.getX(), area.getWidth());
        if (region >= 0.0f) {
            viewStart_ = std::clamp(region - viewLength_ * 0.5f, 0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
        overviewDrag_ = true;
        lastPanX_ = event.position.x;
        return;
    }

    // pan: botao direito, medio ou Ctrl+clique. o arrasto horizontal move o viewport;
    // sem zoom nao ha para onde ir.
    if (event.mods.isRightButtonDown() || event.mods.isMiddleButtonDown() ||
        event.mods.isCtrlDown()) {
        panning_ = true;
        lastPanX_ = event.position.x;
        return;
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onAddressActivated != nullptr) {
        caretMoved_ = false;
        onAddressActivated(address);
    }
}

void ByteDisplay::mouseDrag(const juce::MouseEvent& event) {
    // varre: o mesmo caminho do clique, sem pressa. o rato chega a mais de 60 Hz e cada
    // escrita notifica o host, por isso o endereco fica guardado e o tick descarrega um
    // por quadro. no hex o arrasto e' do HexGrid.
    if (mode_ == ViewMode::hex) {
        return;
    }

    // pan pelo mapa ou pelo plot: dx em pixels vira fracao do viewport.
    if (overviewDrag_ || panning_) {
        const auto area = overviewDrag_ ? overviewBounds()
                          : mode_ == ViewMode::entropy ? entropyPlotBounds()
                                                      : plotBounds();
        if (area.getWidth() > 0.0f && viewLength_ < 1.0f) {
            const auto dx = (event.position.x - lastPanX_) / area.getWidth();
            lastPanX_ = event.position.x;
            viewStart_ = std::clamp(viewStart_ - dx * viewLength_, 0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
        return;
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max()) {
        pendingScrub_ = address;
    }
}

void ByteDisplay::mouseUp(const juce::MouseEvent&) {
    panning_ = false;
    overviewDrag_ = false;
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
    // nas vistas novas a roda e' zoom continuo centrado no cursor: a fracao sob o cursor
    // fica quieta e o resto aproxima-se. Shift+roda faz scroll horizontal.
    if (mode_ != ViewMode::hex) {
        const auto area =
            mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
        if (event.mods.isShiftDown()) {
            if (viewLength_ < 1.0f && area.getWidth() > 0.0f) {
                viewStart_ = std::clamp(viewStart_ - wheel.deltaY * viewLength_ * 2.0f,
                                        0.0f, 1.0f - viewLength_);
                cacheValid_ = false;
                repaint();
            }
        } else {
            const auto centre = regionFractionAt(event.position.x, area.getX(),
                                                 area.getWidth());
            const auto inPlot = centre >= 0.0f;
            const auto delta = static_cast<float>(wheel.deltaY);
            // roda para cima (deltaY > 0) aproxima: o comprimento cai.
            zoomAround(std::pow(1.15f, -delta),
                       inPlot ? (centre - viewStart_) / viewLength_ : 0.5f);
        }
    }

    // no hex a roda e' do HexGrid, que faz scroll por linhas e tem o accumulator do
    // entalhe; um delta aqui virava duas rolagens por entalhe.
    static_cast<void>(event);
}

} // namespace opcoda