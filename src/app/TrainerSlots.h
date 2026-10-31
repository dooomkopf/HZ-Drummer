#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <string>

// The speed trainer's save slots: a practice spot under a name of one's own, so that it never has to be
// searched and set up again. A slot holds what was selected (a pattern or arrangement, else the metronome in
// a meter), the loop range and the trainer's values. Kept in ~/.config/HZ-Drummer/trainer.json.
struct TrainerSlot
{
    juce::String name;          // empty: the slot is free
    std::string patternId;      // the pattern or arrangement (library id); empty: the metronome ...
    juce::String meterKey;      // ... in this meter (MeterLibrary::key)
    int loopFirst = -1, loopLast = -1;   // bars of the range, -1: none
    bool loopOn = false;
    double from = 100.0, to = 140.0, step = 1.0;
    int repeats = 1;

    bool isFree() const { return name.isEmpty(); }
};

namespace trainerslots
{
    constexpr int count = 10;
    using Slots = std::array<TrainerSlot, count>;

    juce::File file();                                  // ~/.config/HZ-Drummer/trainer.json
    Slots load (const juce::File&);                     // missing or broken file: all free
    bool save (const Slots&, const juce::File&);        // false if it could not be written
}
