#include "PitchShifter.h"

void PitchShifter::prepare(double sampleRate, int numChannels)
{
    currNumChannels = numChannels;
    currSampleRate = sampleRate;
}

void PitchShifter::reset()
{
    // TODO
}

void PitchShifter::setSemitones(float semitones)
{
    // TODO
}

void PitchShifter::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples)
{
   
}
