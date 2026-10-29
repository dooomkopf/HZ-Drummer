#pragma once

#include "engine/Meter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <vector>

// All available meters (catalog + custom), favorites and recently used.
// The table has a fixed capacity: entries are only appended and never modified,
// so the audio thread may read a published index at any time.
class MeterLibrary
{
public:
    static constexpr int capacity = 256;

    MeterLibrary();

    int size() const { return count; }
    const hz::Meter& get (int index) const { return table[(size_t) index]; }
    juce::String label (int index) const;

    int indexOf (const hz::Meter&) const;
    int findOrAdd (const hz::Meter&);   // -1 if the table is full
    bool isCustom (int index) const { return index >= catalogSize; }

    // Browse: dir = +1/-1, wraps around at the end
    int step (int current, int dir) const;
    int stepFavorite (int current, int dir) const;

    bool isFavorite (int index) const;
    void toggleFavorite (int index);
    void noteUsed (int index);

    struct MenuCallbacks
    {
        std::function<void (int)> select;
        std::function<void()> custom;
        std::function<void (int)> toggleFavorite;
        juce::String hint;   // optional line at the top (e.g. "selecting switches to the metronome")
    };
    juce::PopupMenu buildMenu (int current, MenuCallbacks) const;

    // Stored as text ("7/8:2+2+3") so catalog changes don't break saved data
    static juce::String key (const hz::Meter&);
    int fromKey (const juce::String&);   // adds unknown valid meters as custom ones; -1 on error
    void save (juce::PropertiesFile&) const;
    void load (juce::PropertiesFile&);

    // "Eigene Taktart" dialog: asks for e.g. "17/16 4+4+4+5", creates it and reports the index
    void askCustomMeter (juce::Component* parent, int current, std::function<void (int)> onSelected);

private:
    juce::StringArray keysOf (const std::vector<int>&) const;
    std::vector<int> indicesOf (const juce::StringArray&);

    std::array<hz::Meter, capacity> table {};
    int count = 0;
    int catalogSize = 0;
    std::vector<int> favorites;   // ascending by table index
    std::vector<int> recent;      // most recently used first
};
