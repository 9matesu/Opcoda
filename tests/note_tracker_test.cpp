#include <gtest/gtest.h>

#include "opcoda_core/rt/note_tracker.h"

using opcoda::rt::NoteTracker;

namespace {
NoteTracker::Event noteOn(int number) { return {NoteTracker::Event::Kind::noteOn, number, 100}; }
NoteTracker::Event noteOff(int number) { return {NoteTracker::Event::Kind::noteOff, number, 0}; }
NoteTracker::Event sustain(bool down) {
    return {NoteTracker::Event::Kind::sustain, 64, down ? 127 : 0};
}
NoteTracker::Event allNotesOff() { return {NoteTracker::Event::Kind::allNotesOff, 0, 0}; }
} // namespace

TEST(NoteTracker, SilentWithoutNotes) {
    NoteTracker tracker;
    EXPECT_FALSE(tracker.sounding());
    EXPECT_EQ(tracker.held(), 0);
}

TEST(NoteTracker, NoteOnSoundsAndNoteOffStops) {
    NoteTracker tracker;
    tracker.handle(noteOn(60));
    EXPECT_TRUE(tracker.sounding());

    tracker.handle(noteOff(60));
    EXPECT_FALSE(tracker.sounding());
}

TEST(NoteTracker, CountsChords) {
    NoteTracker tracker;
    tracker.handle(noteOn(60));
    tracker.handle(noteOn(64));
    tracker.handle(noteOn(67));
    EXPECT_TRUE(tracker.sounding());

    // Uma nota-off de cada: so depois da ultima o som deve parar.
    tracker.handle(noteOff(60));
    tracker.handle(noteOff(64));
    EXPECT_TRUE(tracker.sounding()) << "o acorde parou antes de todas as notas largarem";
    tracker.handle(noteOff(67));
    EXPECT_FALSE(tracker.sounding());
}

TEST(NoteTracker, StrayNoteOffCannotSilenceForever) {
    // Hostes reenviam o estado inicial no primeiro bloco, e alguns mandam
    // note-off sem note-on. Sem o piso em zero, sounding() ficaria falso para
    // sempre e o instrumento nunca mais tocaria.
    NoteTracker tracker;
    for (int i = 0; i < 5; ++i) {
        tracker.handle(noteOff(60));
    }
    EXPECT_FALSE(tracker.sounding());

    tracker.handle(noteOn(60));
    EXPECT_TRUE(tracker.sounding()) << "o tracker ficou travado depois de note-offs soltos";
}

TEST(NoteTracker, SustainKeepsSoundingAfterKeyRelease) {
    NoteTracker tracker;
    tracker.handle(sustain(true));
    tracker.handle(noteOn(60));
    tracker.handle(noteOff(60));
    EXPECT_TRUE(tracker.sounding()) << "o pedal deveria segurar a nota";

    tracker.handle(sustain(false));
    EXPECT_FALSE(tracker.sounding()) << "soltar o pedal deveria soltar a nota";
}

TEST(NoteTracker, SustainDoesNotHideStillHeldKeys) {
    // Soltar o pedal enquanto uma tecla continua premida: o som tem de ficar.
    NoteTracker tracker;
    tracker.handle(noteOn(60));
    tracker.handle(noteOn(64));
    tracker.handle(sustain(true));
    tracker.handle(noteOff(60));
    tracker.handle(sustain(false));

    EXPECT_TRUE(tracker.sounding()) << "a nota ainda premida foi silenciada com o pedal";
    tracker.handle(noteOff(64));
    EXPECT_FALSE(tracker.sounding());
}

TEST(NoteTracker, AllNotesOffClearsSustain) {
    NoteTracker tracker;
    tracker.handle(sustain(true));
    tracker.handle(noteOn(60));
    tracker.handle(noteOff(60));
    tracker.handle(allNotesOff());

    EXPECT_FALSE(tracker.sounding());
    EXPECT_FALSE(tracker.sounding()) << "all notes off tem de vencer o pedal";
}

TEST(NoteTracker, ControllersAreClampedAndFlagged) {
    NoteTracker tracker;
    EXPECT_FALSE(tracker.controllersChanged());
    EXPECT_FLOAT_EQ(tracker.controller(0), 0.0f);

    tracker.setController(0, 0.5f);
    EXPECT_TRUE(tracker.controllersChanged());
    EXPECT_FLOAT_EQ(tracker.controller(0), 0.5f);

    tracker.clearControllersChanged();
    EXPECT_FALSE(tracker.controllersChanged());

    tracker.setController(0, 4.0f);
    EXPECT_FLOAT_EQ(tracker.controller(0), 1.0f) << "valor fora de [0,1] nao foi limitado";
    tracker.setController(0, -3.0f);
    EXPECT_FLOAT_EQ(tracker.controller(0), 0.0f);
}

TEST(NoteTracker, OutOfRangeControllerIndexIsIgnored) {
    // Índice invalido nao pode escrever em memoria fora do array.
    NoteTracker tracker;
    tracker.setController(-1, 0.9f);
    tracker.setController(NoteTracker::kTrackedControllers, 0.9f);
    tracker.setController(9999, 0.9f);

    EXPECT_FLOAT_EQ(tracker.controller(-1), 0.0f);
    EXPECT_FLOAT_EQ(tracker.controller(NoteTracker::kTrackedControllers), 0.0f);
}

TEST(NoteTracker, ResetClearsEverything) {
    NoteTracker tracker;
    tracker.handle(sustain(true));
    tracker.handle(noteOn(60));
    tracker.setController(1, 0.7f);
    tracker.reset();

    EXPECT_FALSE(tracker.sounding());
    EXPECT_FLOAT_EQ(tracker.controller(1), 0.0f);
    EXPECT_FALSE(tracker.controllersChanged());
}