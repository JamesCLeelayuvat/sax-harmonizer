#include "ReferencePitchShifter.h"
#include "../Utils/TuningUtils.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Wraps a phase into -pi..pi
    float wrapPhase(float phase)
    {
        return phase - twoPi * std::round(phase / twoPi);
    }
}

void ReferencePitchShifter::prepare(double /*sampleRate*/, int /*numChannels*/)
{
    window.resize(fftSize);
    for (int n = 0; n < fftSize; ++n)
        window[(size_t)n] = 0.5f - 0.5f * std::cos(twoPi * (float)n / (float)fftSize);

    inputFifo.resize(fftSize);
    frame.resize(fftSize * 2);
    outputAccum.resize(fftSize);

    analysisMag.resize(numBins);
    analysisPhase.resize(numBins);
    prevAnalysisPhase.resize(numBins);
    trueBin.resize(numBins);
    synthMag.resize(numBins);
    synthPhase.resize(numBins);

    // Reserve so push_back / copying in process() never allocates
    peaks.reserve(numBins);
    prevPeaks.reserve(numBins);

    reset();
}

void ReferencePitchShifter::reset()
{
    std::fill(inputFifo.begin(), inputFifo.end(), 0.0f);
    std::fill(outputAccum.begin(), outputAccum.end(), 0.0f);
    std::fill(prevAnalysisPhase.begin(), prevAnalysisPhase.end(), 0.0f);
    std::fill(synthPhase.begin(), synthPhase.end(), 0.0f);
    peaks.clear();
    prevPeaks.clear();
    fifoPos = 0;
    hopCounter = 0;
    outputPos = 0;
}

void ReferencePitchShifter::setSemitones(float newSemitones)
{
    semitones = newSemitones;
}

void ReferencePitchShifter::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples)
{
    auto *in = input.getReadPointer(0);
    auto *out = output.getWritePointer(0);

    for (int n = 0; n < numSamples; ++n)
    {
        inputFifo[(size_t)fifoPos] = in[n];
        fifoPos = (fifoPos + 1) % fftSize;

        out[n] = outputAccum[(size_t)outputPos++];

        if (++hopCounter == hopSize)
        {
            hopCounter = 0;
            outputPos = 0;
            processFrame();
        }
    }
}

void ReferencePitchShifter::processFrame()
{
    // Phase advance of bin k over one hop is k * binAdvance
    const float binAdvance = twoPi * (float)hopSize / (float)fftSize;

    // Analysis: the last fftSize samples, oldest first, windowed
    for (int n = 0; n < fftSize; ++n)
        frame[(size_t)n] = inputFifo[(size_t)((fifoPos + n) % fftSize)] * window[(size_t)n];

    std::fill(frame.begin() + fftSize, frame.end(), 0.0f);
    fft.performRealOnlyForwardTransform(frame.data(), true);

    for (int k = 0; k < numBins; ++k)
    {
        const float re = frame[(size_t)(2 * k)];
        const float im = frame[(size_t)(2 * k + 1)];
        analysisMag[(size_t)k] = std::sqrt(re * re + im * im);
        analysisPhase[(size_t)k] = std::atan2(im, re);

        // The phase advance beyond what bin k's centre frequency predicts gives the true frequency
        const float deviation = wrapPhase(analysisPhase[(size_t)k] - prevAnalysisPhase[(size_t)k] - (float)k * binAdvance);
        trueBin[(size_t)k] = (float)k + deviation / binAdvance;
        prevAnalysisPhase[(size_t)k] = analysisPhase[(size_t)k];
    }

    findPeaks();
    shiftPeaks(TuningUtils::getEqualTemperedRatio(semitones));

    // Resynthesis (the inverse FFT only reads bins 0..fftSize/2 and mirrors the rest)
    for (int k = 0; k < numBins; ++k)
    {
        frame[(size_t)(2 * k)] = synthMag[(size_t)k] * std::cos(synthPhase[(size_t)k]);
        frame[(size_t)(2 * k + 1)] = synthMag[(size_t)k] * std::sin(synthPhase[(size_t)k]);
    }

    fft.performRealOnlyInverseTransform(frame.data());

    // Overlap-add: drop the hop that was just played, then add the new frame.
    // A Hann analysis window times a Hann synthesis window at 4x overlap sums to 1.5.
    std::copy(outputAccum.begin() + hopSize, outputAccum.end(), outputAccum.begin());
    std::fill(outputAccum.end() - hopSize, outputAccum.end(), 0.0f);

    for (int n = 0; n < fftSize; ++n)
        outputAccum[(size_t)n] += frame[(size_t)n] * window[(size_t)n] / 1.5f;
}

void ReferencePitchShifter::findPeaks()
{
    peaks.clear();

    const float maxMag = *std::max_element(analysisMag.begin(), analysisMag.end());
    if (maxMag <= 0.0f)
        return; // silence

    const float threshold = maxMag * 0.001f; // -60 dB

    // A peak is louder than the two bins on each side
    for (int k = 2; k < numBins - 2; ++k)
    {
        const float mag = analysisMag[(size_t)k];

        if (mag > threshold
            && mag > analysisMag[(size_t)(k - 1)] && mag >= analysisMag[(size_t)(k + 1)]
            && mag > analysisMag[(size_t)(k - 2)] && mag >= analysisMag[(size_t)(k + 2)])
        {
            Peak peak;
            peak.bin = k;
            peaks.push_back(peak);
        }
    }

    if (peaks.empty())
        return;

    // Regions: split at the quietest bin between neighbouring peaks.
    // The valley goes to the lower region; endBin is exclusive.
    peaks.front().startBin = 0;

    for (size_t p = 0; p + 1 < peaks.size(); ++p)
    {
        int valley = peaks[p].bin + 1;

        for (int k = valley + 1; k < peaks[p + 1].bin; ++k)
            if (analysisMag[(size_t)k] < analysisMag[(size_t)valley])
                valley = k;

        peaks[p].endBin = valley + 1;
        peaks[p + 1].startBin = valley + 1;
    }

    peaks.back().endBin = numBins;
}

void ReferencePitchShifter::shiftPeaks(float ratio)
{
    const float binAdvance = twoPi * (float)hopSize / (float)fftSize;

    std::fill(synthMag.begin(), synthMag.end(), 0.0f);

    // peaks and prevPeaks are both sorted by bin, so the nearest previous peak only moves forward
    size_t prev = 0;

    for (auto &peak : peaks)
    {
        const float shiftedBin = ratio * trueBin[(size_t)peak.bin];

        while (prev + 1 < prevPeaks.size()
               && std::abs(prevPeaks[prev + 1].bin - peak.bin) <= std::abs(prevPeaks[prev].bin - peak.bin))
            ++prev;

        const bool tracked = !prevPeaks.empty() && std::abs(prevPeaks[prev].bin - peak.bin) <= maxTrackingDistance;

        // Continue the same partial's phase, advanced at the *shifted* frequency.
        // A new partial starts from its analysis phase.
        peak.phase = tracked ? wrapPhase(prevPeaks[prev].phase + shiftedBin * binAdvance)
                             : analysisPhase[(size_t)peak.bin];

        const int targetBin = (int)std::round(shiftedBin);
        if (targetBin < 0 || targetBin >= numBins)
            continue;

        // Move the whole region by the same number of bins, keeping each bin's
        // phase relative to the peak
        const int offset = targetBin - peak.bin;

        for (int k = peak.startBin; k < peak.endBin; ++k)
        {
            const int dest = k + offset;
            if (dest < 0 || dest >= numBins)
                continue;

            if (analysisMag[(size_t)k] <= synthMag[(size_t)dest])
                continue; // an overlapping region is louder here

            synthMag[(size_t)dest] = analysisMag[(size_t)k];
            synthPhase[(size_t)dest] = peak.phase + analysisPhase[(size_t)k] - analysisPhase[(size_t)peak.bin];
        }
    }

    prevPeaks = peaks; // capacity was reserved in prepare(), so this doesn't allocate
}
