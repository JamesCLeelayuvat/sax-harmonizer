#pragma once

//==============================================================================
/*
    Decides what interval (in semitones) each harmony voice should play.
*/
class IntervalController
{
public:
    float getIntervalFor (int voiceIndex, float inputFrequency) const;
};
