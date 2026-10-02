#include "opcoda_core/rt/note_tracker.h"

#include <algorithm>

namespace opcoda::rt {

void NoteTracker::handle(const Event& event) noexcept {
    switch (event.kind) {
        case Event::Kind::noteOn:
            ++pressed_;
            break;

        case Event::Kind::noteOff:
            // O pedal decide onde a nota vai parar, nao a tecla.
            if (sustain_ && pressed_ > 0) {
                --pressed_;
                ++sustained_;
            } else {
                --pressed_;
            }
            break;

        case Event::Kind::sustain:
            sustain_ = event.value >= 64;
            // Soltar o pedal solta tudo o que estava preso. As teclas ainda
            // premidas continuam a soar.
            if (!sustain_) {
                sustained_ = 0;
            }
            break;

        case Event::Kind::allNotesOff:
            pressed_ = 0;
            sustained_ = 0;
            sustain_ = false;
            break;

        case Event::Kind::controller:
        case Event::Kind::other:
            break;
    }

    // Uma nota-off sem nota-on correspondente vem de hosts que reenviam o
    // estado inicial no primeiro bloco. Deixar a contagem negativa faria
    // sounding() mentir e o motor ficaria calado de forma permanente.
    pressed_ = std::max(pressed_, 0);
    sustained_ = std::max(sustained_, 0);
    sounding_ = pressed_ > 0 || sustained_ > 0;
}

void NoteTracker::reset() noexcept {
    pressed_ = 0;
    sustained_ = 0;
    sustain_ = false;
    sounding_ = false;
    // Tambem os valores de CC: reset significa esquecer tudo. Deixa-los para
    // tras faria o proximo CC chegar ao host com um valor de um estado
    // anterior, invisivel para quem depura.
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