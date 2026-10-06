#pragma once

#include <atomic>
#include <cstdint>

namespace opcoda::rt {

// Transporte de audicao: percorre a regiao selecionada de inicio a fim, sem
// nota MIDI, para que se possa ouvir o material sem teclado.
//
// **Vive no nucleo e nao no plugin** pela razao de sempre: nao depende de JUCE, e
// por isso tem teste, sanitizers e cobertura. E' a mesma disciplina da
// NoteTracker, e a regra que vale e a mesma - o que cruza threads e' atomico, e o
// que e' de uma thread so e' variavel simples.
//
// A divisao do trabalho e' esta, e cada metade tem o seu dono:
//
//   - a interface chama play, stop e seekToFraction, de qualquer thread;
//   - a thread de audio chama advance, uma vez por bloco.
//
// **advance() e' a unica funcao que mexe em posicao_, e posicao_ nunca e' escrita
// por ninguem fora da thread de audio.** A interface descobre onde esta a cabeca
// por positionFraction(), que le o valor publicado, e nao por posicao_.
//
// A posicao e' uma fracao da REGIAO e nao um endereco do ficheiro. pe::toSamples
// converte um byte numa amostra, entao uma regiao de N bytes tem N amostras, e o
// motor le `read = position * (sourceCount - 1)`. E' por isso que este classe
// trabalha em espaco de fracao e a traducao para endereco e' do editor, com
// pe::byteForPosition.
//
// **A posicao nunca chega a 1,0.** Em granular_engine.cpp a voz desliga quando
// `index + 1 >= sourceCount`, e por isso pe::positionForByte ja' e' meio aberto em
// cima: o endereco mais alto que produz som e' `end - 2`. Um transporte que
// embrulha em 1,0 exacto leva a ultima fraccao de cada volta a audibilidade zero,
// e isso ouve-se como um clique a cada volta. O lado de cima e' por isso
// `1 - 2/bytes`, e nao 1,0.
class Transport {
public:
    // Tecto de duracao. Doze megabytes a 48 kHz dao 262 s de reproduccao directa,
    // que nao e' uma audicao: e' uma espera. Passados 30 s o material ja foi
    // ouvido e o transporte esta a repetir.
    static constexpr double kMaxSeconds {30.0};

    // Piso de duracao, e ele tem uma razao que nao e' "arredondar".
    //
    // Abaixo do comprimento do maior grao que o motor consegue produzir, uma
    // passagem nem da para completar um grao: o transporte produz um clique e
    // nao uma audicao. Esse comprimento e' GranularEngine::kMaxGrainMs, 100 ms, e
    // por isso o piso e' 0,1 s.
    //
    // O valor e' verificado contra o motor por static_assert em transport.cpp, e
    // nao repetido aqui: dois numeros que precisam de concordar num so sitio
    // divergem no primeiro que alguem mexer num deles.
    //
    // O que este piso **nao** faz e' salvar a regiao de 4 KB, que dura 85 ms de
    // forma natural. Essa fica esticada para 100 ms, uma alteracao de 17 %, e e' a
    // diferenca entre "quase o tempo certo" e "clique". Ver a Historia 1 da
    // specs/008-transporte-e-vistas: o piso e' 0,1 s e nao 0,25 s por causa
    // disso.
    static constexpr double kMinSeconds {0.1};

    // Os atomicos precisam de ser lock-free. Um atomic que toma um mutex interno
    // dentro da thread de audio viola o Principio II do mesmo modo que um lock
    // explicito, e a falha seria invisivel numa revisao. No x64 do MSVC sao
    // todos lock-free, e estas assercoes turningam isso em erro de compilacao
    // se algum dia deixar de ser.
    static_assert(std::atomic<bool>::is_always_lock_free,
                  "Transport exige atomic<bool> sem lock");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Transport exige atomic<uint64_t> sem lock");
    static_assert(std::atomic<double>::is_always_lock_free,
                  "Transport exige atomic<double> sem lock");

    // Taxa de amostragem. Chamada de prepareToPlay, na thread de interface, e nao
    // da thread de audio: e' por isso que e' uma variavel simples e nao um
    // atomico. Quem escreve e' sempre o mesmo e sempre antes do primeiro bloco.
    void prepare(double sampleRate) noexcept;

    // Comprimento da regiao em bytes, que e' tambem o comprimento do buffer de
    // amostras que o motor recebe. Chamado da interface quando a regiao muda.
    void setRegionLength(std::uint64_t bytes) noexcept;

    // Ancora de busca: a fracao a partir da qual a proxima volta comeca. E' o
    // valor de POSITION, e por isso o knob continua vivo e automatizavel.
    //
    // **A ancora nao e' lida a cada bloco.**seekToFraction incrementa uma geracao,
    // e a thread de audio aplica a ancora uma vez e so. Sem a geracao, o editor
    // - que repinta a 60 Hz - voltaria a ancorar a cada quadro e a cabeca nunca
    // passaria do ponto de ancoragem.
    void seekToFraction(double fraction) noexcept;

    void play() noexcept;
    void stop() noexcept;

    // Repoe o estado inicial e para. Chamado de releaseResources.
    void reset() noexcept;

    [[nodiscard]] bool isPlaying() const noexcept {
        return playing_.load(std::memory_order_acquire);
    }

    // Verdadeiro quando ha regiao com material suficiente para produzir som.
    //
    // **O predicado e' `>= 3`, e nao `>= 1` nem `>= 2`.** Com dois bytes,
    // upperBound() da 1 - 2/2 = 0 e advance() congelava em zero para sempre com o
    // gate aberto: um drone de uma amostra com o cursor parado e um botao que parece
    // funcionar. Com tres ha uma posicao util, que e' o que `end - 2` exige.
    [[nodiscard]] bool hasRegion() const noexcept {
        return regionBytes_.load(std::memory_order_acquire) >= 3;
    }

    // **Thread de audio, uma vez por bloco.** Devolve a fracao de posicao a usar
    // neste bloco, ja avancada e ja recortada.
    //
    // `playingSnapshot` tem de ser o valor que o chamador leu de isPlaying(). Passar
    // o resultado em vez de o reler dentro e' o que garante que a posicao e o gate
    // vem do mesmo estado: com o atómico lido duas vezes, o intervalo entre as duas
    // leituras produz ate' um bloco de audio com a posicao congelada e o gate aberto.
    // Nao aloca, nao trava e nao le ficheiros. E' a unica funcao que escreve em
    // posicao_.
    [[nodiscard]] float advance(int numSamples, bool playingSnapshot) noexcept;

    // Posicao corrente, para o display. Le o valor publicado pela thread de audio
    // e nao posicao_, que e' dela.
    [[nodiscard]] float positionFraction() const noexcept {
        return static_cast<float>(publishedPosition_.load(std::memory_order_relaxed));
    }

    // Duracao de uma passagem em segundos, ja com o piso e o tecto aplicados.
    // Chamada da interface para escrever no display; le atomicos e nao escreve.
    [[nodiscard]] double durationSeconds() const noexcept;

private:
    // Avanco por amostra, em fracao da regiao.
    //
    // **O numero de bytes cancela-se, e isso e' proposito.** A fracao por amostra
    // e' 1 / (taxa * duracao), e a duracao natural e' bytes / taxa: o quociente
    // da regiao inteira reduz a 1/bytes, que e' o consumo de um byte por amostra
    // de saida. So o clamp precisa dos bytes, e so para decidir se a duracao
    // natural cabe no tecto.
    [[nodiscard]] double fractionPerSample() const noexcept;

    // Lado de cima da posicao. Abaixo de 1,0 porque 1,0 e' silencio.
    [[nodiscard]] double upperBound() const noexcept;

    // Pede ao bloco seguinte que reponha a posicao a zero. Nao escreve em
    // posicao_: quem escreve e' advance(), e so' advance' escreve.
    void requestReset() noexcept;

    double sampleRate_ {44100.0};

    // De uma thread so: posicao_ e' escrita exclusivamente por advance(), que so a
    // thread de audio chama, e nao por mais nada. reset() e setRegionLength()
    // pedem o reset por requestReset() e nao escrevem aqui.
    double position_ {0.0};
    std::uint64_t appliedSeekGeneration_ {0};

    std::atomic<std::uint64_t> regionBytes_ {0};
    std::atomic<bool> playing_ {false};
    std::atomic<double> anchor_ {0.0};
    std::atomic<std::uint64_t> seekGeneration_ {0};
    std::atomic<bool> resetRequested_ {false};
    std::atomic<std::uint64_t> resetSeekGeneration_ {0};
    std::atomic<double> publishedPosition_ {0.0};
};

} // namespace opcoda::rt