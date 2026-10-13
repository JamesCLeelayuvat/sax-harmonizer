#pragma once

#include <JuceHeader.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include "DspConfig.h"
#include "../Utils/RingBuffer.h"
#include "../Utils/TuningUtils.h"

//==============================================================================
/*
    Wraps SignalsmithStretch for real-time pitch shifting.
*/
class PitchShifter
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setSemitones(float semitones);
    void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples);

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::size_t writeIndex;
    RingBuffer inputBuffer{fftSize};
    std::vector<float> outputAccumulator = std::vector<float>(fftSize);
    double currSampleRate;
    int currNumChannels;
    static constexpr auto fftOrder = DspConfig::fftOrder; // [1]
    static constexpr auto fftSize = DspConfig::fftSize;   // [2]
    static constexpr int overlapFactor = 4;
    static constexpr int hopSize = fftSize / overlapFactor;
    juce::dsp::FFT forwardFFT{fftOrder}; // [3]

    std::vector<float> fftData = std::vector<float>(fftSize * 2);
    std::vector<float> prevFftPhaseData = std::vector<float>(fftSize);
    std::vector<float> fftDataOut = std::vector<float>(fftSize * 2);
    std::vector<float> hannWindow;
    std::vector<float> outputPhase = std::vector<float>(fftSize / 2 + 1);
};
static void multiplyWindow(std::vector<float> &fftData, std::vector<float> window)
{
    for (std::size_t i = 0; i < window.size(); i++)
    {
        fftData[i] = fftData[i] * window[i];
    }
}
static std::vector<float> generateHannWindow(std::size_t N)
{
    std::vector<float> window = std::vector<float>(N);
    for (std::size_t i = 0; i < N; i++)
    {
        window[i] = 0.5 - 0.5 * std::cos((2 * juce::MathConstants<float>::pi * i) / (N));
    }

    return window;
}

;
