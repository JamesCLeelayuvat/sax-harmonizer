# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A real-time saxophone harmonizer (COMP490 Senior Seminar). It's a JUCE standalone GUI app (`projectType="guiapp"`). It takes live audio input, detects the pitch, and mixes in pitch-shifted harmony voices. Most of the DSP is still stubbed out with `// TODO`.

## Working with the user

The user is learning C++/DSP through this project. Explain concepts and suggest changes, but only edit code when explicitly asked, and keep edits minimal and scoped to the request.

## Keeping this file current

Update this CLAUDE.md yourself (no need to ask first) whenever something meaningful about the project changes or is decided in conversation. For example:
- project structure changes (classes added, removed, renamed, or moved; changes to the signal chain)
- libraries or JUCE modules are added, removed, or upgraded
- the build, run, or test setup changes (new tools, toolsets, test framework, benchmark workflow)
- design decisions are made (pitch-detection algorithm, number of voices, interval/key logic, latency targets)
- the stubs described below get implemented, so the description no longer matches the code

Keep the edits short and replace outdated text rather than appending. When you update the file, tell the user what you changed in one line. Don't record routine code edits, debugging steps, or anything that's obvious from reading the code.

## Build & run (Windows)

- **Projucer is the source of truth.** `sax-harmonizer.jucer` generates `Builds/VisualStudio2026/` and `JuceLibraryCode/`. When you add or remove source files or change header paths/modules, update the `.jucer` (and re-save in Projucer), not just the `.vcxproj`. Otherwise the change is lost on the next export.
- JUCE modules are expected at `C:/Program Files/JUCE/modules`. `juce_dsp` is enabled (used for `juce::dsp::FFT`).
- Build (Debug), the same as the VS Code default build task:
  ```
  "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe" Builds/VisualStudio2026/sax-harmonizer.sln -p:Configuration=Debug -p:Platform=x64 -p:PlatformToolset=v143 -m -nologo -v:minimal
  ```
  `-p:PlatformToolset=v143` is required. Projucer targets VS 2026 (v145), but only the VS 2022 Build Tools (v143) are installed.
- Output: `Builds/VisualStudio2026/x64/Debug/App/NewProject.exe` (the exe still has the old project name). The VS Code launch config "Debug sax-harmonizer" builds and then runs it.
- The top-level `VisualStudio2026/` folder (`NewProject.sln`) is a stale leftover export. Use `Builds/VisualStudio2026/`.
- **CMake (alternative build):** `CMakeLists.txt` + `CMakePresets.json` build the app and both test apps in parallel to Projucer (pulls JUCE in via `add_subdirectory`, `-DJUCE_DIR=` overrides the path). Source lists there must be kept in sync with the `.jucer` by hand. `cmake --preset vs2022` → `cmake --build --preset debug` → `ctest --preset debug`. Output: `build/SaxHarmonizer_artefacts/Debug/sax-harmonizer.exe`.
- MSVC, compiled as **C++17** (the `.jucer` doesn't set `cppLanguageStandard`, so the exported `.vcxproj` uses `stdcpp17`). C++20 features such as `std::numbers` are unavailable unless that's changed in Projucer.

## Tests

`Tests/PitchDetectorTests.jucer` is a separate Projucer console app (modules: core, audio_basics, audio_formats, dsp) that compiles `Tests/PitchDetectorTests.cpp` with `Source/DSP/PitchDetector.cpp`. It feeds sine waves in and checks `getFrequency()` to within one FFT bin. There's no test framework, just a plain `main` that returns non-zero on failure. Build and run it with `powershell -ExecutionPolicy Bypass -File Tests/run_tests.ps1`. If you add source files to it, update that `.jucer` and run `Projucer.exe --resave`. `Tests/PitchShifterTests/` is a second console app in its own folder (so the two projects don't share `JuceLibraryCode/`). It compiles PitchShifter, PitchDetector and TuningUtils, runs sines through the shifter in 512-sample blocks, and checks the output pitch (measured with PitchDetector) and RMS level. Run it with `powershell -ExecutionPolicy Bypass -File Tests/PitchShifterTests/run_tests.ps1`. `Tests/BenchmarkRunner.cpp` is still empty, and `Tests/test_signals/` has no files yet.

## Architecture

Signal flow (audio thread):

```
MainComponent (juce::AudioAppComponent, 1 in (mono mic) / 2 out)
  │   input meter -> engine -> master gain -> output meter -> copy ch0 to all outputs
  └─ HarmonizerEngine            all DSP, no GUI; prepare/process/release mirror the audio callbacks
       ├─ PitchDetector          estimates input f0; keeps recent samples in a RingBuffer
       ├─ NoteController         lock-free bitmask of GUI-selected absolute *written* notes (A3..G6, max 4 voices)
       │                         + transposition (Concert / Bb = -2 / Eb = -9); voice i plays the i-th selected
       │                         note, ascending; getSemitoneShiftFor(voice, inputHz) = concert target - input,
       │                         so harmonies depend on PitchDetector accuracy
       ├─ HarmonizerParameters   atomics: dryGain/harmonyGain (GUI -> engine), detectedFrequency (engine -> GUI)
       └─ PitchShifter(s)        one per harmony voice (std::vector, DspConfig::maxVoices; HarmonyVoice wrapper was removed);
                                 hand-written phase vocoder (STFT, hop = fftSize/4) with Laroche–Dolson
                                 peak-region shifting + identity phase locking; a signalsmith stretch member remains but is unused
```

- The engine and voices are the user's to implement. `MainComponent` does only routing and monitoring around the engine; keep the DSP out of it.
- Channel contract: `HarmonizerEngine::process()` gets the mic in channel 0 and must leave its mono mix (dry + harmonies) in channel 0, working in place with `startSample`/`numSamples`. The GUI reaches the engine only via `getNoteController()` and `getParameters()`.
- GUI (`Source/GUI/`): `NoteKeyboard` is a toggle piano of absolute written notes (labels show written name, plus concert name when transposed); a combo box above it picks the transposition. `LevelMeter` is a peak meter. The gain sliders are in dB, and the "Audio Settings..." button opens a JUCE `AudioDeviceSelectorComponent` whose device state is saved with `ApplicationProperties`.
- `Source/DSP/DspConfig.h` holds shared DSP constants (`DspConfig::fftOrder` / `fftSize`). `PitchDetector`, `PitchShifter` and the tests all read the FFT size from there, so don't hard-code it anywhere else.
- `Source/Utils/TuningUtils` has the 12-TET ratio (`getEqualTemperedRatio(float semitones)` computes 2^(n/12), fractional shifts allowed, clamped to -12..+12) and `frequencyToMidiNote(hz)`.
- Code in `prepare`/`process` runs on the audio thread. Do allocation (stretch configuration, buffer sizing) in `prepare`, not in `process`.

## Third-party

`ThirdParty/signalsmith-stretch` (v1.3.2) and `ThirdParty/signalsmith-linear` are vendored, header-only, and MIT licensed (no submodules). Their include paths are set in the `.jucer` and in `.vscode/c_cpp_properties.json`. Include the library with `#include <signalsmith-stretch/signalsmith-stretch.h>`. See `ThirdParty/README.md` for how to update them.
