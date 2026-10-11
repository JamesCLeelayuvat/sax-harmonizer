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
}
