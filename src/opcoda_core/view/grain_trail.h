#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace opcoda::view {

// O rastro dos graos, sem JUCE.
//
// **Vive no nucleo e nao no ByteDisplay por causa dos testes.** A logica toda —
// deslocar, truncar, esvaziar por voz, decidir se ha razao para pintar — e' aritmetica
// pura sobre arrays fixos, e dentro de um `juce::Component` nao ha como a testar sem
// instanciar uma janela. `pe::reduceToColumns` e' o mesmo caso: e' calculo que o
// display faz na thread de interface, e vive aqui para ter quinze ensaios.
//
// Nao aloca, nao tem estado escondido e e' called na thread de interface a 60 Hz.
//
// **O rastro e' por voz, e nao por slot publicado.** O `publish` do motor compacta os
// graos activos para a frente, portanto o slot 0 e' o primeiro grao activo *deste*
// bloco e nada mais. Ligar uma cauda a um slot faria a cauda saltar de um grao para
// outro cada vez que um deles morre, e o ecra ganharia um rasgo falso atravessado a
// cada bloco. `GrainView::voice` e' o indice fixo no motor, e e' o que fecha a cauda.
class GrainTrailSet {
public:
    static constexpr int kMaxVoices = dsp::GrainTelemetry::kMaxVoices;
    static constexpr int kFrames = 24;

    // Vinte e quatro quadros, e nao oito. Oito era o minimo para evitar a
    // cintilacao de um grao que atravessa o ecra em dois quadros e meio, e a medicao
    // mostrou que nao chega: um grao anda cerca de 0,4 px por quadro numa regiao de
    // 700 KB, e oito quadros davam dois pixels de curso, lidos como ponto. Com
    // vinte e quatro sao seis, e a cauda ve-se como cauda.
    //
    // **Com spray e pitch a zero o rastro continua a ler-se como ponto**, e isso e'
    // o comportamento certo: nao ha direccao para mostrar, porque o grao nao se mexe.

    struct Trail {
        std::array<float, kFrames> positions {};
        int count {0};

        [[nodiscard]] float newest() const noexcept { return count > 0 ? positions[0] : 0.0f; }
        [[nodiscard]] float oldest() const noexcept {
            return count > 0 ? positions[static_cast<std::size_t>(count - 1)] : 0.0f;
        }

        // Curso total do rastro, em fracao da regiao. E' zero enquanto o grao nao
        // se mexe, e e' por isso que o desenho trata curso zero como "ponto".
        [[nodiscard]] float span() const noexcept { return newest() - oldest(); }

        // Posicao e' valida quando esta no intervalo. Um valor fora disso vem de um
        // publish antigo ou de um bug a montante, e nao de um grao a ler.
        [[nodiscard]] static bool validPosition(float value) noexcept {
            return value >= 0.0f && value <= 1.0f;
        }
    };

    // Acrescenta a posicao de hoje a uma voz, e o endereco que essa posicao
    // representa. Devolve false e nao mexe em nada se a identidade for invalida:
    // melhor um grao sem rastro do que um rastro no sitio errado.
    //
    // **O endereco e' calculado aqui e nao pelo display**, para que o valor que a
    // vista hex usa seja o mesmo que aqui esta' sob ensaio. Foi a primeira
    // implementacao a converter no `paint`, e a aritmetica ficou sem um unico
    // ensaio — sendo exactamente a parte que pode mentir sobre onde o grao esta'.
    bool push(int voice, float position) noexcept {
        if (voice < 0 || voice >= kMaxVoices || !Trail::validPosition(position)) {
            return false;
        }
        if (region_.empty()) {
            // Sem regiao a fracao nao quer dizer nada, e converter na daria o byte 0
            // — que e' o inicio do ficheiro, e nao o inicio do material.
            return false;
        }

        auto& trail = trails_[static_cast<std::size_t>(voice)];

        // Desloca e acrescenta. O mais antigo cai fora, e e' isso que da a
        // comprimento ao rastro em quadros e nao em tempo: o rastro mede sempre os
        // ultimos kFrames quadros, independente da taxa a que o host repinta.
        for (int f = std::min(trail.count, kFrames - 1); f > 0; --f) {
            trail.positions[static_cast<std::size_t>(f)] =
                trail.positions[static_cast<std::size_t>(f - 1)];
        }
        if (trail.count < kFrames) {
            ++trail.count;
        }
        trail.positions[0] = position;

        addresses_[static_cast<std::size_t>(voice)] =
            pe::byteForPosition(static_cast<double>(position), region_);
        active_[static_cast<std::size_t>(voice)] = true;
        return true;
    }

    // Esvazia tudo. Chamado quando a fracao deixa de significar a mesma coisa: mudar
    // de regiao ou de vista. Sem isto, a cauda de um grao que leu o fim da regiao
    // antiga aparecia sobre a regiao nova, o que e' pior do que nao ter cauda nenhuma.
    void clear() noexcept {
        for (auto& trail : trails_) {
            trail = Trail {};
        }
        active_.fill(false);
    }

    void setRegion(std::uint64_t start, std::uint64_t end) noexcept {
        region_ = pe::ByteRange {start, end};
    }

    [[nodiscard]] const Trail& trail(int voice) const noexcept {
        return trails_[static_cast<std::size_t>(clampVoice(voice))];
    }

    // Vozes fora do intervalo nao sao ativas, e nao a voz 0. `clampVoice` serve
    // para ler, mas para perguntar se uma voz esta a tocar a resposta tem de ser
    // "nao": empurrar `push(-1, ...)` recusado nao pode deixar a voz 0 marcada.
    [[nodiscard]] bool active(int voice) const noexcept {
        if (voice < 0 || voice >= kMaxVoices) {
            return false;
        }
        return active_[static_cast<std::size_t>(voice)];
    }

    [[nodiscard]] std::uint64_t address(int voice) const noexcept {
        return addresses_[static_cast<std::size_t>(clampVoice(voice))];
    }

    // Algum rastro tem curso? E' o teste barato para "vale a pena pintar", e sem ele
    // o display repinta a 60 Hz mesmo sem som nenhum.
    [[nodiscard]] bool anyActive() const noexcept {
        for (std::size_t v = 0; v < active_.size(); ++v) {
            if (active_[v]) {
                return true;
            }
        }
        return false;
    }

private:
    [[nodiscard]] static int clampVoice(int voice) noexcept {
        return (voice < 0 || voice >= kMaxVoices) ? 0 : voice;
    }

    std::array<Trail, kMaxVoices> trails_ {};
    std::array<std::uint64_t, kMaxVoices> addresses_ {};
    std::array<bool, kMaxVoices> active_ {};
    pe::ByteRange region_ {};
};

} // namespace opcoda::view