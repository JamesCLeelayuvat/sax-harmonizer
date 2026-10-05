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
    int maxIndex = 0;
    float maxAmp = 0.0f;
    for (int i = 1; i < fftSize / 2; i++)
    {
        if (fftData[i] > maxAmp)
        {
            maxIndex = i;
            maxAmp = fftData[i];
        }
    }

    frequency = maxIndex * (float)currSampleRate / fftSize;
}
