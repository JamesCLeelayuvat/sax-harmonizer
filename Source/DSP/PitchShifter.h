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
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setSemitones(float semitones);
    void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples);

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    double currSampleRate;
    int currNumChannels;
    static constexpr auto fftOrder = 10;           // [1]
    static constexpr auto fftSize = 1 << fftOrder; // [2]
    std::array<float, fftSize> fifo;               // [4]
    std::array<float, fftSize * 2> fftData;        // [5]

    template <std::size_t N>
    static std::array<double, N> generateHannWindow()
    {
        std::array<double, N> window;
        for (std::size_t i = 0; i < N; i++)
        {
            window[i] = 0.5 - 0.5 * std::cos((2 * std::numbers::pi * i) / (N - 1));
        }

        return window;
    }
};
