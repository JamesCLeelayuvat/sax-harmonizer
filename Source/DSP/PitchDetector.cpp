#include "PitchDetector.h"
void PitchDetector::prepare(double sampleRate, int numChannels)
{
    currSampleRate = sampleRate;
    currNumChannels = numChannels;
}

void PitchDetector::reset()
{
    // TODO
}

void PitchDetector::pushSamples(const juce::AudioBuffer<float> &input, int numSamples)
{

    auto inputBuffer = input.getReadPointer(0);
    for (int i = 0; i < numSamples; i++)
    {
        pushNextSampleIntoFifo(inputBuffer[i]);
    }
    // TODO
}

float PitchDetector::getFrequency() const
{
    // TODO
    return frequency;
}

void PitchDetector::pushNextSampleIntoFifo(float sample)
{
    // if the fifo contains enough data, set a flag to say
    // that the next line should now be rendered..
    if (fifoIndex == fftSize) // [8]
    {
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        std::copy(fifo.begin(), fifo.end(), fftData.begin());
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
        detectPitch();
        fifoIndex = 0;
    }
    fifo[(size_t)fifoIndex++] = sample; // [9]
}

void PitchDetector::detectPitch()
{
    // Harmonic product spectrum: the loudest bin is often the 2nd harmonic on a sax,
    // so score each candidate f0 by the product of its first numHarmonics harmonics
    // (a sum of logs here). Only the true fundamental lines up with all of them.
    constexpr int numHarmonics = 4;            // 3 still loses to the octave when the fundamental is ~16 dB down
    constexpr float minFrequency = 60.0f;      // just below a bari's low A (55 Hz is out of reach of the bin size anyway)
    constexpr float maxFrequency = 1600.0f;    // above the soprano's altissimo
    constexpr float candidateThreshold = 0.1f; // a candidate must be within 20 dB of the loudest bin

    const float binWidth = (float)currSampleRate / fftSize;
    const int minBin = std::max(2, (int)std::ceil(minFrequency / binWidth));
    const int maxBin = std::min((int)(maxFrequency / binWidth), fftSize / 2 / numHarmonics - 1);

    float maxAmp = 0.0f;
    int maxIndex = 0;
    for (int i = 1; i < fftSize / 2; i++)
    {
        if (fftData[i] > fftData[maxIndex])
        {
            maxIndex = i;
            maxAmp = fftData[i];
        }
    }
    frequency = (maxIndex) * (float)currSampleRate / fftSize;

    if (maxIndex != 0 && maxIndex != fftSize / 2 - 1)
    {
        float lower = std::log(fftData[maxIndex - 1] + 1e-12f);
        float mid = std::log(fftData[maxIndex]);
        float higher = std::log(fftData[maxIndex + 1] + 1e-12f);

        float offset = (lower - higher) / (2 * (lower - 2 * mid + higher));
        frequency = (maxIndex + offset) * (float)currSampleRate / fftSize;
    }
}

