#include "HarmonizerEngine.h"

void HarmonizerEngine::prepare(double sampleRate, int maxBlockSize)
{
    double currSampleRate = sampleRate;
    using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
    Stretch stretch;
    detector.prepare(sampleRate, 1);
    for(int i = 0; i < DspConfig::maxVoices; i++){
        voices[i].prepare(sampleRate, maxBlockSize);
    }
}

void HarmonizerEngine::process(juce::AudioBuffer<float> &buffer, int startSample, int numSamples)
{
    input = juce::AudioBuffer(buffer);
    /*
    get the intervals that it needs to be pitched by. assign each to a harmony voice. 
    harmony voices will be initialized ahead of time. All summed together and scaled then output.
    */
}

void HarmonizerEngine::release()
{
    // TODO
}
