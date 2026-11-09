#pragma once

#include <JuceHeader.h>
#include "DspConfig.h"
#include "../Utils/RingBuffer.h"
#include "../Utils/TuningUtils.h"

//==============================================================================
/*
    Phase-vocoder pitch shifter with its own STFT: every hopSize samples it
    Hann-windows the last fftSize input samples, FFTs them, shifts the spectrum
    (processSpectrum), inverse-FFTs and overlap-adds. Latency: fftSize samples.
*/
class PitchShifter
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setSemitones(float semitones);
    void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples);

private:
    void processFrame();
    // One STFT frame, spectrum in -> spectrum out. Keeps phases between frames, so call once per hop, in order.
    void processSpectrum(const juce::AudioBuffer<float> &inputSpectrum, juce::AudioBuffer<float> &outputSpectrum);

    double currSampleRate;
    int currNumChannels;
    float semitones = 0;
    static constexpr auto fftSize = DspConfig::fftSize;   // [2]
    static constexpr auto hopSize = DspConfig::hopSize;

    juce::dsp::FFT fft{DspConfig::fftOrder};
    RingBuffer inputBuffer{fftSize};          // last fftSize input samples
    std::vector<float> timeFrame;             // those samples, oldest first
    std::vector<float> hannWindow;
    std::vector<float> outputAccumulator;     // overlap-add buffer
    juce::AudioBuffer<float> analysisSpectrum;  // forward FFT of this frame
    juce::AudioBuffer<float> synthesisSpectrum; // the shifted spectrum, before the inverse FFT
    int hopCounter = 0;
    int outputPos = 0;

    struct peakRegion
    {
        int startBin = 0;
        int peakBin = 0;
        int endBin = 0;
    };
    std::vector<float> prevFftPhaseData = std::vector<float>(fftSize);
    std::vector<float> outputPhase = std::vector<float>(fftSize / 2 + 1);
    std::vector<float> outputMag = std::vector<float>(fftSize / 2 + 1);
    std::vector<peakRegion> peaks = std::vector<peakRegion>(fftSize / 2);
    std::vector<float> inputMag = std::vector<float>(fftSize / 2 + 1);
    std::vector<float> inputPhase = std::vector<float>(fftSize / 2 + 1);
    std::vector<float> realBinVector = std::vector<float>(fftSize / 2 + 1);
    std::vector<float> binOwnerMag = std::vector<float>(fftSize / 2 + 1);
};
