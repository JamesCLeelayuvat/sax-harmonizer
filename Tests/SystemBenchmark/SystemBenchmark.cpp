#include <JuceHeader.h>
#include <Saxofony.h>
#include "../../Source/DSP/HarmonizerEngine.h"
#include "../../Source/DSP/PitchDetector.h"
#include "../../Source/DSP/PitchShifter.h"
#include "../../Source/DSP/ReferencePitchShifter.h"
#include "../../Source/Utils/TuningUtils.h"

#include <fstream>
#include <limits>
#include <map>
#include <random>

//==============================================================================
/*
    Whole-application benchmark. Writes one long-format CSV (a `test` column says
    which test a row belongs to, unused columns are blank) that plot_results.py
    turns into a PDF. Input tones come from STK's Saxofony physical model.

      shifter_pitch     every tenor note x all 24 shifts x both shifters: pitch and level error
      detector          every tenor note, several trials: error of each PitchDetector estimate
      latency           50 random trials for 1-4 voices through HarmonizerEngine
      timbre            5 notes x all 24 shifts x both shifters: error of each harmonic peak
      calibration       how close each STK note got to its target (run before the others)

    Usage: SystemBenchmark [--tests=all|latency] [--out=file.csv] [--append]
    It's a benchmark, not a test: it exits 0 whatever the results are, and non-zero
    only if its own measuring tools fail their self-test.
*/

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int toneLength = 60000;       // 1.25 s
    constexpr int settleSamples = 19200;    // 0.4 s, skips the STK attack and the shifter start-up
    constexpr int shifterLatency = DspConfig::fftSize;
    constexpr double inputRmsDb = -18.0;

    constexpr int tenorLowMidi = 44;        // Ab2, tenor written low Bb
    constexpr int tenorHighMidi = 76;       // E5, tenor written high F#
    const int timbreNotes[] = { 44, 52, 60, 68, 76 }; // Ab2 E3 C4 Ab4 E5, evenly across the tenor range

    constexpr int detectorTrials = 8;
    constexpr int latencyTrials = 50;
    constexpr int latencySnippet = 4800;    // 100 ms
    constexpr double latencyOnsetDb = -40.0;

    const char* buildName =
       #if JUCE_DEBUG
        "Debug";
       #else
        "Release";
       #endif

    const double nan = std::numeric_limits<double>::quiet_NaN();

    double midiToHz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }
    double centsBetween (double hz, double referenceHz) { return 1200.0 * std::log2 (hz / referenceHz); }
    double semitonesToRatio (int semitones) { return std::pow (2.0, semitones / 12.0); }
    double toDb (double linear) { return 20.0 * std::log10 (std::max (linear, 1.0e-12)); }

    std::string noteName (int midi)
    {
        static const char* names[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return std::string (names[midi % 12]) + std::to_string (midi / 12 - 1);
    }

    double rms (const float* x, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / n);
    }

    //==============================================================================
    // Measuring tools. Independent of the code under test, and much higher
    // resolution than it (long windows, zero padding).

    struct Spectrum
    {
        double binHz = 0.0;
        std::vector<float> power;   // scaled so a sine of amplitude A peaks at A^2

        double powerAt (double hz) const
        {
            const auto bin = (size_t) std::lround (hz / binHz);
            return bin < power.size() ? power[bin] : 0.0;
        }
    };

    struct Peak
    {
        double hz = 0.0;
        double db = -240.0;
        bool found = false;     // false = no real peak: the loudest bin was at the edge of the search range
    };

    // Hann-windowed spectrum, power-averaged over overlapping windows
    Spectrum analyse (const float* x, int numSamples, int windowSize, int padFactor)
    {
        static std::map<int, std::unique_ptr<juce::dsp::FFT>> ffts;

        const int fftLength = windowSize * padFactor;
        const int order = juce::roundToInt (std::log2 (fftLength));
        auto& fft = ffts[order];
        if (fft == nullptr)
            fft = std::make_unique<juce::dsp::FFT> (order);

        std::vector<float> window ((size_t) windowSize);
        double windowSum = 0.0;
        for (int i = 0; i < windowSize; ++i)
        {
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / windowSize));
            windowSum += window[(size_t) i];
        }

        Spectrum spectrum;
        spectrum.binHz = sampleRate / fftLength;
        spectrum.power.assign ((size_t) fftLength / 2 + 1, 0.0f);

        std::vector<float> data ((size_t) fftLength * 2);
        int numWindows = 0;

        for (int start = 0; start + windowSize <= numSamples; start += windowSize / 2)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            for (int i = 0; i < windowSize; ++i)
                data[(size_t) i] = x[start + i] * window[(size_t) i];

            fft->performFrequencyOnlyForwardTransform (data.data(), true);

            for (size_t k = 0; k < spectrum.power.size(); ++k)
            {
                const double magnitude = data[k] * 2.0 / windowSum;
                spectrum.power[k] += (float) (magnitude * magnitude);
            }
            ++numWindows;
        }

        for (auto& p : spectrum.power)
            p /= (float) juce::jmax (1, numWindows);

        return spectrum;
    }

    // Loudest bin in the range, refined with a parabola through the dB values
    Peak findPeak (const Spectrum& spectrum, double lowHz, double highHz)
    {
        const int last = (int) spectrum.power.size() - 2;
        const int low = juce::jlimit (1, last, (int) std::ceil (lowHz / spectrum.binHz));
        const int high = juce::jlimit (1, last, (int) std::floor (highHz / spectrum.binHz));

        int best = low;
        for (int k = low; k <= high; ++k)
            if (spectrum.power[(size_t) k] > spectrum.power[(size_t) best])
                best = k;

        const auto dbAt = [&] (int k) { return 10.0 * std::log10 (std::max ((double) spectrum.power[(size_t) k], 1.0e-24)); };

        Peak peak;
        peak.hz = best * spectrum.binHz;
        peak.db = dbAt (best);

        if (best == low || best == high)
            return peak;

        const double a = dbAt (best - 1), b = dbAt (best), c = dbAt (best + 1);
        const double denominator = a - 2.0 * b + c;
        const double offset = denominator != 0.0 ? 0.5 * (a - c) / denominator : 0.0;

        peak.hz = (best + offset) * spectrum.binHz;
        peak.db = b - 0.25 * (a - c) * offset;
        peak.found = true;
        return peak;
    }

    // Pitch of a harmonic tone that should be near expectedHz. A harmonic comb is slid over
    // +/- 700 cents, then the f0 implied by each strong harmonic is averaged. Staying under an
    // octave means it can't be fooled by sub- or super-harmonics, or by a weak fundamental.
    double measurePitch (const float* x, int numSamples, double expectedHz)
    {
        const auto spectrum = analyse (x, numSamples, 32768, 2);

        const auto harmonicsOf = [] (double f0) { return juce::jmin (24, (int) (16000.0 / f0)); };

        double bestF0 = expectedHz, bestScore = -1.0;
        for (int cents = -700; cents <= 700; ++cents)
        {
            const double f0 = expectedHz * std::pow (2.0, cents / 1200.0);
            double score = 0.0;
            for (int h = 1; h <= harmonicsOf (f0); ++h)
                score += std::sqrt (spectrum.powerAt (f0 * h));

            if (score > bestScore)
            {
                bestScore = score;
                bestF0 = f0;
            }
        }

        double strongest = -240.0;
        std::vector<std::pair<int, Peak>> peaks;
        for (int h = 1; h <= harmonicsOf (bestF0); ++h)
        {
            const double centre = bestF0 * h;
            const double halfWidth = juce::jmax (bestF0 * 0.1, 4.0 * spectrum.binHz);
            const auto peak = findPeak (spectrum, centre - halfWidth, centre + halfWidth);
            if (peak.found)
            {
                peaks.push_back ({ h, peak });
                strongest = juce::jmax (strongest, peak.db);
            }
        }

        double weightedF0 = 0.0, totalWeight = 0.0;
        for (const auto& [h, peak] : peaks)
        {
            if (peak.db < strongest - 30.0)
                continue;
            const double weight = std::pow (10.0, peak.db / 20.0);
            weightedF0 += weight * peak.hz / h;
            totalWeight += weight;
        }

        return totalWeight > 0.0 ? weightedF0 / totalWeight : bestF0;
    }

    // Fraction of the spectrum's power that sits on harmonics of f0: near 1 for a clean tone
    double harmonicFraction (const float* x, int numSamples, double f0)
    {
        const auto spectrum = analyse (x, numSamples, 8192, 2);
        double onHarmonics = 0.0, total = 0.0;
        for (size_t k = 1; k < spectrum.power.size(); ++k)
        {
            const double hz = (double) k * spectrum.binHz;
            const double distance = std::abs (hz - f0 * std::round (hz / f0));
            total += spectrum.power[k];
            if (distance < 15.0)
                onHarmonics += spectrum.power[k];
        }
        return total > 0.0 ? onHarmonics / total : 0.0;
    }

    //==============================================================================
    // STK tones

    std::vector<float> renderSax (double setFrequency, unsigned int seed)
    {
        stk::Saxofony sax (20.0);
        std::srand (seed);              // Saxofony's breath noise uses rand(); its constructor seeds from the clock
        sax.controlChange (1, 0.0);     // vibrato off, so there is one right answer for the pitch
        sax.noteOn (setFrequency, 0.8);

        std::vector<float> tone ((size_t) toneLength);
        for (auto& sample : tone)
            sample = (float) sax.tick();

        const auto scale = (float) (std::pow (10.0, inputRmsDb / 20.0) / juce::jmax (1.0e-9, rms (tone.data() + settleSamples, toneLength - settleSamples)));
        for (auto& sample : tone)
            sample *= scale;

        return tone;
    }

    double measureTone (const std::vector<float>& tone, double expectedHz)
    {
        return measurePitch (tone.data() + settleSamples, toneLength - settleSamples, expectedHz);
    }

    // The waveguide doesn't sound exactly at the frequency it's given (the error grows up the
    // range), so nudge the frequency until the note it actually plays matches the target.
    struct CalibratedNote
    {
        double setFrequency = 0.0;
        double measuredHz = 0.0;
        double cleanFraction = 0.0;
        std::vector<float> tone;
    };

    CalibratedNote calibrate (int midi)
    {
        const double target = midiToHz (midi);
        const auto seed = 1000u + (unsigned int) midi;

        CalibratedNote note;
        double setFrequency = target;
        double bestError = std::numeric_limits<double>::max();

        for (int iteration = 0; iteration < 8 && bestError >= 0.05; ++iteration)
        {
            auto tone = renderSax (setFrequency, seed);
            const double measured = measureTone (tone, target);
            const double error = std::abs (centsBetween (measured, target));

            if (error < bestError)
            {
                bestError = error;
                note.setFrequency = setFrequency;
                note.measuredHz = measured;
                note.tone = std::move (tone);
            }

            setFrequency *= target / measured;
        }

        note.cleanFraction = harmonicFraction (note.tone.data() + settleSamples, toneLength - settleSamples, note.measuredHz);
        return note;
    }

    std::map<int, CalibratedNote> calibratedNotes;

    //==============================================================================
    // CSV

    struct Row
    {
        std::string test, shifter, note, shifts;
        int midi = -1, shift = 0, voices = -1, trial = -1, harmonic = -1, peakFound = -1;
        bool hasShift = false;
        double targetHz = nan, inputHz = nan, expectedHz = nan, outputHz = nan, pitchErrorCents = nan,
               inputLevelDb = nan, outputLevelDb = nan, levelErrorDb = nan, firstSampleMs = nan, onsetMs = nan,
               computeMs = nan, maxBlockComputeMs = nan, setHz = nan, cleanFraction = nan;
    };

    class CsvWriter
    {
    public:
        CsvWriter (const std::string& path, bool append)
            : file (path, append ? std::ios::app : std::ios::trunc)
        {
            if (! append)
                file << "test,build,shifter,note,midi,shift_semitones,voices,shifts,trial,harmonic,peak_found,"
                        "target_hz,input_hz,expected_hz,output_hz,pitch_error_cents,input_level_db,output_level_db,level_error_db,"
                        "first_sample_ms,onset_ms,compute_ms,max_block_compute_ms,stk_set_hz,harmonic_fraction\n";
            file.precision (10);
        }

        bool isOpen() const { return file.is_open(); }

        void write (const Row& r)
        {
            file << r.test << ',' << buildName << ',' << r.shifter << ',' << r.note << ',';
            integer (r.midi); integer (r.hasShift ? r.shift : -99, -99); integer (r.voices);
            file << r.shifts << ',';
            integer (r.trial); integer (r.harmonic); integer (r.peakFound);
            for (double v : { r.targetHz, r.inputHz, r.expectedHz, r.outputHz, r.pitchErrorCents, r.inputLevelDb, r.outputLevelDb,
                              r.levelErrorDb, r.firstSampleMs, r.onsetMs, r.computeMs, r.maxBlockComputeMs, r.setHz })
                real (v);
            if (! std::isnan (r.cleanFraction))
                file << r.cleanFraction;
            file << '\n';
        }

    private:
        void integer (int value, int blank = -1)
        {
            if (value != blank)
                file << value;
            file << ',';
        }

        void real (double value)
        {
            if (! std::isnan (value))
                file << value;
            file << ',';
        }

        std::ofstream file;
    };

    //==============================================================================
    // Running a shifter over a whole tone, in blocks like the audio callback

    template <typename Shifter>
    std::vector<float> runShifter (const std::vector<float>& input, int semitones)
    {
        auto shifter = std::make_unique<Shifter>();
        shifter->prepare (sampleRate, 1);
        shifter->setSemitones ((float) semitones);

        juce::AudioBuffer<float> in (1, blockSize), out (1, blockSize);
        std::vector<float> output (input.size());

        for (size_t pos = 0; pos < input.size(); pos += blockSize)
        {
            const int n = juce::jmin (blockSize, (int) (input.size() - pos));
            in.copyFrom (0, 0, input.data() + pos, n);
            shifter->process (in, out, n);
            std::copy_n (out.getReadPointer (0), n, output.data() + pos);
        }

        return output;
    }

    template <typename Fn>
    void forEachShift (Fn&& fn)
    {
        for (int shift = -12; shift <= 12; ++shift)
            if (shift != 0)
                fn (shift);
    }

    //==============================================================================
    // Test 1: pitch and level error of each shifter

    template <typename Shifter>
    void testShifterPitch (const char* shifterName, CsvWriter& csv)
    {
        const int length = toneLength - settleSamples - shifterLatency;

        for (int midi = tenorLowMidi; midi <= tenorHighMidi; ++midi)
        {
            const auto& note = calibratedNotes.at (midi);
            const auto* in = note.tone.data() + settleSamples;
            const double inputHz = measurePitch (in, length, note.measuredHz);
            const double inputLevel = toDb (rms (in, length));

            forEachShift ([&] (int shift)
            {
                const auto output = runShifter<Shifter> (note.tone, shift);
                const auto* out = output.data() + settleSamples + shifterLatency;
                const double expected = inputHz * semitonesToRatio (shift);
                const double outputHz = measurePitch (out, length, expected);
                const double outputLevel = toDb (rms (out, length));

                Row r;
                r.test = "shifter_pitch"; r.shifter = shifterName; r.note = noteName (midi); r.midi = midi;
                r.shift = shift; r.hasShift = true;
                r.targetHz = midiToHz (midi); r.inputHz = inputHz; r.expectedHz = expected; r.outputHz = outputHz;
                r.pitchErrorCents = centsBetween (outputHz, expected);
                r.inputLevelDb = inputLevel; r.outputLevelDb = outputLevel; r.levelErrorDb = outputLevel - inputLevel;
                csv.write (r);
            });

            std::printf ("  shifter_pitch %s %s\n", shifterName, noteName (midi).c_str());
        }
    }

    //==============================================================================
    // Test 2: every estimate PitchDetector makes on a steady tone

    void testDetector (CsvWriter& csv)
    {
        std::mt19937 random (12345);

        for (int midi = tenorLowMidi; midi <= tenorHighMidi; ++midi)
        {
            const auto& note = calibratedNotes.at (midi);

            for (int trial = 0; trial < detectorTrials; ++trial)
            {
                // A new breath-noise seed and a different alignment with the detector's blocks each trial
                const auto tone = renderSax (note.setFrequency, 2000u + (unsigned int) (midi * 100 + trial));
                const double inputHz = measureTone (tone, midiToHz (midi));
                const auto startOffset = (size_t) (random() % DspConfig::fftSize);

                auto detector = std::make_unique<PitchDetector>();
                detector->prepare (sampleRate, 1);

                juce::AudioBuffer<float> in (1, blockSize);
                float previous = 0.0f;

                for (size_t pos = startOffset; pos + blockSize <= tone.size(); pos += blockSize)
                {
                    in.copyFrom (0, 0, tone.data() + pos, blockSize);
                    detector->pushSamples (in, blockSize);

                    const float detected = detector->getFrequency();
                    if (detected == previous)
                        continue;
                    previous = detected;

                    // Only count estimates made entirely from the settled part of the tone
                    if ((int) pos < settleSamples + DspConfig::fftSize)
                        continue;

                    Row r;
                    r.test = "detector"; r.note = noteName (midi); r.midi = midi; r.trial = trial;
                    r.targetHz = midiToHz (midi); r.inputHz = inputHz; r.outputHz = detected;
                    r.pitchErrorCents = detected > 0.0f ? centsBetween (detected, inputHz) : nan;
                    csv.write (r);
                }
            }

            std::printf ("  detector %s\n", noteName (midi).c_str());
        }
    }

    //==============================================================================
    // Test 3: through the whole engine, input snippet to first output sample

    void testLatency (CsvWriter& csv)
    {
        const int tailSamples = DspConfig::fftSize * 3;

        for (int voices = 1; voices <= NoteController::maxVoices; ++voices)
        {
            std::mt19937 random (777u + (unsigned int) voices);   // same trials in every build

            for (int trial = 0; trial < latencyTrials; ++trial)
            {
                const int midi = tenorLowMidi + (int) (random() % (tenorHighMidi - tenorLowMidi + 1));
                const auto& note = calibratedNotes.at (midi);

                auto engine = std::make_unique<HarmonizerEngine>();
                engine->prepare (sampleRate, blockSize);

                std::vector<int> shifts;
                while ((int) shifts.size() < voices)
                {
                    const int shift = (int) (random() % 25) - 12;
                    if (shift != 0 && std::find (shifts.begin(), shifts.end(), shift) == shifts.end())
                        shifts.push_back (shift);
                }
                std::sort (shifts.begin(), shifts.end());

                std::string shiftText;
                for (int shift : shifts)
                {
                    engine->getNoteController().setNoteEnabled (NoteController::unisonNote + shift, true);
                    shiftText += (shiftText.empty() ? "" : ";") + std::to_string (shift);
                }

                // Snippet cut from the steady part of the tone, starting abruptly so "first input"
                // is exact, and landing at a random position relative to the blocks and hops
                const int snippetStart = settleSamples + (int) (random() % (toneLength - settleSamples - latencySnippet));
                const int inputStart = blockSize + (int) (random() % DspConfig::fftSize);

                std::vector<float> signal ((size_t) (inputStart + latencySnippet + tailSamples), 0.0f);
                std::copy_n (note.tone.data() + snippetStart, latencySnippet, signal.data() + inputStart);

                std::vector<double> blockMs;
                juce::AudioBuffer<float> buffer (1, blockSize);

                for (size_t pos = 0; pos + blockSize <= signal.size(); pos += blockSize)
                {
                    buffer.copyFrom (0, 0, signal.data() + pos, blockSize);

                    const auto start = juce::Time::getHighResolutionTicks();
                    engine->process (buffer, 0, blockSize);
                    const auto end = juce::Time::getHighResolutionTicks();

                    blockMs.push_back (1000.0 * juce::Time::highResolutionTicksToSeconds (end - start));
                    std::copy_n (buffer.getReadPointer (0), blockSize, signal.data() + pos);
                }

                // Two definitions of "first sample out". The phase vocoder's first frame after the onset
                // spreads a tiny amount of energy over the whole frame, so the first non-zero sample is far
                // ahead of (and far quieter than) anything audible. Both are recorded.
                float peak = 0.0f;
                for (float sample : signal)
                    peak = juce::jmax (peak, std::abs (sample));

                const auto threshold = peak * (float) std::pow (10.0, latencyOnsetDb / 20.0);
                int firstNonZero = -1, firstOnset = -1;
                for (int i = 0; i < (int) signal.size(); ++i)
                {
                    const float level = std::abs (signal[(size_t) i]);
                    if (firstNonZero < 0 && level > 0.0f)
                        firstNonZero = i;
                    if (firstOnset < 0 && level >= threshold && peak > 0.0f)
                    {
                        firstOnset = i;
                        break;
                    }
                }

                Row r;
                r.test = "latency"; r.note = noteName (midi); r.midi = midi; r.voices = voices; r.trial = trial; r.shifts = shiftText;
                r.shifter = "HarmonizerEngine";
                r.maxBlockComputeMs = *std::max_element (blockMs.begin(), blockMs.end());
                if (firstOnset >= 0)
                {
                    // Sample-accurate delay + the time spent computing the block that delivered the sample
                    const auto delayMs = [&] (int index)
                    {
                        return 1000.0 * (index - inputStart) / sampleRate + blockMs[(size_t) (index / blockSize)];
                    };
                    r.firstSampleMs = delayMs (firstNonZero);
                    r.onsetMs = delayMs (firstOnset);
                    r.computeMs = blockMs[(size_t) (firstOnset / blockSize)];
                }
                csv.write (r);
            }

            std::printf ("  latency %s %d voice(s)\n", buildName, voices);
        }
    }

    //==============================================================================
    // Test 4: each harmonic peak, input against output

    template <typename Shifter>
    void testTimbre (const char* shifterName, CsvWriter& csv)
    {
        constexpr int windowSize = 8192;    // resolves harmonics an octave below the lowest tenor note
        constexpr int padFactor = 4;
        constexpr double maxHarmonicHz = 20000.0;
        constexpr double dynamicRangeDb = 60.0;
        constexpr double prominenceDb = 10.0;   // above the breath noise between harmonics

        const int length = toneLength - settleSamples - shifterLatency;

        for (int midi : timbreNotes)
        {
            const auto& note = calibratedNotes.at (midi);
            const auto* in = note.tone.data() + settleSamples;
            const auto inputSpectrum = analyse (in, length, windowSize, padFactor);
            const double f0 = measurePitch (in, length, note.measuredHz);

            std::vector<std::pair<int, Peak>> inputPeaks;
            double strongest = -240.0;
            for (int h = 1; h * f0 < sampleRate / 2.0; ++h)
            {
                const auto peak = findPeak (inputSpectrum, f0 * h - f0 / 4.0, f0 * h + f0 / 4.0);
                if (! peak.found)
                    continue;

                std::vector<float> band;
                for (double hz = f0 * h - f0 / 2.0; hz < f0 * h + f0 / 2.0; hz += inputSpectrum.binHz)
                    band.push_back ((float) inputSpectrum.powerAt (hz));
                std::nth_element (band.begin(), band.begin() + (long) band.size() / 2, band.end());

                if (peak.db < 10.0 * std::log10 (std::max ((double) band[band.size() / 2], 1.0e-24)) + prominenceDb)
                    continue;

                inputPeaks.push_back ({ h, peak });
                strongest = juce::jmax (strongest, peak.db);
            }

            forEachShift ([&] (int shift)
            {
                const double ratio = semitonesToRatio (shift);
                const auto output = runShifter<Shifter> (note.tone, shift);
                const auto outputSpectrum = analyse (output.data() + settleSamples + shifterLatency, length, windowSize, padFactor);
                const double halfWidth = juce::jmax (f0 * ratio / 4.0, 4.0 * outputSpectrum.binHz);

                for (const auto& [h, inputPeak] : inputPeaks)
                {
                    const double expected = inputPeak.hz * ratio;
                    if (inputPeak.db < strongest - dynamicRangeDb || expected > maxHarmonicHz)
                        continue;

                    const auto outputPeak = findPeak (outputSpectrum, expected - halfWidth, expected + halfWidth);

                    Row r;
                    r.test = "timbre"; r.shifter = shifterName; r.note = noteName (midi); r.midi = midi;
                    r.shift = shift; r.hasShift = true; r.harmonic = h; r.peakFound = outputPeak.found ? 1 : 0;
                    r.targetHz = midiToHz (midi); r.inputHz = inputPeak.hz; r.expectedHz = expected;
                    r.inputLevelDb = inputPeak.db; r.outputLevelDb = outputPeak.db; r.levelErrorDb = outputPeak.db - inputPeak.db;
                    if (outputPeak.found)
                    {
                        r.outputHz = outputPeak.hz;
                        r.pitchErrorCents = centsBetween (outputPeak.hz, expected);
                    }
                    csv.write (r);
                }
            });

            std::printf ("  timbre %s %s (%d input harmonics)\n", shifterName, noteName (midi).c_str(), (int) inputPeaks.size());
        }
    }

    //==============================================================================
    // Sanity check of the measuring tools on tones with a known answer

    bool selfTest()
    {
        const double f0 = 233.3;
        std::vector<float> tone ((size_t) toneLength, 0.0f);
        for (int h = 1; h <= 10; ++h)
            for (int i = 0; i < toneLength; ++i)
                tone[(size_t) i] += (float) (0.1 / h * std::sin (juce::MathConstants<double>::twoPi * f0 * h * i / sampleRate));

        const double pitchError = std::abs (centsBetween (measurePitch (tone.data(), toneLength, f0 * 1.05), f0));

        const auto spectrum = analyse (tone.data(), toneLength, 8192, 4);
        const auto peak = findPeak (spectrum, f0 * 3.0 - f0 / 4.0, f0 * 3.0 + f0 / 4.0);
        const double peakError = std::abs (centsBetween (peak.hz, f0 * 3.0));
        const double levelError = std::abs (peak.db - toDb (0.1 / 3.0));

        std::printf ("Self-test: pitch %.4f cents, peak %.4f cents, level %.4f dB\n", pitchError, peakError, levelError);
        return pitchError < 0.05 && peakError < 0.2 && levelError < 0.05;
    }
}

int main (int argc, char* argv[])
{
    std::string tests = "all", path = "benchmark_results.csv";
    bool append = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg.rfind ("--tests=", 0) == 0)
            tests = arg.substr (8);
        else if (arg.rfind ("--out=", 0) == 0)
            path = arg.substr (6);
        else if (arg == "--append")
            append = true;
    }

    std::printf ("%s build, FFT size %d, block size %d, %.0f Hz\n", buildName, DspConfig::fftSize, blockSize, sampleRate);

    if (! selfTest())
    {
        std::printf ("Measuring tools failed their self-test\n");
        return 1;
    }

    CsvWriter csv (path, append);
    if (! csv.isOpen())
    {
        std::printf ("Can't open %s\n", path.c_str());
        return 1;
    }

    stk::Stk::setSampleRate (sampleRate);

    std::printf ("Calibrating STK tones\n");
    for (int midi = tenorLowMidi; midi <= tenorHighMidi; ++midi)
    {
        auto note = calibrate (midi);

        Row r;
        r.test = "calibration"; r.note = noteName (midi); r.midi = midi; r.targetHz = midiToHz (midi);
        r.setHz = note.setFrequency; r.inputHz = note.measuredHz; r.cleanFraction = note.cleanFraction;
        r.pitchErrorCents = centsBetween (note.measuredHz, r.targetHz);
        csv.write (r);

        std::printf ("  %-4s target %8.2f Hz, STK told %8.2f (%+6.1f cents), plays %8.2f (%+.3f cents), %.1f%% of power on harmonics\n",
                     r.note.c_str(), r.targetHz, r.setHz, centsBetween (r.setHz, r.targetHz), note.measuredHz, r.pitchErrorCents, 100.0 * note.cleanFraction);

        calibratedNotes.emplace (midi, std::move (note));
    }

    if (tests == "all" || tests == "latency")
        testLatency (csv);

    if (tests == "all")
    {
        testDetector (csv);
        testShifterPitch<PitchShifter> ("PitchShifter", csv);
        testShifterPitch<ReferencePitchShifter> ("ReferencePitchShifter", csv);
        testTimbre<PitchShifter> ("PitchShifter", csv);
        testTimbre<ReferencePitchShifter> ("ReferencePitchShifter", csv);
    }

    std::printf ("Wrote %s\n", path.c_str());
    return 0;
}
