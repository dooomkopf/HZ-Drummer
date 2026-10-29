#pragma once

#include "engine/Pattern.h"

#include "app/views/SourceBadge.h"

#include <juce_graphics/juce_graphics.h>

#include <iterator>

// Shared colours of the main window and its views
namespace theme
{
    const juce::Colour background   { 0xff121214 };
    const juce::Colour panel        { 0xff1c1c20 };
    const juce::Colour textMain     { 0xffeeeeee };
    const juce::Colour textDim      { 0xff8a8a92 };
    const juce::Colour downbeat     { 0xffe0443a };
    const juce::Colour groupStart   { 0xffe89a2c };
    const juce::Colour weakBeat     { 0xff5c8fd6 };
    const juce::Colour unlit        { 0xff2c2c33 };
    const juce::Colour progress     { 0xff48a0ff };

    inline juce::Colour accentColour (hz::Meter::Accent a)
    {
        return a == hz::Meter::Accent::Downbeat ? downbeat : a == hz::Meter::Accent::Group ? groupStart : weakBeat;
    }

    // Colour per instrument family (beat display, editor)
    inline juce::Colour voiceColour (hz::Voice v)
    {
        using I = hz::Instrument;
        switch (v.instrument)
        {
            case I::Kick:  return juce::Colour (0xffe0443a);
            case I::Snare: return juce::Colour (0xffeeeeee);
            case I::HiHat: return juce::Colour (0xff3cc6b4);
            case I::Ride:  return juce::Colour (0xffe8c547);
            case I::Tom1: case I::Tom2: case I::Tom3: case I::Tom4: return juce::Colour (0xffa77de8);
            case I::Crash1: case I::Crash2: case I::China: case I::Splash: return juce::Colour (0xffe89a2c);
            case I::Cowbell: case I::Count: break;
        }
        return juce::Colour (0xff5c8fd6);
    }
}
