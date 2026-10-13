#include "TuningUtils.h"
#include <array>
#include <cmath>

namespace
{
    // 2^(n/12) for n = 0..11
    constexpr std::array<float, 12> equalTemperedRatios {
        1.0f,                 //  0  unison
        1.0594630943592953f,  //  1  minor 2nd
        1.1224620483093730f,  //  2  major 2nd
        1.1892071150027210f,  //  3  minor 3rd
        1.2599210498948732f,  //  4  major 3rd
        1.3348398541700344f,  //  5  perfect 4th
        1.4142135623730951f,  //  6  tritone
        1.4983070768766815f,  //  7  perfect 5th
        1.5874010519681994f,  //  8  minor 6th
        1.6817928305074290f,  //  9  major 6th
        1.7817974362806785f,  // 10  minor 7th
        1.8877486253633868f   // 11  major 7th
    };
}

float TuningUtils::getEqualTemperedRatio (int pitchClass)
{
    const int wrapped = ((pitchClass % 12) + 12) % 12;
    return equalTemperedRatios[static_cast<std::size_t> (wrapped)];
}

const char* TuningUtils::getIntervalName (int semitones)
{
    static constexpr const char* names[] { "Uni", "m2", "M2", "m3", "M3", "P4", "TT",
                                           "P5", "m6", "M6", "m7", "M7", "Oct" };
    const int size = semitones < 0 ? -semitones : semitones;
    return size <= 12 ? names[size] : "";
}

float TuningUtils::frequencyToMidiNote (float hz)
{
    return 69.0f + 12.0f * std::log2 (hz / 440.0f);
}
