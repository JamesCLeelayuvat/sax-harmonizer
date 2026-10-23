#include "TuningUtils.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    // 2^(n/12) for n = -12..12, stored at index n + 12
    constexpr int maxSemitones = 12;
   
}

float TuningUtils::getEqualTemperedRatio(float semitones)
{
    const float clamped = std::clamp(semitones, (float)-maxSemitones, (float)maxSemitones);
    return std::pow(2.0f, (clamped / 12.0f));
}

const char *TuningUtils::getIntervalName(int semitones)
{
    static constexpr const char *names[]{"Uni", "m2", "M2", "m3", "M3", "P4", "TT",
                                         "P5", "m6", "M6", "m7", "M7", "Oct"};
    const int size = semitones < 0 ? -semitones : semitones;
    return size <= 12 ? names[size] : "";
}

float TuningUtils::frequencyToMidiNote(float hz)
{
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}
