#pragma once

#include <JuceHeader.h>
#include "../Utils/RingBuffer.h"
#include "DspConfig.h"

//==============================================================================
/*
    Estimates the fundamental frequency of the incoming sax signal.
*/
class PitchDetector
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void pushSamples(const juce::AudioBuffer<float> &input, int numSamples);
    float getFrequency() const;
    void pushNextSampleIntoFifo(float sample);
    void detectPitch();

private:
    double currSampleRate;
    int currNumChannels;
    static constexpr auto fftOrder = DspConfig::fftOrder; // [1]
    static constexpr auto fftSize = DspConfig::fftSize;   // [2]
    juce::dsp::FFT forwardFFT{fftOrder};           // [3]
    std::array<float, fftSize> fifo;               // [4]
    std::array<float, fftSize * 2> fftData;        // [5]
    int fifoIndex = 0;                             // [6]
    bool nextFFTBlockReady = false;                // [7]
    float frequency = 0;
};
