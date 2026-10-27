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
    constexpr int numHarmonics = 4; // 3 still loses to the octave when the fundamental is ~16 dB down
    constexpr float minFrequency = 60.0f;   // just below a bari's low A (55 Hz is out of reach of the bin size anyway)
    constexpr float maxFrequency = 1600.0f; // above the soprano's altissimo
    constexpr float candidateThreshold = 0.1f; // a candidate must be within 20 dB of the loudest bin

    const float binWidth = (float)currSampleRate / fftSize;
    const int minBin = std::max(2, (int)std::ceil(minFrequency / binWidth));
    const int maxBin = std::min((int)(maxFrequency / binWidth), fftSize / 2 / numHarmonics - 1);

    float maxAmp = 0.0f;
    for (int i = 1; i < fftSize / 2; i++)
    {
        maxAmp = std::max(maxAmp, fftData[i]);
    }

    int maxIndex = 0;
    float bestScore = -std::numeric_limits<float>::infinity();
    for (int i = minBin; i <= maxBin; i++)
    {
        // Without this gate a pure sine's leakage makes half its frequency win:
        // the fundamental has to be a real peak, not a gap between harmonics
        if (fftData[i] < candidateThreshold * maxAmp || fftData[i] < fftData[i - 1] || fftData[i] < fftData[i + 1])
        {
            continue;
        }

        float score = 0.0f;
        for (int h = 1; h <= numHarmonics; h++)
        {
            // f0 can be up to half a bin off i, so harmonic h can be up to h/2 bins off h*i
            const int spread = h / 2;
            float harmonicAmp = 0.0f;
            for (int j = h * i - spread; j <= h * i + spread; j++)
            {
                harmonicAmp = std::max(harmonicAmp, fftData[j]);
            }
            score += std::log(harmonicAmp + 1e-12f);
        }

        if (score > bestScore)
        {
            bestScore = score;
            maxIndex = i;
        }
    }

    if (maxIndex == 0)
    {
        frequency = 0.0f;
        return;
    }

    float lower = std::log(fftData[maxIndex - 1] + 1e-12f);
    float mid = std::log(fftData[maxIndex]);
    float higher = std::log(fftData[maxIndex + 1] + 1e-12f);

    float offset = (lower - higher) / (2 * (lower - 2 * mid + higher));

    frequency = (maxIndex + offset) * (float)currSampleRate / fftSize;
}
