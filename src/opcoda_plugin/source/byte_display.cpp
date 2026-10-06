#include "byte_display.h"

#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda {
namespace {

// Preenchimento do envelope.
//
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

    monoFont_ = BoxedLabel::monoFont(10.0f);

    // 4.1.2. O nome diz o que e' e o texto de ajuda diz o que se pode fazer, porque
    // um componente que so se opera com o rato e' um buraco sem nome para quem
    // navega por leitor de tela.
    setName("Display do material binario");
    setHelpText(
        "Tres vistas do material: forma de onda, grelha de bytes e curva de "
        "entropia. Clique para mover a cabeca de leitura. As teclas 1, 2 e 3 "
        "mudam de vista.");

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
    const auto fraction = fractionOf(readHead_);
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

    const auto fraction = std::clamp(playheadFraction_, 0.0f, 1.0f);
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
    if (mode_ == ViewMode::hex) {
        return; // o HexGrid trata do seu
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onAddressActivated != nullptr) {
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