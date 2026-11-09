#include "PitchDetector.h"
void PitchDetector::prepare(double sampleRate, int numChannels)
{
    currSampleRate = sampleRate;
    currNumChannels = numChannels;
}

void PitchDetector::reset()
{
    frequency = 0;
    cumulative = 0;
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(squareSum.begin(),squareSum.end(), 0.0f);
    hopCounter = 0;
}

void PitchDetector::pushSamples(const juce::AudioBuffer<float> &input, int numSamples)
{
    auto in = input.getReadPointer(0);
    for (int n = 0; n < numSamples; n++)
    {
        inputBuffer.push(in[n]);
        if (++hopCounter == hopSize)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

void PitchDetector::processFrame()
{
    // The last fftSize samples, unwindowed, then the forward FFT
    inputBuffer.readAll(frame);
    std::copy(frame.begin(), frame.end(), fftData.begin());
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performRealOnlyForwardTransform(fftData.data(), true);

    float cumulativeSquare = 0;
    for (int i = 0; i < fftSize; i++)
    { // computing the square sum
        cumulativeSquare += frame[i] * frame[i];
        squareSum[i] = cumulativeSquare;
    }

    for (int i = 0; i < fftSize * 2; i++) // Wiener-Khinchin Theorem
    {
        // calculate magnitude
        fftData[i * 2] = fftData[i * 2] * fftData[i * 2] + fftData[i * 2 + 1] * fftData[i * 2 + 1];
        fftData[i * 2 + 1] = 0.0f;
    }

    fft.performRealOnlyInverseTransform(fftData.data());

    detectPitch();
}

float PitchDetector::getFrequency() const
{

    if (frequency > 20000 || frequency < 20)
    {
        return 0;
    }
    // TODO
    return frequency;
}

void PitchDetector::detectPitch()
{
    cumulative = 0;
    dip = 0;

    for (std::size_t i = 1; i < fftSize / 2; i++) // evaluating the cumulative mean
    {

        fftData[i] = squareSum[fftSize - 1 - i] + squareSum[fftSize - 1] - squareSum[i - 1] - 2 * fftData[i];
        cumulative += fftData[i];
        if (cumulative > 0)
        {
            fftData[i] = fftData[i] * i / cumulative;
        }
    }
    fftData[0] = 1;

    for (std::size_t i = 1; i < (fftSize / 2) - 1; i++)
    {
        if (fftData[i] < threshold && fftData[i] < fftData[i + 1])
        {
            dip = i;
            break;
        }
    }
    int minIndex = 0;
    if (dip == 0)
    {
        for (int i = 1; i < (fftSize / 2) - 1; i++)
        {
            if (fftData[i] < fftData[minIndex] && fftData[i] < fftData[i - 1] && fftData[i] < fftData[i + 1])
            {
                minIndex = i;
            }
        }

        dip = minIndex;
    }

    if (dip == 0)
    {
        frequency = 0;
    }
    else
    {
        float estimate = dip + (fftData[dip - 1] - fftData[dip + 1]) / (2 * (fftData[dip - 1] - 2 * fftData[dip] + fftData[dip + 1]));
        frequency = currSampleRate / estimate;
    }
}
