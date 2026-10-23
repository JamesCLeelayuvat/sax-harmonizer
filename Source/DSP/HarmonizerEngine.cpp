#include "HarmonizerEngine.h"

void HarmonizerEngine::prepare(double sampleRate, int maxBlockSize)
{
    detector.prepare(sampleRate, 1);
    input.setSize(1, maxBlockSize);
    output.setSize(1, maxBlockSize);
    outputCombined.setSize(1, maxBlockSize);
    outputCombined.clear();
    for (int i = 0; i < DspConfig::maxVoices; i++)
    {
        voices[i].prepare(sampleRate, 1);
    }
}

void HarmonizerEngine::process(juce::AudioBuffer<float> &buffer, int startSample, int numSamples)
{
    input.copyFrom(0, 0, buffer, 0, startSample, numSamples);
    HarmonizerEngine::detector.pushSamples(input, numSamples);
    frequency = detector.getFrequency();
    parameters.detectedFrequency = frequency;

    buffer.applyGain(0, startSample, numSamples, parameters.dryGain);
    for (int i = 0; i < notes.getNumActiveVoices(); i++)
    {
        voices[i].setSemitones(notes.getSemitoneShiftFor(i, frequency));
        voices[i].process(input, output, numSamples);
        outputCombined.addFrom(0, 0, output, 0, 0, numSamples, 1.);
    }
    outputCombined.applyGain(parameters.harmonyGain);
    buffer.addFrom(0, startSample, outputCombined, 0, 0, numSamples, 1.);
    buffer.applyGain(0, startSample, numSamples, 1.0f / (notes.getNumActiveVoices() + 1.0f));
    outputCombined.clear();
    /*
    get the intervals that it needs to be pitched by. assign each to a harmony voice.
    harmony voices will be initialized ahead of time. All summed together and scaled then output.
    */
}

void HarmonizerEngine::release()
{
    // TODO
}
