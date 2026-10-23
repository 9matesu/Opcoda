#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace opcoda::view {

// o rastro dos graos, sem JUCE.
//
// e' aritmetica pura sobre arrays fixos — deslocar, truncar, esvaziar por voz,
// decidir se ha razao para pintar — e dentro de um Component nao havia como a
// testar. nao aloca e roda na thread de interface a 60 Hz.
//
// o rastro e' por voz, e nao por slot publicado. o publish do motor compacta os
// graos ativos, entao o slot 0 e' o primeiro grao deste bloco e nada mais. ligar
// uma cauda ao slot faria a cauda saltar de grao em grao a cada morte e dar um
// rasgo falso no ecra. GrainView::voice e' o indice fixo.
class GrainTrailSet {
public:
    static constexpr int kMaxVoices = dsp::GrainTelemetry::kMaxVoices;
    static constexpr int kFrames = 24;

    // vinte e quatro quadros, e nao oito. oito era o minimo para evitar cintilacao,
    // e a medicao mostrou que nao chega: um grao anda ~0,4 px por quadro numa
    // regiao de 700 KB, lido como ponto. com 24 sao seis pixels de curso.
    //
    // com spray e pitch a zero o rastro continua a ser um ponto, e isso esta' certo:
    // nao ha direccao para mostrar.

    struct Trail {
        std::array<float, kFrames> positions {};
        int count {0};

        [[nodiscard]] float newest() const noexcept { return count > 0 ? positions[0] : 0.0f; }
        [[nodiscard]] float oldest() const noexcept {
            return count > 0 ? positions[static_cast<std::size_t>(count - 1)] : 0.0f;
        }

        // curso total em fracao da regiao. zero enquanto o grao nao se mexe, e e'
        // por isso que o desenho trata curso zero como ponto.
        [[nodiscard]] float span() const noexcept { return newest() - oldest(); }

        // posicao valida quando esta' no intervalo. fora disso vem de um publish
        // antigo ou de um bug a montante, nao de um grao a ler.
        [[nodiscard]] static bool validPosition(float value) noexcept {
            return value >= 0.0f && value <= 1.0f;
        }
    };

    // acrescenta a posicao de hoje a uma voz, e o endereco que ela representa.
    // false, sem mexer em nada, se a identidade for invalida: melhor um grao sem
    // rastro do que um rastro no sitio errado.
    //
    // o endereco e' calculado aqui e nao pelo display, para a conta que a vista hex
    // usa ficar sob ensaio. no paint ficava a parte que pode mentir, sem teste.
    bool push(int voice, float position) noexcept {
        if (voice < 0 || voice >= kMaxVoices || !Trail::validPosition(position)) {
            return false;
        }
        if (region_.empty()) {
            // sem regiao a fracao nao quer dizer nada, e converter daria o byte 0:
            // o inicio do ficheiro, e nao o inicio do material.
            return false;
        }

        auto& trail = trails_[static_cast<std::size_t>(voice)];

        // desloca e acrescenta. o mais antigo cai fora, e o rastro mede sempre os
        // ultimos kFrames quadros, nao segundos: a taxa de repintura do host nao
        // muda o comprimento.
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

    // esvazia tudo. chamado quando a fracao deixa de significar a mesma coisa:
    // regiao ou vista nova. sem isto a cauda de um grao que leu a regiao antiga
    // aparecia sobre a nova, pior do que nao ter cauda.
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

    // voz fora do intervalo nao esta' ativa, e nao vale a voz 0. clampVoice serve
    // para ler; para perguntar se uma voz toca, a resposta e' "nao", senao um
    // push(-1) recusado deixaria a voz 0 marcada.
    [[nodiscard]] bool active(int voice) const noexcept {
        if (voice < 0 || voice >= kMaxVoices) {
            return false;
        }
        return active_[static_cast<std::size_t>(voice)];
    }

    [[nodiscard]] std::uint64_t address(int voice) const noexcept {
        return addresses_[static_cast<std::size_t>(clampVoice(voice))];
    }

    // algum rastro ativo? e' o teste barato para "vale a pena pintar"; sem ele o
    // display repinta a 60 Hz mesmo em silencio.
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