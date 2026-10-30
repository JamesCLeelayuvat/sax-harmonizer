#include <JuceHeader.h>
#include "../../Source/DSP/PitchShifter.h"
#include "../../Source/DSP/PitchDetector.h"

//==============================================================================
/*
    Feeds sine waves through PitchShifter and measures the output with
    PitchDetector (pitch) and RMS (level). Exits non-zero if any case fails.
*/

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testFftSize = DspConfig::fftSize;
    constexpr int blockSize = 512;                 // like a real audio callback
    constexpr int totalSamples = testFftSize * 8;
    constexpr int warmupSamples = testFftSize * 2;  // skip latency / start-up transient

    // PitchDetector is a naive FFT peak-picker, so one bin is the best we can ask for
    constexpr double pitchTolerance = testSampleRate / testFftSize;

    // Output RMS should be within this factor of the input RMS (e.g. 0.8x - 1.25x)
    constexpr double levelTolerance = 1.25;

    int failures = 0;
    int tests = 0;

    void report (bool passed, const char* fmt, double a, double b)
    {
        ++tests;
        if (! passed) ++failures;
        std::printf ("  %-4s  ", passed ? "PASS" : "FAIL");
        std::printf (fmt, a, b);
        std::printf ("\n");
    }

    // Runs the whole signal through the shifter in blockSize chunks
    juce::AudioBuffer<float> runShifter (const juce::AudioBuffer<float>& input, float semitones)
    {
        PitchShifter shifter;
        shifter.prepare (testSampleRate, 1);
        shifter.setSemitones (semitones);

        juce::AudioBuffer<float> output (1, input.getNumSamples());
        output.clear();

        for (int start = 0; start < input.getNumSamples(); start += blockSize)
        {
            const int n = std::min (blockSize, input.getNumSamples() - start);
            juce::AudioBuffer<float> in (1, n), out (1, n);
            in.copyFrom (0, 0, input, 0, start, n);
            out.clear();
            shifter.process (in, out, n);
            output.copyFrom (0, start, out, 0, 0, n);
        }
        return output;
    }

    juce::AudioBuffer<float> makeSine (double frequency, float amplitude)
    {
        juce::AudioBuffer<float> buffer (1, totalSamples);
        for (int i = 0; i < totalSamples; ++i)
            buffer.setSample (0, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / testSampleRate));
        return buffer;
    }

    bool hasNaN (const juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (0, i)))
                return true;
        return false;
    }

    void testShift (double inputFreq, float semitones)
    {
        const auto input = makeSine (inputFreq, 0.5f);
        const auto output = runShifter (input, semitones);
        const double expected = inputFreq * std::pow (2.0, semitones / 12.0);

        std::printf ("\n%.1f Hz shifted by %+.0f semitones (expect %.1f Hz)\n", inputFreq, semitones, expected);

        if (hasNaN (output))
        {
            report (false, "output contains NaN/inf%.0s%.0s", 0, 0);
            return;
        }

        const int steadyLength = totalSamples - warmupSamples;

        PitchDetector detector;
        detector.prepare (testSampleRate, 1);
        juce::AudioBuffer<float> steady (1, steadyLength);
        steady.copyFrom (0, 0, output, 0, warmupSamples, steadyLength);
        detector.pushSamples (steady, steadyLength);

        const double detected = detector.getFrequency();
        report (std::abs (detected - expected) <= pitchTolerance,
                "pitch: expected %7.1f Hz   got %7.1f Hz", expected, detected);

        const double inRms = input.getRMSLevel (0, warmupSamples, steadyLength);
        const double outRms = output.getRMSLevel (0, warmupSamples, steadyLength);
        const double ratio = outRms / inRms;
        report (ratio <= levelTolerance && ratio >= 1.0 / levelTolerance,
                "level: input RMS %.3f   output RMS %.3f", inRms, outRms);
    }

    // Rough alto-sax stand-in: 12 harmonics, rolling off with a bump around 1-2 kHz
    // (the sax's main formant region), fixed "random" phases so harmonics don't line up
    juce::AudioBuffer<float> makeSaxTone (double f0, float peakAmplitude)
    {
        constexpr int numHarmonics = 12;
        const double phases[numHarmonics] = { 0.0, 1.3, 2.9, 0.7, 4.1, 2.2, 5.5, 3.3, 1.9, 0.4, 4.8, 2.6 };

        juce::AudioBuffer<float> buffer (1, totalSamples);
        buffer.clear();
        for (int h = 1; h <= numHarmonics; ++h)
        {
            const double freq = f0 * h;
            const double formant = 1.0 + 1.5 * std::exp (-std::pow ((freq - 1500.0) / 700.0, 2.0));
            const double amp = formant / std::pow ((double) h, 0.9);
            for (int i = 0; i < totalSamples; ++i)
                buffer.addSample (0, i, (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * i / testSampleRate + phases[h - 1])));
        }
        buffer.applyGain (peakAmplitude / buffer.getMagnitude (0, 0, totalSamples));
        return buffer;
    }

    // Fraction of the signal's energy that sits within +/- halfWidthHz of a multiple of f0.
    // A clean shift keeps this near 1; smeared or mis-assigned peak regions lower it.
    double harmonicEnergyRatio (const juce::AudioBuffer<float>& buffer, int start, double f0, double halfWidthHz)
    {
        constexpr int order = 13;
        constexpr int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data (size * 2, 0.0f);
        for (int i = 0; i < size; ++i)
            data[i] = buffer.getSample (0, start + i) * (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / size));
        fft.performFrequencyOnlyForwardTransform (data.data());

        const double binHz = testSampleRate / size;
        double total = 0.0, harmonic = 0.0;
        for (int k = 1; k <= size / 2; ++k)
        {
            const double power = (double) data[k] * data[k];
            const double freq = k * binHz;
            const double nearest = std::round (freq / f0) * f0;
            total += power;
            if (nearest > 0.0 && std::abs (freq - nearest) <= halfWidthHz)
                harmonic += power;
        }
        return total > 0.0 ? harmonic / total : 0.0;
    }

    void testSaxShift (double inputFreq, float semitones)
    {
        const auto input = makeSaxTone (inputFreq, 0.5f);
        const auto output = runShifter (input, semitones);
        const double expected = inputFreq * std::pow (2.0, semitones / 12.0);

        std::printf ("\nSax tone %.1f Hz shifted by %+.0f semitones (expect %.1f Hz)\n", inputFreq, semitones, expected);

        if (hasNaN (output))
        {
            report (false, "output contains NaN/inf%.0s%.0s", 0, 0);
            return;
        }

        const int steadyLength = totalSamples - warmupSamples;

        PitchDetector detector;
        detector.prepare (testSampleRate, 1);
        juce::AudioBuffer<float> steady (1, steadyLength);
        steady.copyFrom (0, 0, output, 0, warmupSamples, steadyLength);
        detector.pushSamples (steady, steadyLength);

        const double detected = detector.getFrequency();
        report (std::abs (detected - expected) <= pitchTolerance,
                "pitch: expected %7.1f Hz   got %7.1f Hz", expected, detected);

        const double inRms = input.getRMSLevel (0, warmupSamples, steadyLength);
        const double outRms = output.getRMSLevel (0, warmupSamples, steadyLength);
        const double ratio = outRms / inRms;
        report (ratio <= levelTolerance && ratio >= 1.0 / levelTolerance,
                "level: input RMS %.3f   output RMS %.3f", inRms, outRms);

        // half a bin of the shifter's FFT either side of each expected harmonic
        const double halfWidth = 0.5 * testSampleRate / testFftSize;
        const double inPurity = harmonicEnergyRatio (input, warmupSamples, inputFreq, halfWidth);
        const double outPurity = harmonicEnergyRatio (output, warmupSamples, expected, halfWidth);
        report (outPurity >= 0.9,
                "harmonic energy: input %.3f   output %.3f (need >= 0.9)", inPurity, outPurity);
    }

    void testSilence()
    {
        std::printf ("\nSilence in\n");
        juce::AudioBuffer<float> input (1, totalSamples);
        input.clear();
        const auto output = runShifter (input, 7.0f);

        const double peak = hasNaN (output) ? -1.0 : output.getMagnitude (0, 0, totalSamples);
        report (peak >= 0.0 && peak < 1.0e-6, "silence out: peak %g%.0s (NaN shows as -1)", peak, 0);
    }
}

int main()
{
    std::printf ("PitchShifter tests  (sample rate %.0f Hz, block %d, pitch tolerance +/- %.1f Hz)\n",
                 testSampleRate, blockSize, pitchTolerance);

    testSilence();
    testShift (440.0, 0.0f);   // unison: checks windowing + overlap-add gain alone
    testShift (440.0, 1.0f);
    testShift (440.0, 7.0f);
    testShift (440.0, 12.0f);
    testShift (440.0, -12.0f);
    testSaxShift (311.1, 0.0f);    // baseline for the harmonic tone
    testSaxShift (311.1, -12.0f);  // alto sax Eb4 (concert): dense harmonics, regions overlap after the shift

    std::printf ("\n%d of %d passed\n", tests - failures, tests);
    return failures == 0 ? 0 : 1;
}
