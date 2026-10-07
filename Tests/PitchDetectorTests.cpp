#include <JuceHeader.h>
#include "../Source/DSP/PitchDetector.h"

//==============================================================================
/*
    Feeds pure sine waves at known frequencies into PitchDetector and checks
    getFrequency() against them. Exits with a non-zero code if any case fails.
*/

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testFftSize = DspConfig::fftSize;

    // A naive FFT peak-picker can only be as accurate as one bin
    constexpr double testTolerance = testSampleRate / testFftSize;

    bool runSineTest (double frequency)
    {
        PitchDetector detector;
        detector.prepare (testSampleRate, 1);

        // Several FFT frames' worth, so the detector has definitely run
        juce::AudioBuffer<float> buffer (1, testFftSize * 4);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / testSampleRate));

        detector.pushSamples (buffer, buffer.getNumSamples());

        const auto detected = detector.getFrequency();
        const auto error = std::abs (detected - frequency);
        const bool passed = error <= testTolerance;

        std::printf ("  %-4s  expected %7.1f Hz   got %7.1f Hz   (error %6.1f Hz)\n",
                     passed ? "PASS" : "FAIL", frequency, (double) detected, error);
        return passed;
    }
}

int main()
{
    std::printf ("PitchDetector sine tests  (sample rate %.0f Hz, tolerance +/- %.1f Hz)\n",
                 testSampleRate, testTolerance);

    // Roughly the alto sax range: Db3, A3, A4, A5
    const double frequencies[] = { 138.6, 220.0, 440.0, 880.0 };

    int failures = 0;
    for (auto f : frequencies)
        if (! runSineTest (f))
            ++failures;

    std::printf ("\n%d of %d passed\n", (int) std::size (frequencies) - failures, (int) std::size (frequencies));
    return failures == 0 ? 0 : 1;
}
