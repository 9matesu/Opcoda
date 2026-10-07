#pragma once

#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace opcoda::dsp {

struct GranularParams {
    float grainSizeMs {40.0f};
    float densityGrainsPerSec {20.0f};
    float position {0.0f};
    float spray {0.0f};
    float pitchSemitones {0.0f};
    float volumeDb {0.0f};
    float pan {0.0f};
    WindowType window {WindowType::kHann};
};

struct Grain {
    bool active {false};
    double position {};
    double readStep {};
    int remaining {};
    int windowIndex {};
    int writeIndex {0};
    float gain {};
    float panLeft {};
    float panRight {};
};

// O que a thread de interface precisa para desenhar um grao no sitio onde ele
// esta' a ler o material.
struct GrainView {
    // **O indice da voz, e nao um numero de ordem.** E' a identidade do grao, e
    // sem ela o rastro nao se pode construir: os slots sao compactados a cada
    // publish, portanto o slot 0 deste bloco pode ser a voz 3 e no seguinte a
    // voz 1. Ligar um rastro a um slot，而不是 a uma voz, faria a cauda saltar
    // entre graos e dar um rasgo falso no ecra.
    //
    // Estavel por todo o tempo de vida do grao, porque `voices_[i]` e' um slot
    // fixo do motor. O valor -1 nao existe: quem le trata de voice < 0.
    int voice {-1};

    float position {0.0f}; ///< Fracao do material, 0..1.
    float gain {0.0f};     ///< 0..1, sem a rampa de saida.
    float phase {0.0f};    ///< 0..1 dentro da janela do grao.
};

// Publicacao do estado dos graos para a thread de interface.
//
// O display quer desenhar cada grao onde ele esta' a ler. Isso nao pode passar
// por um canal de mensagens: quem publica e' a thread de audio, e um canal com
// fila aloca. Logo e' memoria partilhada com atomicos.
//
// **Um atomico por campo, e nao um seqlock sobre uma struct simples.** O seqlock
// e' a resposta obvia e e' errada aqui: o leitor da interface copia a struct
// enquanto a thread de audio a escreve, e isso e' corrida de dados pelo padrao
// do C++, mesmo que funcione em x86. Passa a correr mal em TSan e em qualquer
// maquina onde o par store/load nao seja FETCH_MODIFY nem bloqueie o escritor.
// Com um atomico por campo nao ha corrida: cada campo e' lido ou escrito
// atomicamente e nada mais.
//
// O que se perde e' a coerencia *entre* campos: os tres numeros de um grao podem
// vir de dois blocos seguidos. Num decalque decorativo e' invisivel, e por isso
// quem le e' avisado so pelo `count`. **Ninguem mede nada com isto.** Se uma
// medicao precisa de coerencia, mede-se na thread de audio e publica-se o
// resultado ja fechado, como o Telemetry do processador faz com o pico.
//
// **O `count` tambem pode estar atrasado em relacao aos slots.** Um leitor que
// viu count = 8 e le depois de um publish seguinte ter escrito 3 slots desenha
// 5 graos do bloco anterior. Os valores ficam no intervalo e nunca ha indice a
// fora, por isso nao rebenta nada — mas e' a frase que diz ao codigo de
// desenho para nao ler o count como "os graos que existem agora".
//
// **Nao ha teste para publish e read a correr ao mesmo tempo.** A corrida nao e'
// reproduzivel a pedido: um teste com duas threads durante N iteracoes passa sem
// provar nada e falha quando calha. E' verificado por construcao — um atomico
// por campo e release/acquire no count — e nao por teste, porque a plataforma
// nao tem runtime de TSan. Excepcao declarada, nao escondida.
class GrainTelemetry {
public:
    // Igual a GranularEngine::kMaxVoices, verificado em baixo com static_assert.
    static constexpr int kMaxVoices = 8;

    // O portao C e' probatorio: um store que possa travar nao serve. Verificado
    // em build, e nao por confianca de quem le o codigo. No MSVC x64 um store
    // relaxado e' um MOVSS, mas isso e' propriedade desta plataforma e nao do
    // standard, e um tipo diferente aqui quebraria o silencio.
    static_assert(std::atomic<float>::is_always_lock_free,
                  "store relaxado nao pode travar: o portao C e' probatorio");
    static_assert(std::atomic<int>::is_always_lock_free,
                  "idem para o count, que e' o que o leitor sincroniza com");

    // Thread de audio. Uma vez por bloco, depois de processar.
    //
    // Os graos activos sao **compactados para a frente**, e nao escritos no seu
    // indice. O `count` que a interface le tem de poder servir de indice: se o
    // grao 0 estiver parado e os 1, 2 e 3 soarem, count = 3 e a interface le os
    // slots 0, 1 e 2 — que sem compactar seriam o grao parado e dois activos.
    void publish(const std::array<Grain, kMaxVoices>& voices) noexcept {
        int count = 0;

        for (std::size_t voice = 0; voice < voices.size(); ++voice) {
            const auto& grain = voices[voice];
            if (!grain.active) {
                continue;
            }

            // A identidade vai antes de tudo o resto. E' um store de `int` para
            // um slot cujo valor cabe em tres bits, e emparelha com o count do
            // mesmo publish: quem le um count novo sabe que a voz veio com ele.
            voice_[static_cast<std::size_t>(count)].store(static_cast<int>(voice),
                                                           std::memory_order_relaxed);

            // position ja vem normalizado em [0, 1] do startGrain, mas um grao
            // com pitch a subir avanca para alem de 1 e so volta a entrar na
            // leitura por um wrap. Publicar o valor cru punha o grao fora do ecra
            // a direita, que e' pior do que o mostrar colado a margem.
            //
            // isfinite antes do clamp, e nao depois: **std::clamp nao trata
            // NaN**, clamp(NaN) devolve NaN. Um NaN aqui sai pela mesma via que
            // o ganho e apaga o grao do ecra em silencio. Compila para tres
            // instrucoes de comparacao, nao para uma chamada a fpclassify.
            position_[static_cast<std::size_t>(count)].store(
                static_cast<float>(std::isfinite(grain.position)
                                       ? std::clamp(grain.position, 0.0, 1.0)
                                       : 0.0),
                std::memory_order_relaxed);

            // isfinite antes de publicar. std::clamp nao trata NaN — clamp(NaN)
            // devolve NaN — e um NaN aqui propaga-se em silencio para a posicao
            // do desenho e apaga o unico sinal de que ha graos. Hoje o motor nao
            // produz isto, porque o startGrain limita as entradas e o readStep so
            // e' finito quando sourceCount_ > 0, mas um store de NaN e' um
            // pixel vagabundo e custa uma comparacao desfazer.
            gain_[static_cast<std::size_t>(count)].store(
                std::isfinite(grain.gain) ? grain.gain : 0.0f,
                std::memory_order_relaxed);

            // A fase sai dos dois contadores sem guardar o comprimento do grao:
            // windowIndex e remaining sobem e descem juntos a partir de
            // activeWindowLength_, portanto a soma e' o comprimento original.
            // Um grao terminado tem os dois a zero, e ai a divisao daria 0/0.
            const auto total = grain.windowIndex + grain.remaining;
            phase_[static_cast<std::size_t>(count)].store(
                total > 0 ? static_cast<float>(grain.windowIndex) / static_cast<float>(total)
                          : 0.0f,
                std::memory_order_relaxed);

            ++count;
        }

        // Release, e o leitor usa acquire: quem viu este count viu os campos
        // publicados antes dele.
        count_.store(count, std::memory_order_release);
    }

    // Thread de interface. Devolve quantos slots de `out` preencher, e preenche
    // so esses. Zero e' um resultado valido e nao um erro.
    [[nodiscard]] int read(std::array<GrainView, kMaxVoices>& out) const noexcept {
        const auto count = count_.load(std::memory_order_acquire);
        if (count <= 0) {
            return 0;
        }

        const auto toRead = std::min(count, kMaxVoices);
        for (int i = 0; i < toRead; ++i) {
            const auto index = static_cast<std::size_t>(i);
            out[static_cast<std::size_t>(i)] = GrainView {
                voice_[index].load(std::memory_order_relaxed),
                position_[index].load(std::memory_order_relaxed),
                gain_[index].load(std::memory_order_relaxed),
                phase_[index].load(std::memory_order_relaxed)};
        }
        return toRead;
    }

    private:
    std::array<std::atomic<int>, kMaxVoices> voice_ {};
    std::array<std::atomic<float>, kMaxVoices> position_ {};
    std::array<std::atomic<float>, kMaxVoices> gain_ {};
    std::array<std::atomic<float>, kMaxVoices> phase_ {};
    std::atomic<int> count_ {0};
};

class GranularEngine {
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxGrainSamples = 48000;
    static constexpr float kMaxGrainMs = 100.0f;
    static constexpr float kMinGrainMs = 1.0f;

// Numero de pontos da curva de entropia. Publico porque quem calcula a curva na
    // thread de interface tem de saber quantos pontos o motor aceita.
    static constexpr std::size_t kMaxEntropyPoints {1024};

    // A telemetria tem o seu proprio kMaxVoices para nao depender da ordem de
    // declaracao das duas classes. Se os dois deixarem de bater certo, o
    // read() comeca a devolver um numero que o publish nunca escreveu.
    static_assert(GrainTelemetry::kMaxVoices == kMaxVoices,
                  "GrainTelemetry::kMaxVoices tem de ser igual a kMaxVoices");

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setSampleRate(double sampleRate) noexcept { sampleRate_ = sampleRate; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    // **Thread de audio apenas.** Chamado de PluginProcessor::drainIncomingQueue,
    // dentro de processBlock. A interface publica material pela fila SPSC e nunca
    // chama isto: um seek e' uma mensagem, nao uma chamada.
    //
    // O contrato importa porque este metodo escreve estado que pertence a thread de
    // audio — `source_`, `sourceCount_`, `entropyCurve_`, e agora tambem `voices_` —
    // e nenhum desses campos e' atomico. Uma chamada do lado da interface seria
    // corrida em todos eles, e o compilador nao avisa. Um "fechar ficheiro" do lado
    // da interface e' uma mensagem pela fila, nunca esta chamada directa.
    void setSource(const float* samples, std::size_t count) noexcept;

    // **Thread de audio apenas**, como a sobrecarga de dois argumentos.
    //
    // Publica o material E a curva de entropia ja reduzida, que e' o que a thread de
    // interface deve passar.
    //
    // **A curva nunca e' calculada na thread de audio.** A versao de um argumento
    // calcula-a, e isso e' O(region) dentro de processBlock: para 12 MB sao 12
    // milhoes de leituras de float, e nao aloca nada — o array do histograma e' de
    // pilha — pelo que o guard de alocacao passa e e' cego para isto. Com o
    // transporte, arrastar a regiao republica o material a cada evento de rato, e
    // cada publicacao era uma passagem completa na thread de audio. Varios ms de
    // pico por evento e' xrun.
    //
    // Quem calcula e' pe::reduceToColumns, na thread de interface, e a curva que
    // entra aqui tem no maximo kMaxEntropyPoints pontos. Se for nullptr, o motor usa
    // 4,0 bits, que e' o valor medio de um executavel, e nao calcula nada.
    void setSource(const float* samples,
                   std::size_t count,
                   const float* entropyCurve,
                   std::size_t entropyPoints) noexcept;

    [[nodiscard]] bool hasSource() const noexcept { return source_ != nullptr; }
    [[nodiscard]] std::size_t sourceSize() const noexcept { return sourceCount_; }

    // Gate com rampa linear, para as notas ligarem e desligarem sem estalo.
    //
    // Abrir e fechar a saida de uma vez produz um degrau no sinal, e um degrau
    // e' um transiente largo em frequencia: o ataque e' audivel mesmo com
    // release curto. A rampa de 5 ms custa duas multiplicacoes por amostra e
    // remove o problema.
    void setSounding(bool on) noexcept { gateTarget_ = on ? 1.0f : 0.0f; }
    [[nodiscard]] bool isSounding() const noexcept { return gateTarget_ > 0.5f; }
    void setGateSeconds(double seconds) noexcept;

    // Nivel corrente da rampa. Existe para o teste verificar a rampa em si,
    // sem depender do agendamento de graos, que precisa de dezenas de blocos
    // para produzir audio e tornaria a medicao lenta e fragil.
    [[nodiscard]] float gateLevel() const noexcept { return gateLevel_; }

    void processBlock(float* left, float* right, int numSamples,
                      const GranularParams& params) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] int lastActiveVoices() const noexcept { return lastActiveVoices_; }

    // Estado dos graos para o display. Quem escreve e' esta classe, no fim de
    // cada bloco; quem le e' a thread de interface, que nunca deve processar um
    // bloco para o saber.
    [[nodiscard]] const GrainTelemetry& telemetry() const noexcept { return telemetry_; }

private:
    void rebuildWindow(const GranularParams& params) noexcept;
    void startGrain(int voice, const GranularParams& params, double entropy) noexcept;
    void applyGate(float* left, float* right, int numSamples) noexcept;


    double sampleRate_ {44100.0};

    const float* source_ {nullptr};
    std::size_t sourceCount_ {0};

    std::array<Grain, kMaxVoices> voices_ {};

    std::array<float, kMaxGrainSamples> windowA_ {};
    std::array<float, kMaxGrainSamples> windowB_ {};
    const float* activeWindow_ {nullptr};
    int activeWindowLength_ {0};
    WindowType activeWindowType_ {WindowType::kHann};

    double spawnAccumulator_ {0.0};

    std::array<float, kMaxEntropyPoints> entropyCurve_ {};
    std::size_t entropyPointCount_ {0};

    DcBlocker dcBlockerLeft_ {};
    DcBlocker dcBlockerRight_ {};
    Limiter limiterLeft_ {};
    Limiter limiterRight_ {};

    float gateLevel_ {0.0f};
    float gateTarget_ {0.0f};
    float gateStep_ {0.0f};

    int lastActiveVoices_ {0};

    GrainTelemetry telemetry_ {};
};

} // namespace opcoda::dsp
