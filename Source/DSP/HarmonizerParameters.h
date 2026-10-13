#pragma once

#include <atomic>

//==============================================================================
/*
    Values shared between the GUI and HarmonizerEngine. Every field is atomic, so
    either thread can read or write without locks. Gains are linear (1.0 = 0 dB).
*/
struct HarmonizerParameters
{
    // GUI -> engine
    std::atomic<float> dryGain { 1.0f };     // level of the original (input) signal in the mix
    std::atomic<float> harmonyGain { 1.0f }; // level of the harmony voices in the mix

    // Engine -> GUI
    std::atomic<float> detectedFrequency { 0.0f }; // Hz of the input note; 0 = nothing detected
};
