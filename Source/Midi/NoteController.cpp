#include "NoteController.h"
#include "../Utils/TuningUtils.h"

namespace
{
    int countBits(std::uint64_t mask)
    {
        int count = 0;
        for (; mask != 0; mask &= mask - 1) // clears the lowest set bit each time
            ++count;
        return count;
    }
}

int NoteController::getConcertOffset(Transposition transposition)
{
    switch (transposition)
    {
    case Transposition::bFlat:
        return -2;
    case Transposition::eFlat:
        return -9;
    case Transposition::concert:
        break;
    }

    return 0;
}

bool NoteController::isInRange(int writtenNote)
{
    return writtenNote >= lowestNote && writtenNote <= highestNote;
}

std::uint64_t NoteController::bitFor(int writtenNote)
{
    return std::uint64_t{1} << (writtenNote - lowestNote);
}

bool NoteController::setNoteEnabled(int writtenNote, bool enabled)
{
    if (!isInRange(writtenNote))
        return false;

    const auto mask = enabledMask.load();

    if (enabled)
    {
        if ((mask & bitFor(writtenNote)) == 0 && countBits(mask) >= maxVoices)
            return false;

        enabledMask = mask | bitFor(writtenNote);
    }
    else
    {
        enabledMask = mask & ~bitFor(writtenNote);
    }

    return true;
}

bool NoteController::isNoteEnabled(int writtenNote) const
{
    return isInRange(writtenNote) && (enabledMask.load() & bitFor(writtenNote)) != 0;
}

void NoteController::clear()
{
    enabledMask = 0;
}

void NoteController::setTransposition(Transposition newTransposition)
{
    transposition = newTransposition;
}

NoteController::Transposition NoteController::getTransposition() const
{
    return transposition.load();
}

int NoteController::getNumActiveVoices() const
{
    return countBits(enabledMask.load());
}


int NoteController::getWrittenNoteFor(int voiceIndex) const
{
    const auto mask = enabledMask.load();
    int voice = 0;

    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if ((mask & bitFor(note)) == 0)
            continue;

        if (voice == voiceIndex)
            return note;

        ++voice;
    }

    return -1;
}

int NoteController::getConcertNoteFor(int voiceIndex) const
{
    const int written = getWrittenNoteFor(voiceIndex);
    return written < 0 ? -1 : written + getConcertOffset(getTransposition());
}

float NoteController::getSemitoneShiftFor(int voiceIndex, float inputFrequency) const
{
    const int target = getConcertNoteFor(voiceIndex);
    if (target < 0 || inputFrequency <= 0.0f)
        return 0.0f;

    return (float)target - TuningUtils::frequencyToMidiNote(inputFrequency);
}
