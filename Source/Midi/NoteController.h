#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

//==============================================================================
/*
    Decides which interval each harmony voice plays.

    Relative mode: each keyboard key is an interval from whatever note comes in,
    measured from unisonNote (C5). Selecting E5 gives a voice a major 3rd (+4)
    above the input, A4 a minor 3rd (-3) below it, whatever you play. The shift
    doesn't depend on the detected pitch. Everything is atomic, so the GUI and
    the audio thread can both use it without locks.

    Notes are MIDI note numbers (60 = middle C, C4). The transposition (Concert,
    Bb or Eb) is still stored, but intervals are the same in every transposition,
    so it no longer affects the shift.

    Voices are numbered in ascending note order (= ascending interval): with
    A4 and E5 selected, voice 0 plays -3 and voice 1 plays +4.
*/
class NoteController
{
public:
    // Keyboard range C4..C6: one octave each side of the unison key, which is as
    // far as the shifter goes (TuningUtils clamps to +/-12 semitones)
    static constexpr int lowestNote = 60;
    static constexpr int highestNote = 84;
    static constexpr int unisonNote = 72; // C5 = no shift
    static constexpr int maxVoices = 4;

    // Semitones from the unison key, e.g. 76 (E5) -> +4
    static int getIntervalFor(int note);

    enum class Transposition
    {
        concert, // written = sounding
        bFlat,   // soprano sax: sounds a major 2nd lower
        eFlat    // alto sax: sounds a major 6th lower
    };

    // Semitones from written to concert pitch (0, -2 or -9)
    static int getConcertOffset(Transposition transposition);

    // GUI thread. Returns false (and changes nothing) if the note is out of
    // range or would exceed maxVoices.
    bool setNoteEnabled(int writtenNote, bool enabled);
    bool isNoteEnabled(int writtenNote) const;
    void clear();

    // Any thread
    void setTransposition(Transposition newTransposition);
    Transposition getTransposition() const;
    int getNumActiveVoices() const;

    // Note for voice `voiceIndex` (0 .. getNumActiveVoices() - 1), or -1 for an
    // inactive voice.
    int getWrittenNoteFor(int voiceIndex) const;
    int getConcertNoteFor(int voiceIndex) const;

    // Semitones to shift the input by for voice `voiceIndex` (its interval).
    // Returns 0 for an inactive voice.
    float getSemitoneShiftFor(int voiceIndex) const;

private:
    static bool isInRange(int writtenNote);
    static std::uint64_t bitFor(int writtenNote);

    std::atomic<std::uint64_t> enabledMask{0}; // bit i = written note lowestNote + i
    std::atomic<Transposition> transposition{Transposition::concert};
};
