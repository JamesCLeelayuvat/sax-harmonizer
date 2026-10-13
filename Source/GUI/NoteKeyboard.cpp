#include "NoteKeyboard.h"

namespace
{
    const juce::Colour selectedColour { 0xffff9f1c };
    const juce::Colour blackKeyColour { 0xff1e1e1e };

    // Flats rather than sharps, since sax music is usually written that way
    juce::String noteName (int note, bool includeOctave)
    {
        return juce::MidiMessage::getMidiNoteName (note, false, includeOctave, 4);
    }
}

NoteKeyboard::NoteKeyboard (NoteController& controller)
    : notes (controller)
{
}

bool NoteKeyboard::isBlackKey (int note)
{
    const int pitchClass = note % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
}

int NoteKeyboard::getNumWhiteKeys()
{
    int count = 0;
    for (int n = NoteController::lowestNote; n <= NoteController::highestNote; ++n)
        if (! isBlackKey (n))
            ++count;
    return count;
}

juce::Rectangle<float> NoteKeyboard::getKeyBounds (int note) const
{
    // Number of white keys to the left of this key
    int whiteIndex = 0;
    for (int n = NoteController::lowestNote; n < note; ++n)
        if (! isBlackKey (n))
            ++whiteIndex;

    const auto whiteWidth = (float) getWidth() / (float) getNumWhiteKeys();
    const auto height = (float) getHeight();

    if (! isBlackKey (note))
        return { (float) whiteIndex * whiteWidth, 0.0f, whiteWidth, height };

    // Black keys straddle the line between two white keys
    const auto blackWidth = whiteWidth * 0.6f;
    return { (float) whiteIndex * whiteWidth - blackWidth * 0.5f, 0.0f, blackWidth, height * 0.6f };
}

int NoteKeyboard::getKeyAt (juce::Point<float> position) const
{
    // Black keys are drawn on top, so check them first
    for (const bool black : { true, false })
        for (int n = NoteController::lowestNote; n <= NoteController::highestNote; ++n)
            if (isBlackKey (n) == black && getKeyBounds (n).contains (position))
                return n;

    return noKey;
}

void NoteKeyboard::paint (juce::Graphics& g)
{
    for (const bool black : { false, true })
        for (int n = NoteController::lowestNote; n <= NoteController::highestNote; ++n)
            if (isBlackKey (n) == black)
                drawKey (g, n);
}

void NoteKeyboard::drawKey (juce::Graphics& g, int note) const
{
    const auto bounds = getKeyBounds (note);
    const bool black = isBlackKey (note);
    const bool selected = notes.isNoteEnabled (note);
    const bool full = notes.getNumActiveVoices() >= NoteController::maxVoices;
    const bool selectable = selected || ! full;

    auto fill = black ? blackKeyColour : juce::Colours::white;
    if (selected)
        fill = black ? selectedColour.darker (0.25f) : selectedColour;
    else if (note == hoveredKey && selectable)
        fill = black ? fill.brighter (0.35f) : fill.darker (0.1f);

    g.setColour (fill);
    g.fillRect (bounds);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRect (bounds, 1.0f);

    auto textColour = (black && ! selected) ? juce::Colours::white : juce::Colours::black;
    if (! selectable)
        textColour = textColour.withAlpha (0.35f);
    g.setColour (textColour);

    // Black keys are too narrow for more than the note letter
    if (black)
    {
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawFittedText (noteName (note, false), bounds.reduced (1.0f, 4.0f).toNearestInt(),
                          juce::Justification::centredBottom, 1);
        return;
    }

    // White keys: written name, with the concert name underneath when transposed
    auto textArea = bounds.reduced (2.0f, 6.0f).toNearestInt();
    const int offset = NoteController::getConcertOffset (notes.getTransposition());

    if (offset != 0)
    {
        g.setFont (juce::FontOptions (10.0f));
        g.drawFittedText (noteName (note + offset, true), textArea.removeFromBottom (14),
                          juce::Justification::centred, 1);
    }

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (noteName (note, true), textArea.removeFromBottom (16),
                      juce::Justification::centred, 1);
}

void NoteKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const int key = getKeyAt (e.position);
    if (key == noKey)
        return;

    if (notes.setNoteEnabled (key, ! notes.isNoteEnabled (key)))
    {
        repaint();
        if (onSelectionChanged)
            onSelectionChanged();
    }
}

void NoteKeyboard::mouseMove (const juce::MouseEvent& e)
{
    setHoveredKey (getKeyAt (e.position));
}

void NoteKeyboard::mouseExit (const juce::MouseEvent&)
{
    setHoveredKey (noKey);
}

void NoteKeyboard::setHoveredKey (int note)
{
    if (note != hoveredKey)
    {
        hoveredKey = note;
        repaint();
    }
}
