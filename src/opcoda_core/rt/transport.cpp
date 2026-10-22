#include "opcoda_core/rt/transport.h"

#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>

namespace opcoda::rt {
namespace {

// O piso de duracao existe porque abaixo de um grao nao ha audicao, e o grao mais
// longo que o motor produz tem GranularEngine::kMaxGrainMs. A assercao e' a forma
// de os dois numeros nao divergirem: se um dia o maximo de grao mudar para 200 ms
// e ninguem mexer aqui, o build parte em vez de o piso ficar curto em silencio.
static_assert(Transport::kMinSeconds ==
                  static_cast<double>(opcoda::dsp::GranularEngine::kMaxGrainMs) / 1000.0,
              "o piso de duracao tem de ser o comprimento do maior grao");

// A posicao e' uma fracao, e o que se recusa e' o que nao e' fracao. Um NaN
// aqui voltaria a sair como zero em silencio, e nao como erro.
bool isUsableFraction(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value < 1.0;
}

} // namespace

void Transport::prepare(double sampleRate) noexcept {
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
    reset();
}

void Transport::reset() noexcept {
    stop();
    requestReset();
}

// O reset e' um pedido, e nao uma escrita.
//
// **posicao_ e' da thread de audio e so dela.** A primeira versao escrevia
// posicao_ = 0 aqui e em setRegionLength, e os dois sao chamados da thread de
// interface — setRegionLength em CADA arrasto do campo de endereco. A thread de
// audio podia estar dentro de advance() a ler e a escrever a mesma variavel. Em x64
// um double alinhado nao rasga, portanto nunca houve valor lixo, mas havia uma
// escrita perdida e o contrato escrito no cabecalho era falso.
//
// O pedido viaja por uma marca, tal como a ancora, e advance() consome-o. O
// resultado e' que a interface nunca escreve em estado da thread de audio.
//
// **A marca guarda a geracao da ancora tal como estava no pedido.** Marcar a
// geracao *actual* no consumo descartaria uma busca que chegou depois do reset e
// antes do bloco seguinte, e a cabeca ficaria em zero quando o utilizador tinha
// acabado de a mover. O que se quer e' descartar a ancora que era a do ficheiro
// antigo, e poupar a que chegou depois.
void Transport::requestReset() noexcept {
    resetRequested_.store(true, std::memory_order_relaxed);
    resetSeekGeneration_.store(seekGeneration_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
}

void Transport::setRegionLength(std::uint64_t bytes) noexcept {
    regionBytes_.store(bytes, std::memory_order_release);

    // Mudar a regiao com o transporte a tocar e' o caso comum, porque o campo de
    // endereco move a regiao enquanto o botao esta' premido. Repor a posicao a
    // zero e' o comportamento honesto: a fracao antiga aponta para um sitio
    // diferente do ficheiro novo, e mantê-la seria ler onde nao ha material.
    requestReset();
}

void Transport::seekToFraction(double fraction) noexcept {
    anchor_.store(std::clamp(fraction, 0.0, 1.0), std::memory_order_relaxed);
    // release, e o advance() de acq_rel: o valor da ancora tem de estar visivel
    // antes do numero que o anuncia.
    seekGeneration_.fetch_add(1, std::memory_order_release);
}

void Transport::setRate(double rate) noexcept {
    // NaN volta a 1.0, e nao a zero: taxa zero congelava a cabeca com o gate
    // aberto, que e' o drone de uma amostra que o predicado >= 3 existe para
    // impedir.
    rate_.store(std::isfinite(rate) ? std::clamp(rate, 0.25, 4.0) : 1.0,
                std::memory_order_relaxed);
}

void Transport::play() noexcept {
    playing_.store(true, std::memory_order_release);
}

void Transport::stop() noexcept {
    playing_.store(false, std::memory_order_release);
}

double Transport::durationSeconds() const noexcept {
    const auto bytes = regionBytes_.load(std::memory_order_acquire);
    if (bytes < 2 || sampleRate_ <= 0.0) {
        return 0.0;
    }

    const auto natural = static_cast<double>(bytes) / sampleRate_;
    const auto rate = rate_.load(std::memory_order_relaxed);
    // Taxa guardada em [0,25, 4,0] pelo setRate, entao nao ha divisao por zero
    // aqui; o clamp depois mantem a passagem audivel em qualquer taxa.
    return std::clamp(natural / rate, kMinSeconds, kMaxSeconds);
}

double Transport::fractionPerSample() const noexcept {
    const auto duration = durationSeconds();
    if (duration <= 0.0) {
        return 0.0;
    }
    return 1.0 / (sampleRate_ * duration);
}

double Transport::upperBound() const noexcept {
    const auto bytes = regionBytes_.load(std::memory_order_acquire);
    if (bytes < 2) {
        return 0.0;
    }
    return std::max(0.0, 1.0 - 2.0 / static_cast<double>(bytes));
}

float Transport::advance(int numSamples, bool playingSnapshot) noexcept {
    const auto upper = upperBound();
    if (upper <= 0.0) {
        // Regiao vazia ou de um byte: nao ha posicao que produza som, e 1,0 e'
        // silencio. Repor a zero mantem a promessa de que a fraccao devolvida
        // esta' sempre dentro da regiao.
        position_ = 0.0;
        publishedPosition_.store(0.0, std::memory_order_relaxed);
        return 0.0f;
    }

    // O reset pedido e' consumido aqui, no unico sitio que pode mudar a posicao.
    // E' o que mantem o invariante de que posicao_ e' escrita so' por advance().
    //
    // A geracao gravada e' a do momento do pedido, nao a de agora: uma busca que
    // chegou depois do reset tem de ser aplicada, e uma que ja era a do ficheiro
    // antigo tem de ser descartada.
    if (resetRequested_.exchange(false, std::memory_order_acquire)) {
        position_ = 0.0;
        appliedSeekGeneration_ = resetSeekGeneration_.load(std::memory_order_relaxed);
    }

    // A ancora aplica-se uma vez por seekToFraction, e nao a cada bloco.
    //
    // **Isto vem ANTES de qualquer guarda de numSamples.** Uma guarda que devolvesse
    // a posicao corrente para numSamples <= 0 ficaria antes da ancora, e uma
    // leitura da posicao sem avanco — que e' o que o editor faz para saber onde
    // esta a cabeca — devolveria a posicao antiga e ignoraria a busca. Foi
    // exactamente o que o teste SeekMovesTheHeadImmediately apanhou.
    const auto generation = seekGeneration_.load(std::memory_order_acquire);
    if (generation != appliedSeekGeneration_) {
        appliedSeekGeneration_ = generation;
        const auto anchor = anchor_.load(std::memory_order_relaxed);
        position_ = isUsableFraction(anchor) ? std::min(anchor, upper) : 0.0;
    }

    // **O `playing` vem do snapshot que o chamador passou.** Rele-lo aqui seria
    // uma segunda leitura do atómico, e o intervalo entre ela e a do chamador e' o
    // intervalo em que a cabeca avanca e o gate nao, ou o contrario. O sintoma e'
    // ate' um bloco de audio parado, que nem e' silencio nem e' clique.
    if (numSamples > 0 && playingSnapshot) {
        const auto step = fractionPerSample();
        if (step > 0.0) {
            position_ += step * static_cast<double>(numSamples);
        }

        // Embrulha em zero. O transporte e' um laco de audicao, e nao uma passagem
        // com fim: quem carregou no botao carregou para ouvir, e uma passagem de
        // 30 s que acaba em silencio obriga a carregar outra vez.
        if (position_ >= upper) {
            position_ = 0.0;
        }
    }

    publishedPosition_.store(position_, std::memory_order_relaxed);
    return static_cast<float>(position_);
}

} // namespace opcoda::rt