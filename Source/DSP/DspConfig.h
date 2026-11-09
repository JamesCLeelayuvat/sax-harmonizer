#pragma once

//==============================================================================
/*
    Shared DSP constants. Change the FFT size here and everything follows.
*/
namespace DspConfig
{
    inline constexpr int fftOrder = 11;
    inline constexpr int fftSize = 1 << fftOrder;
    inline constexpr int hopSize = fftSize / 4;

    // Size of the buffer for one of JUCE's real-only FFTs: interleaved re, im, with bin k
    // at [2 * k] and [2 * k + 1]. Only bins 0..fftSize / 2 are meaningful, but the FFT
    // needs room for fftSize * 2 floats.
    inline constexpr int spectrumSize = fftSize * 2;

    constexpr int maxVoices = 5;
}
