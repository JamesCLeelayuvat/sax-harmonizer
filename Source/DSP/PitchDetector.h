#pragma once

#include <JuceHeader.h>
#include "DspConfig.h"
#include "../Utils/RingBuffer.h"

//==============================================================================
/*
    Estimates the fundamental frequency of the incoming sax signal. Keeps the
    last fftSize input samples and analyses them every hopSize samples, with its
    own FFT. The frame is NOT windowed (YIN compares the signal with a shifted
    copy of itself, and a window would make even a perfectly periodic note
    differ from itself).
*/
class PitchDetector
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void pushSamples(const juce::AudioBuffer<float> &input, int numSamples);
    float getFrequency() const;

private:
    void processFrame();
    void detectPitch();
    double currSampleRate;
    int currNumChannels;
    static constexpr auto fftSize = DspConfig::fftSize; // [2]
    static constexpr auto hopSize = DspConfig::hopSize;
    RingBuffer inputBuffer{fftSize};
    std::vector<float> frame = std::vector<float>(fftSize); // the raw (unwindowed) last fftSize samples, oldest first
    int hopCounter = 0;
    std::vector<float> fftData = std::vector<float>(fftSize * 4 + 2);
    std::vector<float> squareSum = std::vector<float>(fftSize);
    float frequency = 0;
    float cumulative = 0;
    const float threshold = 0.1;
    int dip = 0;
    juce::dsp::FFT fft{DspConfig::fftOrder + 1};

};
