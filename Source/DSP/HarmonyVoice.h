#pragma once

#include "PitchShifter.h"

//==============================================================================
/*
    One harmony line: a pitch-shifted copy of the input at some interval.
*/
class HarmonyVoice
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, int numSamples);

private:
    PitchShifter shifter;
};
