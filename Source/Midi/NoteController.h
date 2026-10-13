#pragma once

#include <atomic>
#include <cstdint>

//==============================================================================
/*
    Decides which note each harmony voice should sound.

    The GUI turns notes on and off on a keyboard of *written* (fingered) sax
    notes, and picks the transposition (Concert, Bb or Eb). The audio thread asks
    which concert note each voice plays, or directly how many semitones to shift
    the input by to reach it. Everything is atomic, so both threads can use it
    without locks.

    Notes are MIDI note numbers (60 = middle C, C4). Written note + transposition
    offset = concert (sounding) note: on an Eb alto, written C5 (72) sounds
    Eb4 (72 - 9 = 63).

    Voices are numbered in ascending note order: with written E4 and G4 selected,
    voice 0 plays E4 and voice 1 plays G4.
*/
class NoteController
{
public:
    // Written range shown on the keyboard: A3..G6, which covers the whole sax
    // range (low Bb3 to high F#6) and starts and ends on a white key
    static constexpr int lowestNote = 57;
    static constexpr int highestNote = 91;
    static constexpr int maxVoices = 4;

    enum class Transposition
    {
        concert, // written = sounding
        bFlat,   // soprano sax: sounds a major 2nd lower
        eFlat    // alto sax: sounds a major 6th lower
    };

    // Semitones from written to concert pitch (0, -2 or -9)
    static int getConcertOffset (Transposition transposition);

    // GUI thread. Returns false (and changes nothing) if the note is out of
    // range or would exceed maxVoices.
    bool setNoteEnabled (int writtenNote, bool enabled);
    bool isNoteEnabled (int writtenNote) const;
    void clear();

    // Any thread
    void setTransposition (Transposition newTransposition);
    Transposition getTransposition() const;
    int getNumActiveVoices() const;

    // Note for voice `voiceIndex` (0 .. getNumActiveVoices() - 1), or -1 for an
    // inactive voice.
    int getWrittenNoteFor (int voiceIndex) const;
    int getConcertNoteFor (int voiceIndex) const;

    // Semitones to shift an input at inputFrequency (Hz) so it lands on voice
    // `voiceIndex`'s concert note. Can be fractional, which also corrects an
    // out-of-tune input. Returns 0 for an inactive voice or no input (<= 0 Hz).
    float getSemitoneShiftFor (int voiceIndex, float inputFrequency) const;

private:
    static bool isInRange (int writtenNote);
    static std::uint64_t bitFor (int writtenNote);

    std::atomic<std::uint64_t> enabledMask { 0 }; // bit i = written note lowestNote + i
    std::atomic<Transposition> transposition { Transposition::concert };
};
