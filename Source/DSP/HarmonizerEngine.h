#pragma once

#include <JuceHeader.h>
#include "PitchShifter.h"
#include "HarmonizerParameters.h"
#include "PitchDetector.h"
#include "../Midi/NoteController.h"
#include "DspConfig.h"

//==============================================================================
/*
    All audio processing, no GUI code. MainComponent owns one of these and
    forwards the audio callbacks to it.

    Channel contract with MainComponent: process() gets the mono mic signal in
    channel 0 and must leave its mono mix (dry + harmonies) in channel 0.
    MainComponent then applies the master gain, meters the result, and copies
    channel 0 to the other output channels (both speakers).

    The GUI talks to the engine only through getNoteController() (which
    harmony notes are selected) and getParameters() (gains in, detected
    pitch out). Both are lock-free, so they're safe to read in process().
*/
class HarmonizerEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float> &buffer, int startSample, int numSamples);
    void release();

    NoteController &getNoteController() { return notes; }
    HarmonizerParameters &getParameters() { return parameters; }

private:
    PitchDetector detector;
    NoteController notes;
    HarmonizerParameters parameters;
    std::vector<PitchShifter> voices = std::vector<PitchShifter>(DspConfig::maxVoices);
    juce::AudioBuffer<float> input;
    juce::AudioBuffer<float> output;
    juce::AudioBuffer<float> outputCombined;

    float frequency;
};
