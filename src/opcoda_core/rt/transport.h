#pragma once

#include <atomic>
#include <cstdint>

namespace opcoda::rt {

// transporte de audicao: percorre a regiao selecionada de inicio a fim, sem nota
// MIDI, para se ouvir o material sem teclado.
//
// vive no nucleo e nao no plugin porque nao depende de JUCE, e por isso tem teste,
// sanitizers e cobertura. o que cruza threads e' atomico; o que e' de uma thread
// so e' variavel simples.
//
// a interface chama play, stop e seekToFraction, de qualquer thread; a thread de
// audio chama advance, uma vez por bloco. advance() e' a unica funcao que mexe em
// posicao_, e a interface le a cabeca por positionFraction().
//
// a posicao e' fracao da REGIAO, nao endereco do ficheiro. a regiao de N bytes tem
// N amostras e o motor le `read = position * (sourceCount - 1)`; a traducao para
// endereco e' do editor, com pe::byteForPosition.
//
// a posicao nunca chega a 1,0: a voz desliga em `index + 1 >= sourceCount`. o lado
// de cima e' 1 - 2/bytes, senao a ultima fracao de cada volta fica muda e ouve-se
// um clique.
class Transport {
public:
    // tecto de duracao. 12 MB a 48 kHz dao 262 s de espera, nao de audicao: aos
    // 30 s o material ja' foi ouvido e o transporte esta a repetir.
    static constexpr double kMaxSeconds {30.0};

    // piso de duracao, e nao por arredondamento.
    //
    // a regiao minima dura 100 ms -- o suficiente para o clique soar como audicao
    // e nao como estalo. o piso nao acompanha o tamanho do grao: desde 27/10/2026
    // o knob vai a 2 s, e uma regiao de 4 KB (85 ms) continua a ser esticada para
    // 100 ms, nao para o comprimento de um grao.
    static constexpr double kMinSeconds {0.1};

    // os atomicos tem de ser lock-free. um atomic com mutex interno dentro da
    // thread de audio viola o Principio II do mesmo modo que um lock explicito, e
    // a falha seria invisivel numa revisao. no x64 do MSVC sao todos lock-free.
    static_assert(std::atomic<bool>::is_always_lock_free,
                  "Transport exige atomic<bool> sem lock");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Transport exige atomic<uint64_t> sem lock");
    static_assert(std::atomic<double>::is_always_lock_free,
                  "Transport exige atomic<double> sem lock");

    // taxa de amostragem. chamada de prepareToPlay, na thread de interface e antes
    // do primeiro bloco; por isso e' variavel simples e nao atomico.
    void prepare(double sampleRate) noexcept;

    // comprimento da regiao em bytes, que e' tambem o do buffer de amostras que o
    // motor recebe. chamado da interface quando a regiao muda.
    void setRegionLength(std::uint64_t bytes) noexcept;

    // ancora de busca: a fracao onde a proxima volta comeca. e' o valor de POSITION,
    // por isso o knob continua vivo e automatizavel.
    //
    // a ancora nao e' lida a cada bloco. seekToFraction incrementa uma geracao e a
    // thread de audio aplica uma vez; sem isso o editor, que repinta a 60 Hz,
    // voltaria a ancorar a cada quadro e a cabeca nunca passaria do ponto.
    void seekToFraction(double fraction) noexcept;

    void play() noexcept;
    void stop() noexcept;

    // velocidade de varredura, multiplica o avanco por amostra. 1,0 e' tempo
    // natural, 2,0 percorre em metade do tempo. faixa 0,25..4,0: abaixo arrasta,
    // acima a cabeca salta posicoes inteiras por bloco e o display mente.
    //
    // de qualquer thread; a thread de audio le o atomico em durationSeconds(),
    // o(1) por bloco. mesmo desenho da ancora: atomico lock-free, nunca escrita
    // directa.
    void setRate(double rate) noexcept;

    // repoe o estado inicial e para. chamado de releaseResources.
    void reset() noexcept;

    [[nodiscard]] bool isPlaying() const noexcept {
        return playing_.load(std::memory_order_acquire);
    }

    // verdadeiro quando ha regiao com material para soar.
    //
    // o predicado e' `>= 3`, e nao `>= 1` nem `>= 2`. com dois bytes, upperBound()
    // da 1 - 2/2 = 0 e advance() congelava em zero com o gate aberto: um drone de
    // uma amostra com um botao que parece funcionar.
    [[nodiscard]] bool hasRegion() const noexcept {
        return regionBytes_.load(std::memory_order_acquire) >= 3;
    }

    // thread de audio, uma vez por bloco. devolve a fracao de posicao a usar neste
    // bloco, ja avancada e recortada.
    //
    // `playingSnapshot` tem de ser o valor lido de isPlaying(). passar o resultado
    // em vez de reler garante que a posicao e o gate vem do mesmo estado: com duas
    // leituras, o intervalo entre elas produz ate' um bloco com a posicao congelada
    // e o gate aberto. nao aloca, nao trava, nao le ficheiros.
    [[nodiscard]] float advance(int numSamples, bool playingSnapshot) noexcept;

    // posicao corrente, para o display. le o valor publicado pela thread de audio,
    // e nao posicao_, que e' dela.
    [[nodiscard]] float positionFraction() const noexcept {
        return static_cast<float>(publishedPosition_.load(std::memory_order_relaxed));
    }

    // duracao de uma passagem em segundos: a natural dividida pela taxa, com piso
    // e tecto aplicados DEPOIS, sobre o valor efetivo. chamada da interface para
    // escrever no display; le atomicos e nao escreve.
    //
    // o clamp depois e' o que importa: uma regiao curta a 4x daria 25 ms, que nao
    // completa a audicao minima de 100 ms. em regiao extrema a taxa satura, e isso
    // e' documentado.
    [[nodiscard]] double durationSeconds() const noexcept;

private:
    // avanco por amostra, em fracao da regiao. os bytes cancelam-se: a fracao e'
    // 1 / (taxa * duracao) e a natural e' bytes / taxa, logo 1/bytes. so o clamp
    // precisa dos bytes, para decidir se a natural cabe no tecto.
    [[nodiscard]] double fractionPerSample() const noexcept;

    // lado de cima da posicao. abaixo de 1,0 porque 1,0 e' silencio.
    [[nodiscard]] double upperBound() const noexcept;

    // pede ao bloco seguinte que reponha a posicao a zero. nao escreve em posicao_:
    // quem escreve e' advance(), e so advance().
    void requestReset() noexcept;

    double sampleRate_ {44100.0};

    // de uma thread so: escrita exclusivamente por advance(), que so a thread de
    // audio chama. reset() e setRegionLength() pedem por requestReset() e nao
    // escrevem aqui.
    double position_ {0.0};
    std::uint64_t appliedSeekGeneration_ {0};

    std::atomic<std::uint64_t> regionBytes_ {0};
    std::atomic<bool> playing_ {false};
    std::atomic<double> anchor_ {0.0};
    std::atomic<double> rate_ {1.0};
    std::atomic<std::uint64_t> seekGeneration_ {0};
    std::atomic<bool> resetRequested_ {false};
    std::atomic<std::uint64_t> resetSeekGeneration_ {0};
    std::atomic<double> publishedPosition_ {0.0};
};

} // namespace opcoda::rt