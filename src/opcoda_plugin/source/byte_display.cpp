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

    font_ = fonts().sans(10.0f);

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

    // **O primeiro gesto do utilizador na grelha corta o seguimento dos graos.**
    // A janela do hex segue o material enquanto ninguem mexe, e isso e' o que torna
    // os graos visiveis, mas quem esta' a ler bytes nao pode ter o ecra a mexer
    // sozinho. A partir da rolagem ou do clique, quem manda e' a pessoa.
    hex_.onUserScrolled = [this] { grainFollow_ = false; };

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

    // **A regiao mudou, portanto a fracao mudou de significado.** Um grao que
    // estava a ler o fim da regiao antiga tem fracao 0,9, e 0,9 na regiao nova e'
    // um sitio completamente diferente. Sem esvaziar, o rastro velho aparecia
    // sobre o material novo e parecia um grao a dar um salto.
    grainTrails_.clear();
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

void ByteDisplay::setGrains(
    const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views, int count) {
    // As posicoes sao fracoes **da regiao**, e a regiao e' a mesma nas tres vistas.
    // No hex a fracao e' convertida em endereco dentro do `push`, porque o hex mostra
    // o ficheiro inteiro e nao a regiao.
    //
    // So se guarda o que interessa. Um display sem material nao tem regiao para ser
    // interpreted, entao um rastro de graos sobre o ecra vazio seria ruido.
    const bool drawable = bytes_ != nullptr && !bytes_->empty() &&
                          regionEnd_ > regionStart_;
    const auto toRead = drawable ? std::clamp(count, 0, dsp::GrainTelemetry::kMaxVoices) : 0;

    for (int i = 0; i < toRead; ++i) {
        const auto& view = views[static_cast<std::size_t>(i)];

        // **A identidade e' `view.voice`, nunca o indice `i`.** O publish do motor
        // compacta os graos activos para a frente, portanto `i` diz em que ordem
        // apareceram e nao que grao e'. A cauda da voz 3 e' a cauda da voz 3, e um
        // rastro ligado ao slot saltaria de grao a cada bloco.
        grainTrails_.push(view.voice, view.position);
    }

    // No hex, a janela segue os graos. Ver followGrainsInHex para o porque e para o
    // que custa.
    if (toRead > 0 && mode_ == ViewMode::hex) {
        followGrainsInHex();
    }

    // Repintar: os rastros que existiam e nao receberam nada hoje ainda estao a
    // desvanecer, e sem este repaint eles ficariam congelados a meio do ecra.
    repaint();
}

void ByteDisplay::followGrainsInHex() noexcept {
    if (!grainFollow_ || regionEnd_ <= regionStart_) {
        return;
    }

    // **A regiao tem de caber na janela, senao nao ha para onde ir.** A janela sao
    // `metrics_.rows` linhas e a regiao pode ter dezenas de milhares. Ancorar no
    // grao mais baixo faz a janela saltar de baixo para cima a cada bloco conforme
    // o agendador sorteia as vozes, e um ecra que treme e pior do que um ecra sem
    // graos.
    //
    // Ancorar no **primeiro endereco da regiao** mantem a janela quieta: ela fica
    // onde a regiao comeca e so se mexe quando o utilizador rola. E o que torna
    // visivel o inicio do material, que e' onde o transporte comeca.
    hex_.scrollToAddress(regionStart_);
}

void ByteDisplay::setMode(ViewMode mode) {
    if (mode == mode_) {
        return;
    }

    // A fracao da regiao continua a valer nas tres vistas, mas o desenho muda e
    // um rastro antigo em cima do hex novo e' ruido sem informacao.
    grainTrails_.clear();
    mode_ = mode;

    // O hex e' o unico modo com um Component proprio, porque o seu desenho tem
    // metricas derivadas da fonte e areas de clique por celula. Nos outros dois o
    // desenho e' um Path por coluna e nao ha filhos.
    applyModeVisibility();

    repaint();
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
        g.setFont(font_);
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

    // Os graos vao sobre o conteudo e por baixo das cabecas. No hex o HexGrid e'
    // um Component filho e pinta-se a si proprio, portanto `paint` acontece antes
    // dele: e' `paintOverChildren` que os coloca por cima.
    if (mode_ != ViewMode::hex) {
        paintGrains(g, getLocalBounds().toFloat().reduced(kPadding));
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

void ByteDisplay::paintGrainsInHex(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    // **Aqui o alvo e' a celula, e nao a coluna.** Um grao desenha-se sobre a celula
    // do byte que esta' a ler, porque no hex o que interessa e' *qual byte*. Desenhar
    // a fracao da regiao como se fosse a posicao no ecra seria uma mentira
    // geometrica, e a captura mostrava uma mancha desfocada a meio da linha em vez
    // de uma celula.
    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        const auto cell = hex_.cellRectForAddress(grainTrails_.address(voice));
        if (cell.isEmpty()) {
            continue;
        }

        // **Sem rastro no hex.** A cauda serve quando o grao se mexe no ecra, e no
        // hex um grao de 40 ms muda de byte a cada bloco: a cauda ocuparia tres ou
        // quatro celulas e marcaria bytes por onde o grao ja passou, que e'
        // informacao falsa — diria que o grao esta' a ler onde ja nao esta'. A
        // forma de onda e' que mostra a varredura; o hex mostra o byte.
        g.setColour(palette::alpha(juce::Colours::white, 0.28f));
        g.fillRect(cell.reduced(1.0f));
        g.setColour(palette::alpha(juce::Colours::white, 0.92f));
        g.drawRect(cell.reduced(1.0f), 1.5f);
    }
}

void ByteDisplay::paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area) {
    // Desenhados em `paint`, antes da cabeca de leitura e da de reproducao, que
    // sao em `paintOverChildren`. A ordem e' a da leitura: o que responde a "o que
    // estou a ouvir" fica por cima, e o grao e' contexto.
    //
    // **A celula do hex e' o sitio onde a coisa esta' a acontecer**, e e' por isso
    // que o hex tambem mostra graos. A fracao publicada e' da regiao e o hex
    // mostra o ficheiro inteiro, entao a conversao passa pelo endereco.
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
    // **Esta funcao e' so para a forma de onda e para a curva.** Nelas a janela e'
    // a regiao, e a fracao do grao e' a posicao no ecra — e' por isso que a
    // fracao vai directamente para x. No hex a escala e' outra: a janela e' o
    // ficheiro inteiro e o grao desenha-se na celula do byte, em
    // `paintGrainsInHex`.
    const auto xOf = [&area](float fraction) {
        if (fraction <= 0.0f || fraction >= 1.0f) {
            return -1.0f; // fora da regiao: melhor nao desenhar do que adivinhar
        }
        return area.getX() + fraction * area.getWidth();
    };

    // O indice 0 e' o mais recente, porque o deslocamento empurra para o fim.
    const auto newestX = xOf(trail.positions[0]);
    if (newestX < 0.0f) {
        return;
    }

    // O risco e' **baixo**, com a altura a dar o ganho, e nunca a toda a altura.
    // Desenhado a toda a altura, o grao ficava indistinto da cabeca de leitura:
    // duas barras brancas do mesmo tamanho no mesmo ecra. A cabeca responde a "o
    // que estou a ouvir" e tem de ganhar. Um risco baixo e um risco de altura
    // total nao se confundem, mesmo sendo a mesma cor - e o 1.4.1 e' sobre forma,
    // nao so sobre cor.
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

    // **O rastro e' uma pilha de quadris com alfa crescente, e nao um
    // gradiente.** O gradiente foi a segunda tentativa e dava faixas visiveis:
    // o JUCE quantiza para 8 bits por canal, e num gradiente de meio alfa
    // durante seis pixels cada degrau de 1/255 aparece como uma banda. Com seis
    // pixels de curso nao ha lado nenhum para um gradiente: uma pilha de retangulos
    // de 2 px com alfa a dar para tras da a mesma leitura e nao tem bandas.
    const auto span = newestX - oldestX;
    if (trail.count > 1 && span > 0.5f) {
        // Do mais antigo para o mais recente. O alfa cresce com a proximidade do
        // grao actual, que e' o que faz o olho ler a direccao do varrimento sem
        // seta nem legenda.
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

    // A cabeca do rastro: o ponto mais recente, opaco. E' este o que diz "o grao
    // esta' aqui", e e' o unico elemento do rastro que nao pode faltar.
    //
    // **Branco, e nao a cor do grao.** A primeira versao desenhou o grao em
    // accentSoft, e ele desaparecia. A razao e' medida: o envelope da forma de
    // onda e' accent a 62%, que da #a6690b, e contra isso
    //
    //   accentSoft  2,92:1     accentWarn  2,62:1     textOnDark  2,74:1
    //
    // todos abaixo dos 3:1 que o 1.4.11 exige de um grafico que precisa de
    // contraste para ser entendido. Um grao laranja sobre um envelope laranja so
    // se distinguia por matiz, que e' o que o criterio proibe. O branco da 4,52:1
    // sobre o envelope e 13,4:1 sobre o fundo, e passa nos dois.
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

    // No hex os graos entram aqui, e nao em `paint`: o HexGrid e' um Component
    // filho e pinta-se depois de `paint` do pai, portanto qualquer marca minha
    // feita em `paint` ficaria **por baixo** da grelha e invisivel.
    //
    // O alvo e' a celula do byte, e nao a fracao da regiao. Ver paintGrainsInHex.
    if (mode_ == ViewMode::hex) {
        paintGrainsInHex(g);
    }

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

    // Contorno do nucleo colorido por entropia, com glow. Cada coluna pinta
    // conforme os bits/byte locais — ciano no previsivel, amarelo no meio, rosa
    // no aleatorio — por isso a linha conta duas leituras de uma vez: onde o
    // som esta' (posicao) e do que o material e' feito (cor).
    //
    // Tres Paths por faixa em vez de um stroke por segmento: um stroke por
    // coluna seriam milhares de chamadas por quadro a 60 Hz, e tres Paths com
    // tres cores dao o mesmo desenho. A junta entre faixas vizinhas perde a
    // continuidade do traco num pixel, e num traco de 1 px isso nao se ve.
    //
    // O glow e' pilha de fills com alfa, como o halo dos graos: duas passadas
    // largas e fracas por baixo do traco de 1 px. Sem DropShadowEffect, sem blur
    // que o renderizador de software nao tem — e sem custo que apareca no perfil.
    paintEntropyLine(
        g, columns_, columnWidth, bounds.getX(),
        [&](std::size_t i) {
            return centreY - std::clamp(columns_[i].rms, 0.0f, 1.0f) * halfHeight;
        },
        [&](std::size_t i) {
            return centreY + std::clamp(columns_[i].rms, 0.0f, 1.0f) * halfHeight;
        });
}

// Linha colorida por entropia. Usada pela forma de onda (sobre o nucleo de RMS,
// espelhado no centro) e pela curva de entropia (sobre a propria curva, sem
// espelho): o dado e' o mesmo, `columns_[i].entropyBits`, e a escala e' a
// mesma — 0 a 8. O `topOf` diz o y de cada coluna; `bottomOf` devolve o mesmo y
// quando nao ha espelho.
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

    // Faixas em bits/byte, com os mesmos tercos do eixo da curva (0, 4, 8).
    const auto bucket = [](float bits) {
        if (bits < 3.0f) {
            return 0;
        }
        return bits < 6.0f ? 1 : 2;
    };

    // Ultima coluna que entrou em cada faixa. Sem isto, duas colunas da mesma
    // faixa separadas por colunas de outra faixa sairiam ligadas por uma
    // diagonal atravessando o ecra — que e' o que a primeira versao desenhava, e
    // a captura mostrava diagonais cor-de-rosa de uma ponta a outra.
    std::size_t lastInBucket[3] {columns.size(), columns.size(), columns.size()};

    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto x = originX + static_cast<float>(i) * columnWidth;
        const auto b = bucket(columns[i].entropyBits);

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
        g.setColour(palette::alpha(inks[b], 0.10f));
        g.strokePath(*paths[b], juce::PathStrokeType {5.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
        g.setColour(palette::alpha(inks[b], 0.22f));
        g.strokePath(*paths[b], juce::PathStrokeType {3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
        g.setColour(palette::alpha(inks[b], 0.95f));
        g.strokePath(*paths[b], juce::PathStrokeType {1.0f});
    }
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

    g.setFont(font_);
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

    // A linha por cima, colorida pela propria entropia que desenha: a curva que
    // sobe para o rosa diz "aqui o material e' aleatorio" sem precisar do eixo.
    // Sem espelho — a curva tem um y so por coluna, e o segundo lambda repete o
    // primeiro.
    const auto yAt = [&](std::size_t i) { return yFor(columns_[i].entropyBits); };
    paintEntropyLine(g, columns_, columnWidth, plot.getX(), yAt, yAt);
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