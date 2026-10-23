#include "PitchShifter.h"

void PitchShifter::prepare(double sampleRate, int numChannels)
{
    currNumChannels = numChannels;
    currSampleRate = sampleRate;
    writeIndex = 0;
    std::fill(prevFftPhaseData.begin(), prevFftPhaseData.end(), 0);
    std::fill(fftData.begin(), fftData.end(), 0);
    std::fill(fftDataOut.begin(), fftDataOut.end(), 0);
    std::fill(outputPhase.begin(), outputPhase.end(), 0);
    std::fill(shiftedFreq.begin(), shiftedFreq.end(), 0);
    std::fill(outputMag.begin(), outputMag.end(), 0);
    hannWindow = generateHannWindow(fftSize);
}

void PitchShifter::reset()
{
    // TODO
}

void PitchShifter::setSemitones(float semitones)
{
    this->semitones = semitones;
}

void PitchShifter::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples)
{

    auto data = input.getReadPointer(0);
    auto dataOut = output.getWritePointer(0);
    for (int i = 0; i < numSamples; i++)
    {
        // adding sample by sample to input buffer
        inputBuffer.push(data[i]);

        if (writeIndex >= hopSize)
        {
            std::fill(shiftedFreq.begin(), shiftedFreq.end(), 0);
            std::fill(outputMag.begin(), outputMag.end(), 0);
            writeIndex = 0;
            std::fill(fftDataOut.begin(), fftDataOut.end(), 0); // zero the output

            // trigger an fft process

            std::fill(fftData.begin(), fftData.end(), 0); // zero the rest of it

            inputBuffer.readAll(fftData);
            multiplyWindow(fftData, hannWindow);
            forwardFFT.performRealOnlyForwardTransform(fftData.data());

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

                // int newBin = round(TuningUtils::getEqualTemperedRatio(semitones) * k);
                // if (newBin >= 0 && newBin <= fftSize / 2)
                // {
                //     shiftedFreq[newBin] = TuningUtils::getEqualTemperedRatio(semitones) * realBin;
                //     dataMag[newBin] += mag;
                //     // bin scaling portion
                // }
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

            // LOOP4 Shifting all peak regions
            for (const auto &region : peaks)
            {
                float newPeak = TuningUtils::getEqualTemperedRatio(semitones) * realBinVector[region.peakBin];
                int newPeakBin = (int)std::round(newPeak);
                int offset = newPeakBin - region.peakBin;

                if (newPeakBin > fftSize / 2 || newPeakBin < 0) // Check if shifted bins are in bounds
                    continue;

                outputMag[newPeakBin] = inputMag[region.peakBin];
                outputPhase[newPeakBin] += 2 * juce::MathConstants<float>::pi * newPeak * hopSize / fftSize;
                outputPhase[newPeakBin] = std::remainder(outputPhase[newPeakBin], 2 * juce::MathConstants<float>::pi);
                for (int i = region.startBin; i < region.endBin; i++)
                {
                    if (i + offset <= fftSize / 2 && i + offset >= 0) // Check if shifted bins are in bounds
                    {
                        outputMag[i + offset] += inputMag[i];
                        outputPhase[i + offset] = outputPhase[newPeakBin] + inputPhase[i] - inputPhase[region.peakBin];
                    }
                }
            }

            // LOOP5:
            for (int i = 0; i <= fftSize / 2; i++) // second pass to update phase
            {

                fftDataOut[i * 2] = outputMag[i] * cos(outputPhase[i]);
                fftDataOut[i * 2 + 1] = outputMag[i] * sin(outputPhase[i]);
            }

            forwardFFT.performRealOnlyInverseTransform(fftDataOut.data());
            multiplyWindow(fftDataOut, hannWindow);

            // shifting the accumulator
            for (std::size_t i = 0; i < fftSize; i++)
            {
                if (i + hopSize >= fftSize)
                {
                    outputAccumulator[i] = 0;
                }
                else
                {
                    outputAccumulator[i] = outputAccumulator[i + hopSize];
                }
                outputAccumulator[i] += fftDataOut[i];
            }
        }
        dataOut[i] = outputAccumulator[writeIndex] / 1.5;
        writeIndex++;
    }
}
