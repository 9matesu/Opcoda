#include "opcoda_core/rt/transport.h"

#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>

namespace opcoda::rt {
namespace {

// o piso existe porque abaixo de um grao nao ha audicao, e o grao mais longo do
// motor e' GranularEngine::kMaxGrainMs. a assercao prende os dois numeros: se o
// maximo mudar e ninguem mexer aqui, o build parte em vez de o piso ficar curto.
static_assert(Transport::kMinSeconds ==
                  static_cast<double>(opcoda::dsp::GranularEngine::kMaxGrainMs) / 1000.0,
              "o piso de duracao tem de ser o comprimento do maior grao");

// a posicao e' fracao, e o que se recusa e' o que nao e' fracao. NaN sairia como
// zero em silencio, e nao como erro.
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

// reset e' um pedido, e nao uma escrita.
//
// posicao_ e' da thread de audio e so dela. setRegionLength e' chamado da
// interface a cada arrasto do campo de endereco, e a thread de audio podia estar
// dentro de advance() a ler e a escrever a mesma variavel. o pedido viaja por uma
// marca, como a ancora, e advance() consome-o.
//
// a marca guarda a geracao da ancora tal como estava no pedido: uma busca que
// chegou depois do reset e antes do bloco seguinte tem de ser aplicada, e a do
// ficheiro antigo tem de ser descartada.
void Transport::requestReset() noexcept {
    resetRequested_.store(true, std::memory_order_relaxed);
    resetSeekGeneration_.store(seekGeneration_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
}

void Transport::setRegionLength(std::uint64_t bytes) noexcept {
    regionBytes_.store(bytes, std::memory_order_release);

    // mudar a regiao com o transporte a tocar e' o caso comum, porque o campo de
    // endereco move a regiao enquanto o botao esta' premido. repor a posicao a zero
    // e' o honesto: a fracao antiga aponta para outro sitio do ficheiro novo.
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
    // aberto, o drone de uma amostra que o predicado >= 3 existe para impedir.
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
    // taxa presa em [0,25, 4,0] pelo setRate, entao nao ha divisao por zero; o
    // clamp mantem a passagem audivel em qualquer taxa.
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
        // regiao vazia ou de um byte: nao ha posicao que produza som, e 1,0 e'
        // silencio. zero mantem a promessa de que a fracao devolvida esta' dentro
        // da regiao.
        position_ = 0.0;
        publishedPosition_.store(0.0, std::memory_order_relaxed);
        return 0.0f;
    }

    // o reset pedido e' consumido aqui, no unico sitio que pode mudar a posicao.
    //
    // a geracao gravada e' a do momento do pedido, nao a de agora: uma busca que
    // chegou depois do reset tem de ser aplicada, e uma que ja' era do ficheiro
    // antigo tem de ser descartada.
    if (resetRequested_.exchange(false, std::memory_order_acquire)) {
        position_ = 0.0;
        appliedSeekGeneration_ = resetSeekGeneration_.load(std::memory_order_relaxed);
    }

    // a ancora aplica-se uma vez por seekToFraction, e nao a cada bloco.
    //
    // isto vem ANTES de qualquer guarda de numSamples: uma leitura da posicao sem
    // avanco, que e' o que o editor faz, devolveria a posicao antiga e ignoraria a
    // busca. foi o que o teste SeekMovesTheHeadImmediately apanhou.
    const auto generation = seekGeneration_.load(std::memory_order_acquire);
    if (generation != appliedSeekGeneration_) {
        appliedSeekGeneration_ = generation;
        const auto anchor = anchor_.load(std::memory_order_relaxed);
        position_ = isUsableFraction(anchor) ? std::min(anchor, upper) : 0.0;
    }

    // o `playing` vem do snapshot que o chamador passou. reler aqui seria uma
    // segunda leitura do atomico, e entre as duas a cabeca avanca e o gate nao, ou
    // o contrario: sobra um bloco parado, que nem e' silencio nem e' clique.
    if (numSamples > 0 && playingSnapshot) {
        const auto step = fractionPerSample();
        if (step > 0.0) {
            position_ += step * static_cast<double>(numSamples);
        }

        // embrulha em zero: o transporte e' um laco de audicao, nao uma passagem
        // com fim. uma volta de 30 s a acabar em silencio obriga a carregar outra
        // vez.
        if (position_ >= upper) {
            position_ = 0.0;
        }
    }

    publishedPosition_.store(position_, std::memory_order_relaxed);
    return static_cast<float>(position_);
}

} // namespace opcoda::rt