#include "HarmonizerEngine.h"

void HarmonizerEngine::prepare(double sampleRate, int maxBlockSize)
{
    detector.prepare(sampleRate, 1);
    input.setSize(1, maxBlockSize);
    voiceOutput.setSize(1, maxBlockSize);
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

    detector.pushSamples(input, numSamples);
    parameters.detectedFrequency = detector.getFrequency();

    outputCombined.clear(0, 0, numSamples);
    for (int i = 0; i < notes.getNumActiveVoices(); i++)
    {
        voices[i].setSemitones(notes.getSemitoneShiftFor(i));
        voices[i].process(input, voiceOutput, numSamples);
        outputCombined.addFrom(0, 0, voiceOutput, 0, 0, numSamples);
    }

    // ISOLATION TEST: drop the dry signal so only the shifted voices are output.
    // Restore with: buffer.applyGain(0, startSample, numSamples, parameters.dryGain);
    buffer.clear(0, startSample, numSamples);
    outputCombined.applyGain(0, 0, numSamples, parameters.harmonyGain);
    buffer.addFrom(0, startSample, outputCombined, 0, 0, numSamples, 1.);
    // ISOLATION TEST: normalise by voices only (no dry). Restore: 1.0f / (notes.getNumActiveVoices() + 1.0f)
    buffer.applyGain(0, startSample, numSamples, 1.0f / std::max(1, notes.getNumActiveVoices()));
    /*
    get the intervals that it needs to be pitched by. assign each to a harmony voice.
    harmony voices will be initialized ahead of time. All summed together and scaled then output.
    */
}

void HarmonizerEngine::release()
{
    // TODO
}
