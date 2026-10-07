#pragma once

//==============================================================================
/*
    Shared DSP constants. Change the FFT size here and everything follows.
*/
namespace DspConfig
{
    inline constexpr int fftOrder = 10;
    inline constexpr int fftSize = 1 << fftOrder;
}
