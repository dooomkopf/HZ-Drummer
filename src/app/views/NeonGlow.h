#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

// A strong neon glow around a pill-shaped button. The parent draws it behind the button, so that it can
// reach beyond the button's own bounds (a component cannot paint outside itself). strength 0..1.
namespace neonglow
{
    inline void draw (juce::Graphics& g, juce::Rectangle<float> pill, juce::Colour colour, float strength)
    {
        juce::Path shape;
        shape.addRoundedRectangle (pill, pill.getHeight() * 0.5f);
        juce::DropShadow halo (colour.withAlpha (1.0f * strength), 40, {});   // wide halo, twice: it reaches far
        halo.drawForPath (g, shape);
        halo.drawForPath (g, shape);
        juce::DropShadow (colour.withAlpha (1.0f * strength), 18, {}).drawForPath (g, shape);   // dense middle
        juce::DropShadow (colour.brighter (0.4f).withAlpha (strength), 6, {}).drawForPath (g, shape);   // hot rim
    }

    // breathing between 0.7 and 1: alive, but always clearly there
    inline float breathing()
    {
        return 0.85f + 0.15f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.0026);
    }
}
