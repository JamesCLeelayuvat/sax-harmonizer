#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    Horizontal peak meter on a dB scale (-60..0 dB). Feed it a linear peak level
    from a GUI timer; it falls back slowly so short peaks stay visible. The red
    end lights up for a moment when the signal clips (>= 0 dB).
*/
class LevelMeter : public juce::Component
{
public:
    void setLevel (float newPeak)
    {
        level = std::max (newPeak, level * 0.85f);
        clipHoldFrames = newPeak >= 1.0f ? 30 : std::max (0, clipHoldFrames - 1);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (bounds, 3.0f);

        const auto db = juce::Decibels::gainToDecibels (level, minDb);
        const auto proportion = juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb);

        const auto colour = db > -3.0f ? juce::Colours::red
                          : db > -12.0f ? juce::Colours::yellow
                                        : juce::Colours::limegreen;
        g.setColour (colour);
        g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * proportion), 3.0f);

        if (clipHoldFrames > 0)
        {
            g.setColour (juce::Colours::red);
            g.fillRoundedRectangle (bounds.removeFromRight (6.0f), 3.0f);
        }
    }

private:
    static constexpr float minDb = -60.0f;
    float level = 0.0f;
    int clipHoldFrames = 0;
};
