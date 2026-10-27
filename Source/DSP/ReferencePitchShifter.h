#pragma once

#include <JuceHeader.h>
#include <vector>
#include "DspConfig.h"

//==============================================================================
/*
    Reference phase-vocoder pitch shifter, written to compare against PitchShifter.

    Same algorithm: STFT (Hann window, hop = fftSize / 4), peak picking, each
    peak's region of bins is moved so the peak lands on its shifted frequency
    (Laroche-Dolson), and every bin in a region is phase-locked to its peak
    (identity phase locking).

    Differences from PitchShifter:
      - Peaks are tracked from frame to frame. A peak continues the running phase
        of the nearest peak in the previous frame, so drifting to another bin
        (vibrato, a changing shift) doesn't make its phase jump.
      - When shifted regions overlap, the louder bin wins, so a bin's magnitude
        and phase always come from the same region.
      - Peaks more than 60 dB below the loudest one are ignored.

    Same interface as PitchShifter, so HarmonizerEngine can use either.
    Latency: fftSize samples.
*/
class ReferencePitchShifter
{
public:
    void prepare(double sampleRate, int numChannels);
    void reset();

    void setSemitones(float newSemitones);
    void process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples);

private:
    void processFrame();
    void findPeaks();
    void shiftPeaks(float ratio);

    static constexpr int fftOrder = DspConfig::fftOrder;
    static constexpr int fftSize = DspConfig::fftSize;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int maxTrackingDistance = 3; // bins a peak may move between frames and still count as the same peak

    struct Peak
    {
        int bin = 0;        // analysis bin of the peak
        int startBin = 0;   // first bin of its region
        int endBin = 0;     // one past the last bin of its region
        float phase = 0.0f; // synthesis phase at the shifted peak
    };

    juce::dsp::FFT fft{fftOrder};

    std::vector<float> window;
    std::vector<float> inputFifo;   // last fftSize input samples (circular)
    std::vector<float> frame;       // FFT workspace, fftSize * 2
    std::vector<float> outputAccum; // overlap-add buffer

    std::vector<float> analysisMag;
    std::vector<float> analysisPhase;
    std::vector<float> prevAnalysisPhase;
    std::vector<float> trueBin; // measured frequency of each bin, in bins

    std::vector<float> synthMag;
    std::vector<float> synthPhase;

    std::vector<Peak> peaks;
    std::vector<Peak> prevPeaks;

    int fifoPos = 0;
    int hopCounter = 0;
    int outputPos = 0;
    float semitones = 0.0f;
};
