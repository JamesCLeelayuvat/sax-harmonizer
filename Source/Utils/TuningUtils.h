#pragma once

//==============================================================================
/*
    12-TET frequency ratios relative to the root, for -12..+12 semitones.
*/
namespace TuningUtils
{
    // Returns 2^(semitones / 12) for -12..12 (e.g. -12 -> 0.5, 7 -> 1.498,
    // 12 -> 2.0). Values outside that range are clamped to +/-12.
    float getEqualTemperedRatio (float semitones);

    // Short name for an interval size, ignoring direction: 4 and -4 both give "M3".
    // Covers 0-12 semitones ("Uni" ... "Oct"); larger intervals give "".
    const char* getIntervalName (int semitones);

    // Fractional MIDI note number for a frequency (A4 = 440 Hz = 69)
    float frequencyToMidiNote (float hz);
}
