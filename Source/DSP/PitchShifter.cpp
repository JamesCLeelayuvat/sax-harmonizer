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
    hannWindow = generateHannWindow(fftSize);
}

void PitchShifter::reset()
{
    // TODO
}

void PitchShifter::setSemitones(float semitones)
{
    // TODO
}

void PitchShifter::process(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, int numSamples)
{

    auto data = input.getReadPointer(0);
    auto dataOut = output.getWritePointer(0);
    for (int i = 0; i < input.getNumSamples(); i++)
    {
        // adding sample by sample to input buffer
        inputBuffer.push(data[i]);

        if (writeIndex >= hopSize)
        {
            writeIndex = 0;
            std::fill(fftDataOut.begin(), fftDataOut.end(), 0); // zero the output

            // trigger an fft process

            std::fill(fftData.begin(), fftData.end(), 0); // zero the rest of it

            inputBuffer.readAll(fftData);
            multiplyWindow(fftData, hannWindow);
            forwardFFT.performRealOnlyForwardTransform(fftData.data());

            // loop for each freq bin
            for (std::size_t k = 0; k <= fftSize / 2; k++)
            {
                float real = fftData[2 * k];
                float imag = fftData[2 * k + 1];
                float mag = std::sqrt(real * real + imag * imag);
                float phase = atan2(imag, real);

                // phase calculation
                float realAdvance = phase - prevFftPhaseData[k];
                float expectedAdvance = 2 * juce::MathConstants<float>::pi * k * hopSize / fftSize;
                float deviation = realAdvance - expectedAdvance;
                prevFftPhaseData[k] = phase;

                // wrap around
                deviation = deviation - 2 * juce::MathConstants<float>::pi * round(deviation / (2 * juce::MathConstants<float>::pi));
                float realBin = k + deviation / (2 * juce::MathConstants<float>::pi * hopSize / fftSize);
                int newBin = round(TuningUtils::getEqualTemperedRatio(1) * realBin);

                // bin scaling portion

                // check if bin out of range and loading output
                if (newBin >= 0 && newBin <= fftSize / 2)
                {
                    outputPhase[newBin] += 2 * juce::MathConstants<float>::pi * realBin * TuningUtils::getEqualTemperedRatio(1) * hopSize / fftSize;
                    fftDataOut[newBin * 2] += mag * cos(outputPhase[newBin]);
                    fftDataOut[newBin * 2 + 1] += mag * sin(outputPhase[newBin]);
                }
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
