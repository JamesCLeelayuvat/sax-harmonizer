#pragma once

#include <JuceHeader.h>
#include <signalsmith-stretch/signalsmith-stretch.h>

//==============================================================================
/*
    Wraps SignalsmithStretch for real-time pitch shifting.
*/
class PitchShifter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setSemitones (float semitones);
    void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, int numSamples);

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double currSampleRate;
    int currNumChannels;
};
