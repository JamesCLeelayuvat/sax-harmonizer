#pragma once

#include <JuceHeader.h>
#include "HarmonyVoice.h"
#include "PitchDetector.h"
#include "../Midi/IntervalController.h"

//==============================================================================
/*
    All audio processing, no GUI code. MainComponent owns one of these and
    forwards the audio callbacks to it.
*/
class HarmonizerEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void release();

private:
    PitchDetector detector;
    IntervalController intervals;
    // TODO: HarmonyVoice(s)
};
