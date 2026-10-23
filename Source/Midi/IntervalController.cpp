#include "IntervalController.h"

namespace
{
    int countBits (std::uint32_t mask)
    {
        int count = 0;
        for (; mask != 0; mask &= mask - 1) // clears the lowest set bit each time
            ++count;
        return count;
    }
}

bool IntervalController::isInRange (int semitones)
{
    return semitones >= minInterval && semitones <= maxInterval;
}

std::uint32_t IntervalController::bitFor (int semitones)
{
    return std::uint32_t { 1 } << (semitones - minInterval);
}

bool IntervalController::setIntervalEnabled (int semitones, bool enabled)
{
    if (! isInRange (semitones) || semitones == 0)
        return false;

    const auto mask = enabledMask.load();

    if (enabled)
    {
        if ((mask & bitFor (semitones)) == 0 && countBits (mask) >= maxVoices)
            return false;

        enabledMask = mask | bitFor (semitones);
    }
    else
    {
        enabledMask = mask & ~bitFor (semitones);
    }

    return true;
}

bool IntervalController::isIntervalEnabled (int semitones) const
{
    return isInRange (semitones) && (enabledMask.load() & bitFor (semitones)) != 0;
}


void IntervalController::clear()
{
    enabledMask = 0;
}

int IntervalController::getNumActiveVoices() const
{
    return countBits (enabledMask.load());
}

float IntervalController::getIntervalFor (int voiceIndex, float inputFrequency) const
{
    (void) inputFrequency;

    const auto mask = enabledMask.load();
    int voice = 0;

    for (int semitones = minInterval; semitones <= maxInterval; ++semitones)
    {
        if ((mask & bitFor (semitones)) == 0)
            continue;

        if (voice == voiceIndex)
            return (float) semitones;

        ++voice;
    }

    return 0.0f;
}
