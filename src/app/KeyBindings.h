#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// One keyboard shortcut. The same table drives key handling and the help overlay.
struct KeyBinding
{
    juce::String keys, description;   // shown in the help (empty keys: hidden companion entry)
    std::function<bool (const juce::KeyPress&)> matches;
    std::function<void()> action;
};

// Named actions of the main window; the bindings only refer to these
struct KeyActions
{
    std::function<void()> togglePlay, rewind, tapTempo, toggleFavorite, nextSound, toggleBeatDisplay;
    std::function<void (double)> changeTempo;
    std::function<void (int, bool)> stepPattern;   // direction, favorites only
    std::function<void (int, bool)> stepCurrent;   // pattern or meter, depending on the mode
    std::function<void (int)> stepStyle;
    std::function<void (int)> seekPart;            // songs: next (+1) / previous (-1) part
    std::function<void()> patternMenu, meterMenu, editor, mixer, search, help;
};

std::vector<KeyBinding> makeKeyBindings (const KeyActions&);

// Keys that also work in other windows (the workshop): space, tempo up/down, tap, M for the mixer
bool isTransportKey (const juce::KeyPress&);
