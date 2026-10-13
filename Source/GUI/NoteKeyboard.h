#pragma once

#include <JuceHeader.h>
#include "../Midi/NoteController.h"

//==============================================================================
/*
    Piano-style keyboard for choosing harmony notes. Keys are absolute *written*
    notes (NoteController::lowestNote..highestNote): with the controller set to
    Eb, pressing written C5 makes a voice sound what an alto fingering C5 sounds
    (concert Eb4). Clicking a key toggles that note in the NoteController.
    White keys show the written name, plus the concert name when transposed.
*/
class NoteKeyboard : public juce::Component
{
public:
    explicit NoteKeyboard (NoteController& controller);

    // Called (on the GUI thread) after a click changes the selection
    std::function<void()> onSelectionChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    static constexpr int noKey = -1;

    static bool isBlackKey (int note);
    static int getNumWhiteKeys();
    juce::Rectangle<float> getKeyBounds (int note) const;
    int getKeyAt (juce::Point<float> position) const;
    void drawKey (juce::Graphics& g, int note) const;
    void setHoveredKey (int note);

    NoteController& notes;
    int hoveredKey = noKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteKeyboard)
};
