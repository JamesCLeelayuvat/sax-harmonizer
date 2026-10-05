#include "HarmonizerEngine.h"

void HarmonizerEngine::prepare (double sampleRate, int maxBlockSize)
{
    double currSampleRate = sampleRate;
    using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
    Stretch stretch;
}

void HarmonizerEngine::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{


}


void HarmonizerEngine::release()
{
    // TODO
}
