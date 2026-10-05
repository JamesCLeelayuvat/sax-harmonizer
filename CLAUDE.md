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
- C++20, MSVC.

## Tests

`Tests/PitchDetectorTests.jucer` is a separate Projucer console app (modules: core, audio_basics, audio_formats, dsp) that compiles `Tests/PitchDetectorTests.cpp` with `Source/DSP/PitchDetector.cpp`. It feeds sine waves in and checks `getFrequency()` to within one FFT bin. There's no test framework, just a plain `main` that returns non-zero on failure. Build and run it with `powershell -ExecutionPolicy Bypass -File Tests/run_tests.ps1`. If you add source files to it, update that `.jucer` and run `Projucer.exe --resave`. `Tests/BenchmarkRunner.cpp` is still empty, and `Tests/test_signals/` has no files yet.

## Architecture

Signal flow (audio thread):

```
MainComponent (juce::AudioAppComponent, 2 in / 2 out)
  └─ HarmonizerEngine            all DSP, no GUI; prepare/process/release mirror the audio callbacks
       ├─ PitchDetector          estimates input f0; keeps recent samples in a RingBuffer
       ├─ IntervalController     (voiceIndex, inputFrequency) -> semitone interval
       └─ HarmonyVoice(s)        one per harmony line (not yet added to the engine)
            └─ PitchShifter      wraps signalsmith::stretch::SignalsmithStretch<float>
```

- `MainComponent` only forwards `prepareToPlay` / `getNextAudioBlock` / `releaseResources` to `HarmonizerEngine`. Keep the DSP out of the GUI class.
- `process()` works in place on the device buffer, using `startSample`/`numSamples` from the `AudioSourceChannelInfo`.
- Code in `prepare`/`process` runs on the audio thread. Do allocation (stretch configuration, buffer sizing) in `prepare`, not in `process`.

## Third-party

`ThirdParty/signalsmith-stretch` (v1.3.2) and `ThirdParty/signalsmith-linear` are vendored, header-only, and MIT licensed (no submodules). Their include paths are set in the `.jucer` and in `.vscode/c_cpp_properties.json`. Include the library with `#include <signalsmith-stretch/signalsmith-stretch.h>`. See `ThirdParty/README.md` for how to update them.
