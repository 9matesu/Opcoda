#include "opcoda_core/rt/note_tracker.h"

#include <algorithm>

namespace opcoda::rt {

void NoteTracker::pushHeld(int note) noexcept {
    // retrigger da mesma nota move-a para o topo em vez de duplicar: senao o
    // fallback mentiria com a pilha cheia de copias.
    removeHeld(note);
    if (heldCount_ >= kMaxHeldNotes) {
        for (int i = 1; i < kMaxHeldNotes; ++i) {
            heldNotes_[static_cast<std::size_t>(i - 1)] = heldNotes_[static_cast<std::size_t>(i)];
        }
        --heldCount_;
    }
    heldNotes_[static_cast<std::size_t>(heldCount_)] = note;
    ++heldCount_;
}

void NoteTracker::removeHeld(int note) noexcept {
    int write = 0;
    for (int read = 0; read < heldCount_; ++read) {
        if (heldNotes_[static_cast<std::size_t>(read)] != note) {
            heldNotes_[static_cast<std::size_t>(write)] = heldNotes_[static_cast<std::size_t>(read)];
            ++write;
        }
    }
    heldCount_ = write;
}

bool NoteTracker::isHeld(int note) const noexcept {
    for (int i = 0; i < heldCount_; ++i) {
        if (heldNotes_[static_cast<std::size_t>(i)] == note) {
            return true;
        }
    }
    return false;
}

void NoteTracker::pushSustained(int note) noexcept {
    if (sustainedCount_ >= kMaxHeldNotes) {
        for (int i = 1; i < kMaxHeldNotes; ++i) {
            sustainedNotes_[static_cast<std::size_t>(i - 1)] =
                sustainedNotes_[static_cast<std::size_t>(i)];
        }
        --sustainedCount_;
    }
    sustainedNotes_[static_cast<std::size_t>(sustainedCount_)] = note;
    ++sustainedCount_;
}

void NoteTracker::handle(const Event& event) noexcept {
    switch (event.kind) {
        case Event::Kind::noteOn: {
            const int note = event.number & 0x7F;
            pushHeld(note);
            activeNote_ = note;
            // a velocidade da ultima tecla e' fonte da matriz, e e' a que dispara
            // o ataque no instrumento mono. notaOff nao a apaga, senao a cauda
            // perdia a fonte a meio.
            activeVelocity_ = std::clamp(static_cast<float>(event.value & 0x7F) / 127.0f, 0.0f, 1.0f);
            ++pressed_;
            break;
        }

        case Event::Kind::noteOff: {
            const int note = event.number & 0x7F;
            // o pedal decide onde a nota vai parar, nao a tecla.
            if (sustain_ && pressed_ > 0) {
                const bool wasHeld = isHeld(note);
                removeHeld(note);
                if (wasHeld) {
                    pushSustained(note);
                }
                --pressed_;
                ++sustained_;
                // mantem a altura: mudar ao largar a tecla seria um salto sem
                // nota nova.
            } else {
                removeHeld(note);
                if (activeNote_ == note && heldCount_ > 0) {
                    activeNote_ = heldNotes_[static_cast<std::size_t>(heldCount_ - 1)];
                }
                --pressed_;
            }
            break;
        }

        case Event::Kind::sustain:
            sustain_ = event.value >= 64;
            // soltar o pedal solta tudo o que estava preso. as teclas ainda
            // premidas continuam a soar, e a ultima delas volta a mandar.
            if (!sustain_) {
                sustained_ = 0;
                sustainedCount_ = 0;
                if (heldCount_ > 0) {
                    activeNote_ = heldNotes_[static_cast<std::size_t>(heldCount_ - 1)];
                }
            }
            break;

        case Event::Kind::allNotesOff:
            pressed_ = 0;
            sustained_ = 0;
            sustain_ = false;
            heldCount_ = 0;
            sustainedCount_ = 0;
            activeNote_ = 60;
            break;

        case Event::Kind::controller:
        case Event::Kind::other:
            break;
    }

    // nota-off sem nota-on vem de hosts que reenviam o estado inicial no primeiro
    // bloco. contagem negativa faria sounding() mentir e o motor ficava calado.
    pressed_ = std::max(pressed_, 0);
    sustained_ = std::max(sustained_, 0);
    sounding_ = pressed_ > 0 || sustained_ > 0;
}

void NoteTracker::reset() noexcept {
    pressed_ = 0;
    sustained_ = 0;
    sustain_ = false;
    sounding_ = false;
    heldCount_ = 0;
    sustainedCount_ = 0;
    activeNote_ = 60;
    activeVelocity_ = 1.0f;
    // reset esquece tambem os CCs. deixar valores para tras faria o proximo CC
    // chegar ao host com estado de antes, invisivel para quem depura.
    for (auto& value : controllers_) {
        value = 0.0f;
    }
    controllersChanged_ = false;
}

float NoteTracker::controller(int index) const noexcept {
    if (index < 0 || index >= kTrackedControllers) {
        return 0.0f;
    }
    return controllers_[index];
}

void NoteTracker::setController(int index, float normalised) noexcept {
    if (index < 0 || index >= kTrackedControllers) {
        return;
    }
    controllers_[index] = std::clamp(normalised, 0.0f, 1.0f);
    controllersChanged_ = true;
}

} // namespace opcoda::rt