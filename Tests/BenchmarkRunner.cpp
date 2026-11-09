#include <JuceHeader.h>
#include "../Source/DSP/PitchDetector.h"

//==============================================================================
/*
    Pitch-detection benchmark over every note of the saxophone family
    (concert pitch, bari low A = C2 up to soprano high F# = E6).

    Each note is tried as a pure sine, a synthetic sax tone, and the sax tone
    with breath noise, at several start phases. Reports the error in cents per
    note, a summary per signal type and per instrument, and the CPU cost.
    It's a benchmark, not a test: it always exits 0.
*/

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int fftSize = DspConfig::fftSize;
    constexpr int blockSize = 512;
    constexpr int estimatesPerNote = 4;    // different start phases per note

    constexpr double fineCents = 10.0;     // "in tune"
    constexpr double grossCents = 50.0;    // beyond this, the wrong note was detected

    enum class Signal { sine, sax, saxNoisy };
    const char* signalNames[] = { "sine", "sax", "sax + noise" };
    constexpr int numSignals = 3;

    // Written range Bb3..F#6 (bari also has low A), transposed to concert MIDI notes
    struct Instrument { const char* name; char letter; int lowMidi; int highMidi; };
    const Instrument instruments[] = {
        { "soprano",  'S', 56, 88 },   // Ab3 .. E6
        { "alto",     'A', 49, 81 },   // Db3 .. A5
        { "tenor",    'T', 44, 76 },   // Ab2 .. E5
        { "baritone", 'B', 36, 69 },   // C2 (low A) .. A4
    };

    double midiToHz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }

    std::string noteName (int midi)
    {
        static const char* names[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return std::string (names[midi % 12]) + std::to_string (midi / 12 - 1);
    }

    // Sax-ish spectral envelope, fixed in frequency: rises through the low register
    // (so low notes have a weak fundamental, like a real horn) and rolls off above ~1 kHz
    double saxEnvelope (double freq)
    {
        return (freq / (freq + 300.0)) * std::exp (-freq / 2500.0);
    }

    juce::AudioBuffer<float> makeSignal (Signal type, double f0, double startPhase, int numSamples, juce::Random& random)
    {
        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();

        if (type == Signal::sine)
        {
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (0, i, (float) std::sin (juce::MathConstants<double>::twoPi * f0 * i / sampleRate + startPhase));
        }
        else
        {
            for (int h = 1; h * f0 < 8000.0 && h <= 30; ++h)
            {
                const double freq = f0 * h;
                const double amp = saxEnvelope (freq) / std::sqrt ((double) h);
                const double phase = startPhase * h + 0.7 * h * h;   // fixed, scattered phases
                for (int i = 0; i < numSamples; ++i)
                    buffer.addSample (0, i, (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * i / sampleRate + phase)));
            }

            if (type == Signal::saxNoisy)   // white "breath" noise at 30 dB below the tone
            {
                const double noiseRms = buffer.getRMSLevel (0, 0, numSamples) * std::pow (10.0, -30.0 / 20.0);
                const double uniformScale = noiseRms * std::sqrt (3.0);   // uniform [-1, 1] has RMS 1/sqrt(3)
                for (int i = 0; i < numSamples; ++i)
                    buffer.addSample (0, i, (float) (uniformScale * (2.0 * random.nextDouble() - 1.0)));
            }
        }

        buffer.applyGain (0.5f / buffer.getMagnitude (0, 0, numSamples));
        return buffer;
    }

    // One fresh detector. The estimate that counts is the last one, the first
    // frame that's completely filled by the signal.
    double estimate (const juce::AudioBuffer<float>& signal)
    {
        PitchDetector detector;
        detector.prepare (sampleRate, 1);

        detector.pushSamples (signal, signal.getNumSamples());
        return detector.getFrequency();
    }

    double centsError (double detected, double expected)
    {
        return detected > 0.0 ? 1200.0 * std::log2 (detected / expected) : -9999.0;
    }

    bool isOctaveError (double cents)
    {
        return std::abs (std::abs (cents) - 1200.0) < grossCents;
    }

    struct Stats
    {
        int count = 0, fine = 0, close = 0, octave = 0, otherGross = 0;
        double sumAbsCents = 0.0;   // over estimates that found the right note
        double worstCents = 0.0;    // same

        void add (double cents)
        {
            ++count;
            const double a = std::abs (cents);
            if (a <= grossCents)
            {
                ++close;
                if (a <= fineCents) ++fine;
                sumAbsCents += a;
                worstCents = std::max (worstCents, a);
            }
            else if (isOctaveError (cents))  ++octave;
            else                             ++otherGross;
        }

        double pct (int n) const { return count > 0 ? 100.0 * n / count : 0.0; }
    };

    void timeDetector()
    {
        constexpr int numFrames = 2000;
        juce::Random random (1);
        const auto signal = makeSignal (Signal::sax, 233.1, 0.0, numFrames * DspConfig::hopSize + 1, random);

        PitchDetector detector;
        detector.prepare (sampleRate, 1);

        juce::AudioBuffer<float> block (1, blockSize);
        const auto startTicks = juce::Time::getHighResolutionTicks();
        for (int start = 0; start + blockSize <= signal.getNumSamples(); start += blockSize)
        {
            block.copyFrom (0, 0, signal, 0, start, blockSize);
            detector.pushSamples (block, blockSize);
        }
        const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);

        const double audioSeconds = signal.getNumSamples() / sampleRate;
        const double usPerFrame = 1.0e6 * seconds / numFrames;
        const double usPerBlock = 1.0e6 * seconds / (signal.getNumSamples() / blockSize);
        const double budgetUs = 1.0e6 * blockSize / sampleRate;

        std::printf ("\nCPU cost, including the detector's own FFTs (%d analysis frames, %d-sample blocks)\n", numFrames, blockSize);
        std::printf ("  %.1f us per analysis frame, %.2f us per block (budget %.0f us), %.0fx faster than real time\n",
                     usPerFrame, usPerBlock, budgetUs, audioSeconds / seconds);
        std::printf ("  latency: one estimate per %d samples = %.1f ms\n", DspConfig::hopSize, 1000.0 * DspConfig::hopSize / sampleRate);
        (void) detector.getFrequency();
    }
}

int main()
{
#if JUCE_DEBUG
    std::printf ("WARNING: Debug build, so the CPU timings are not meaningful\n\n");
#endif
    std::printf ("Pitch detector benchmark  (sample rate %.0f Hz, FFT %d, bin width %.1f Hz)\n",
                 sampleRate, fftSize, sampleRate / fftSize);
    std::printf ("Per note: worst-case error in cents over %d start phases.  '8va' = octave error, 'XX' = other wrong note\n\n",
                 estimatesPerNote);

    std::printf ("  note    Hz       saxes   ");
    for (auto* name : signalNames)
        std::printf ("%14s", name);
    std::printf ("\n");

    Stats bySignal[numSignals];
    Stats byInstrument[std::size (instruments)][numSignals];
    juce::Random random (42);

    for (int midi = 36; midi <= 88; ++midi)
    {
        const double f0 = midiToHz (midi);

        char saxes[5] = "    ";
        for (int k = 0; k < (int) std::size (instruments); ++k)
            if (midi >= instruments[k].lowMidi && midi <= instruments[k].highMidi)
                saxes[k] = instruments[k].letter;

        std::printf ("  %-5s %7.1f    %s  ", noteName (midi).c_str(), f0, saxes);

        for (int s = 0; s < numSignals; ++s)
        {
            double worst = 0.0;
            for (int e = 0; e < estimatesPerNote; ++e)
            {
                const double startPhase = juce::MathConstants<double>::twoPi * e / estimatesPerNote + 0.3;
                const auto signal = makeSignal ((Signal) s, f0, startPhase, fftSize + 1, random);
                const double cents = centsError (estimate (signal), f0);

                bySignal[s].add (cents);
                for (int k = 0; k < (int) std::size (instruments); ++k)
                    if (saxes[k] != ' ')
                        byInstrument[k][s].add (cents);

                if (std::abs (cents) > std::abs (worst))
                    worst = cents;
            }

            if (std::abs (worst) <= grossCents)   std::printf ("%14.1f", worst);
            else if (isOctaveError (worst))       std::printf ("%14s", worst > 0 ? "8va up" : "8va down");
            else                                  std::printf ("%14s", "XX");
        }
        std::printf ("\n");
    }

    std::printf ("\nSummary by signal (%% of estimates)\n");
    std::printf ("  %-12s  within %2.0fc  within %2.0fc  octave err  other err  mean |c|  worst |c|\n", "", fineCents, grossCents);
    for (int s = 0; s < numSignals; ++s)
    {
        const auto& st = bySignal[s];
        std::printf ("  %-12s  %8.1f%%  %9.1f%%  %9.1f%%  %8.1f%%  %8.1f  %9.1f\n", signalNames[s],
                     st.pct (st.fine), st.pct (st.close), st.pct (st.octave), st.pct (st.otherGross),
                     st.close > 0 ? st.sumAbsCents / st.close : 0.0, st.worstCents);
    }
    std::printf ("  (mean/worst |c| only count estimates that found the right note)\n");

    std::printf ("\nRight note (within %.0f cents) by instrument, %% of estimates\n", grossCents);
    std::printf ("  %-10s ", "");
    for (auto* name : signalNames)
        std::printf ("%14s", name);
    std::printf ("\n");
    for (int k = 0; k < (int) std::size (instruments); ++k)
    {
        std::printf ("  %-10s ", instruments[k].name);
        for (int s = 0; s < numSignals; ++s)
            std::printf ("%13.1f%%", byInstrument[k][s].pct (byInstrument[k][s].close));
        std::printf ("\n");
    }

    timeDetector();
    return 0;
}
