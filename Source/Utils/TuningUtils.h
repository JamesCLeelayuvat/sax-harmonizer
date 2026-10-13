#pragma once

//==============================================================================
/*
    12-TET frequency ratios relative to the root, indexed by pitch class (0-11).
*/
namespace TuningUtils
{
    // Returns 2^(pitchClass / 12). pitchClass is wrapped into 0-11, so octave
    // information is dropped (e.g. 12 -> 1.0, -1 -> ratio for 11).
    float getEqualTemperedRatio (int pitchClass);

    // Short name for an interval size, ignoring direction: 4 and -4 both give "M3".
    // Covers 0-12 semitones ("Uni" ... "Oct"); larger intervals give "".
    const char* getIntervalName (int semitones);

    // Fractional MIDI note number for a frequency (A4 = 440 Hz = 69)
    float frequencyToMidiNote (float hz);
}
