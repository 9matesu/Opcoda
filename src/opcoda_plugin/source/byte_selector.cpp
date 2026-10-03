#include "byte_selector.h"

namespace opcoda {
namespace {

// A regiao selecionada ocupa a base do seletor, para o mapa de secoes ficar
    // visivel acima.
constexpr float kBarHeight {5.0f};

constexpr int kMinWindowBytes {64};

} // namespace

ByteSelector::ByteSelector() {
    setOpaque(false);

    slider_.setSliderStyle(juce::Slider::LinearHorizontal);
    slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_.setVelocityBasedMode(false);
    // Inicio e fim da regiao num slider so': o cursor e' o inicio, e o
    // comprimento vem do Processor, que e' quem decide o tamanho da janela.
    slider_.setRange(0.0, 1.0, 0.0);
    slider_.setDoubleClickReturnValue(false, 0.0);
    // Marca o comando para o LookAndFeel usar a pega fisica em vez do cursor
    // padrao. O nome e' uma chave de desenho, e nao um parametro ou funcao.
    slider_.getProperties().set("byteRangeSelector", true);
    slider_.onSnapRequested = [this] {
        if (onSnapRequested) {
            onSnapRequested();
        }
    };

    // 4.1.2 Nome, funcao e valor. Um Slider sem nome nao diz a um leitor de tela
    // o que e' nem o que faz, e "slider" sozinho e' o mesmo que nao dizer nada.
    slider_.setName("Inicio da regiao de bytes");
    // A dica e' a descricao longa que um leitor de tela le. setName e' o nome
    // curto; a dica entra na descricao accesible do Slider, que e' o que o
    // 4.1.2 do WCAG pede: nome e valor, nao nome e adivinhar.
    slider_.setTooltip(
        "Arraste para escolher o byte inicial do material sintetizado. "
        "Seta a esquerda e a direita movem o inicio, Home e End vao para o "
        "inicio e para o fim do ficheiro. Duplo clique ou Enter alterna entre o "
        "byte exato e a secao mais proxima.");

    // A seta escreve no Processor, que valida e publica. O seletor nao decide
    // nada: se o Processor recusar, o valor volta no proximo showRange.
    slider_.onValueChange = [this] {
        if (fileSize_ == 0 || onRangeChanged == nullptr) {
            return;
        }
        const auto start = static_cast<std::uint64_t>(slider_.getValue());
        const auto length = rangeEnd_ > rangeStart_ ? rangeEnd_ - rangeStart_ : 0;
        onRangeChanged(start, start + juce::jmax<std::uint64_t>(length, kMinWindowBytes));
    };

    addAndMakeVisible(slider_);
}

void ByteSelector::setFileSize(std::uint64_t sizeBytes) {
    fileSize_ = sizeBytes;

    // A escala e' o tamanho real do ficheiro, para o valor do slider ser o
    // offset em bytes e nao uma fracao sem significado.
    slider_.setRange(0.0, static_cast<double>(juce::jmax<std::uint64_t>(sizeBytes, 1)), 1.0);
    repaint();
}

void ByteSelector::showRange(std::uint64_t start, std::uint64_t end) {
    rangeStart_ = start;
    rangeEnd_ = juce::jmax(end, start);

    // setValue sem getThumbDragReleaseArea impede que isto dispare
    // onValueChange: sao um valor de fora a entrar, e nao o utilizador a mexer.
    slider_.setValue(static_cast<double>(start), juce::dontSendNotification);
    repaint();
}

void ByteSelector::resized() {
    // O slider ocupa a faixa entre o mapa de secoes e a barra da regiao, para o
    // arraste e as duas leituras nao ficarem um em cima do outro.
    slider_.setBounds(getLocalBounds().reduced(0, 6));
}

void ByteSelector::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::displayPanel);
    g.fillRect(bounds);

    // Mapa de secoes em cima: e' a referencia contra a qual se le a regiao.
    // A barra de regiao vai por baixo, e assim uma regiao que cobre tudo
    // ainda deixa ver o que ha por baixo.
    paintSectionBar(g);

    if (fileSize_ == 0 || rangeEnd_ <= rangeStart_) {
        // Sem ficheiro: uma barra a zero pode ler-se como material carregado e
        // sem som. O texto e' a forma explicita de dizer que nao ha nada.
        g.setColour(palette::textOnDarkSub);
        g.setFont(BoxedLabel::monoFont(9.0f));
        g.drawText("SEM BINARIO", bounds.reduced(8.0f, 0.0f),
                   juce::Justification::centredLeft, false);
        return;
    }

    // Barra da regiao selecionada, na base. Orange sobre o display escuro: da
    // 6,9:1 e sobrevive a 4,5:1, ao contrario do laranja sobre o chassis claro.
    const auto toX = [&](std::uint64_t byte) {
        return bounds.getX() + bounds.getWidth() *
                                    (static_cast<double>(byte) / static_cast<double>(fileSize_));
    };

    const auto x0 = static_cast<float>(toX(rangeStart_));
    const auto x1 = static_cast<float>(toX(rangeEnd_));
    const auto barY = bounds.getBottom() - kBarHeight - 1.0f;

    g.setColour(palette::accent);
    g.fillRoundedRectangle(
        juce::Rectangle<float> {x0, barY, juce::jmax(2.0f, x1 - x0), kBarHeight}, 1.5f);

    // As duas pontas sao hastes de 1 px, mais claras que a barra. Sao o que
    // diz onde a regiao comeca e acaba quando a barra e' estreita demais
    // para os numeros caberem dentro.
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawVerticalLine(juce::roundToInt(x0), bounds.getY(), bounds.getHeight());
    g.drawVerticalLine(juce::roundToInt(x1), bounds.getY(), bounds.getHeight());
}

void ByteSelector::paintSectionBar(juce::Graphics& g) {
    if (fileSize_ == 0 || sections_.empty()) {
        return;
    }

    const auto bounds = getLocalBounds().toFloat();
    const auto stripHeight = 5.0f;

    // As secoes vao no topo, com 1 px de espaco, e a regiao na base: a regiao
    // pode cobrir o ficheiro todo e nao pode tapar o mapa que a explica.
    std::uint64_t total = 0;
    for (const auto& section : sections_) {
        total += section.rawSize;
    }
    if (total == 0) {
        return;
    }

    const auto scale = bounds.getWidth() / static_cast<float>(fileSize_);
    float x = bounds.getX();

    for (const auto& section : sections_) {
        const auto width = static_cast<float>(section.rawSize) * scale;
        if (width >= 1.0f) {
            // Divisoria de 1 px entre secoes: o que distingue duas secoes sem
            // depender so da cor e' o ponto, e o nome vive no rotulo do rodape.
            g.setColour(palette::displayBorder);
            g.fillRect(juce::Rectangle<float> {x, bounds.getY(), juce::jmax(1.0f, width),
                                               stripHeight});
        }
        x += width;
    }
}

} // namespace opcoda
