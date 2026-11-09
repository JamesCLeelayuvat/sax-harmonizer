#include "PitchShifter.h"

namespace
{
    std::vector<float> generateHannWindow(std::size_t N)
    {
        std::vector<float> window = std::vector<float>(N);
        for (std::size_t i = 0; i < N; i++)
        {
            window[i] = (float)(0.5 - 0.5 * std::cos((2 * juce::MathConstants<float>::pi * i) / (N)));
        }

        return window;
    }

    // A Hann analysis window times a Hann synthesis window at 4x overlap sums to 1.5
    constexpr float overlapAddGain = 1.5f;
}

void PitchShifter::prepare(double sampleRate, int numChannels)
{
    currNumChannels = numChannels;
    currSampleRate = sampleRate;
    std::fill(prevFftPhaseData.begin(), prevFftPhaseData.end(), 0);
    std::fill(outputPhase.begin(), outputPhase.end(), 0);
    std::fill(outputMag.begin(), outputMag.end(), 0);
    std::fill(binOwnerMag.begin(), binOwnerMag.end(), 0);

    inputBuffer = RingBuffer(fftSize);
    timeFrame.assign(fftSize, 0.0f);
    hannWindow = generateHannWindow(fftSize);
    outputAccumulator.assign(fftSize, 0.0f);
    analysisSpectrum.setSize(1, DspConfig::spectrumSize);
    synthesisSpectrum.setSize(1, DspConfig::spectrumSize);
    hopCounter = 0;
    outputPos = 0;
}

void PitchShifter::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples)
{
    auto in = input.getReadPointer(0);
    auto out = output.getWritePointer(0);
    for (int n = 0; n < numSamples; n++)
    {
        inputBuffer.push(in[n]);
        out[n] = outputAccumulator[(std::size_t)outputPos++];

        if (++hopCounter == hopSize)
        {
            hopCounter = 0;
            outputPos = 0;
            processFrame();
        }
    }
}

void PitchShifter::processFrame()
{
    // Analysis: the last fftSize samples, windowed, then the forward FFT
    inputBuffer.readAll(timeFrame);
    auto inFrame = analysisSpectrum.getWritePointer(0);
    for (int i = 0; i < fftSize; i++)
    {
        inFrame[i] = timeFrame[(std::size_t)i] * hannWindow[(std::size_t)i];
    }
    std::fill(inFrame + fftSize, inFrame + DspConfig::spectrumSize, 0.0f);
    fft.performRealOnlyForwardTransform(inFrame, true);

    processSpectrum(analysisSpectrum, synthesisSpectrum);

    // Synthesis (the inverse FFT only reads bins 0..fftSize/2 and mirrors the rest)
    auto outFrame = synthesisSpectrum.getWritePointer(0);
    fft.performRealOnlyInverseTransform(outFrame);

    // Overlap-add: drop the hop that was just played, then add the new windowed frame
    for (std::size_t i = 0; i < fftSize; i++)
    {
        outputAccumulator[i] = (i + hopSize < fftSize) ? outputAccumulator[i + hopSize] : 0.0f;
        outputAccumulator[i] += outFrame[i] * hannWindow[i] / overlapAddGain;
    }
}

void PitchShifter::reset()
{
    // TODO
}

void PitchShifter::setSemitones(float semitones)
{
    this->semitones = semitones;
}

void PitchShifter::processSpectrum(const juce::AudioBuffer<float> &inputSpectrum, juce::AudioBuffer<float> &outputSpectrum)
{
    outputSpectrum.clear(); // zero the output
    auto fftData = inputSpectrum.getReadPointer(0);
    auto fftDataOut = outputSpectrum.getWritePointer(0);

    std::fill(outputMag.begin(), outputMag.end(), 0);
    std::fill(binOwnerMag.begin(), binOwnerMag.end(), 0);

    // find peaks

    // LOOP1: Fill mag, phase, and realbin vectors
    for (std::size_t k = 0; k <= fftSize / 2; k++)
    {
        float real = fftData[2 * k];
        float imag = fftData[2 * k + 1];
        float mag = std::sqrt(real * real + imag * imag);
        float phase = atan2(imag, real);
        inputMag[k] = mag; // transferring to array
        inputPhase[k] = phase;
        // phase calculation
        float realAdvance = phase - prevFftPhaseData[k];
        float expectedAdvance = 2 * juce::MathConstants<float>::pi * k * hopSize / fftSize;
        float deviation = realAdvance - expectedAdvance;
        prevFftPhaseData[k] = phase;

        // wrap around
        deviation = deviation - 2 * juce::MathConstants<float>::pi * round(deviation / (2 * juce::MathConstants<float>::pi));
        float realBin = k + deviation / (2 * juce::MathConstants<float>::pi * hopSize / fftSize);
        realBinVector[k] = realBin;
    }

    peaks.clear();

    // LOOP2: Find peaks
    for (int i = 1; i < fftSize / 2; i++)
    {
        if (inputMag[i] > inputMag[i - 1] && inputMag[i] >= inputMag[i + 1])
        {
            peaks.push_back({0, i, 0});
        }
    }

    // LOOP3: Insert valleys between peaks
    if (!peaks.empty())
    {
        peaks[0].startBin = 0;
        peaks[peaks.size() - 1].endBin = fftSize / 2 + 1;

        float minVal = 0;
        int minBin = 0;
        for (int i = 0; i < peaks.size() - 1; i++)
        {
            int start = peaks[i].peakBin + 1;
            int end = peaks[i + 1].peakBin - 1;
            minVal = inputMag[start];
            minBin = start;
            for (int j = start; j <= end; j++)
            {
                if (inputMag[j] < minVal)
                {
                    minVal = inputMag[j];
                    minBin = j;
                }
            }
            peaks[i].endBin = minBin + 1;
            peaks[i + 1].startBin = minBin + 1;
        }
    }
    // Sort all peak regions
    std::sort(peaks.begin(), peaks.end(), [this](const peakRegion &a, const peakRegion &b)
              { return inputMag[a.peakBin] > inputMag[b.peakBin]; });

    // LOOP4 Shifting all peak regions
    for (const auto &region : peaks)
    {

        float newPeak = TuningUtils::getEqualTemperedRatio(semitones) * realBinVector[region.peakBin];
        int newPeakBin = (int)std::round(newPeak);
        int offset = newPeakBin - region.peakBin;

        if (newPeakBin > fftSize / 2 || newPeakBin < 0) // Check if shifted bins are in bounds
            continue;

        if (inputMag[region.peakBin] > binOwnerMag[newPeakBin])
        {
            outputMag[newPeakBin] = inputMag[region.peakBin];
            outputPhase[newPeakBin] += 2 * juce::MathConstants<float>::pi * newPeak * hopSize / fftSize;
            outputPhase[newPeakBin] = std::remainder(outputPhase[newPeakBin], 2 * juce::MathConstants<float>::pi);
            binOwnerMag[newPeakBin] = inputMag[region.peakBin];

            for (int i = region.startBin; i < region.endBin; i++)
            {
                if (i + offset <= fftSize / 2 && i + offset >= 0 && inputMag[region.peakBin] > binOwnerMag[i + offset]) // Check if shifted bins are in bounds
                {
                    outputMag[i + offset] = inputMag[i];
                    outputPhase[i + offset] = outputPhase[newPeakBin] + inputPhase[i] - inputPhase[region.peakBin];
                    binOwnerMag[i + offset] = inputMag[region.peakBin];
                }
            }
        }
    }

    // LOOP5:
    for (int i = 0; i <= fftSize / 2; i++) // second pass to update phase
    {

        fftDataOut[i * 2] = outputMag[i] * cos(outputPhase[i]);
        fftDataOut[i * 2 + 1] = outputMag[i] * sin(outputPhase[i]);
    }
}
