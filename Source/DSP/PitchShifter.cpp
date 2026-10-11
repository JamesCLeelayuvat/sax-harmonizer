#include "PitchShifter.h"

void PitchShifter::prepare(double sampleRate, int numChannels)
{
    currNumChannels = numChannels;
    currSampleRate = sampleRate;
    writeIndex = 0;
    std::fill(prevFftPhaseData.begin(), prevFftPhaseData.end(), 0);
    std::fill(fftData.begin(), fftData.end(), 0);
    std::fill(fftDataOut.begin(), fftDataOut.end(), 0);
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
        writeIndex++;

        if (writeIndex >= hopSize)
        {
            writeIndex = 0;
            std::fill(fftDataOut.begin(), fftDataOut.end(), 0); // zero the output

            // trigger an fft process
            inputBuffer.readAll(fftData);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

            // loop for each freq bin
            for (std::size_t k = 0; k < fftSize; k++)
            {
                float real = fftData[k * i];
                float imag = fftData[k * i + 1];
                float mag = std::sqrt(real * real + imag * imag);
                float phase = atan2(imag, real);

                // phase calculation
                float realAdvance = phase - prevFftPhaseData[i];
                float expectedAdvance = 2 * std::numbers::pi * k * hopSize / fftSize;
                float deviation = realAdvance - expectedAdvance;

                // wrap around
                deviation = deviation - 2 * std::numbers::pi * round(deviation / 2 * std::numbers::pi);
                float realBin = k + deviation / (2 * std::numbers::pi * hopSize / fftSize);

                // bin scaling portion
                float outputPhase = prevFftPhaseData[i] + 2 * std::numbers::pi * k * hopSize / fftSize;
                int newBin = round(TuningUtils::getEqualTemperedRatio() * realBin);

                // check if bin out of range and loading output
                if (newBin <= fftSize / 2)
                {
                    fftDataOut[newBin * 2] += mag * cos(outputPhase);
                    fftDataOut[newBin * 2 + 1] += mag * sin(outputPhase);
                }
                forwardFFT.performRealOnlyInverseTransform(fftDataOut.data());

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
                }
            }
        }
        dataOut[i] = outputAccumulator[writeIndex];
    }
}
