#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The drum kit of the program icon as a very faint picture behind the main window: the kit
// light, H and Z red, the white paper left out. Extracted once from the embedded image
// (assets/drumkit.png, the icon's kit uncropped) and scaled once per window size.
class Watermark
{
public:
    void draw (juce::Graphics&, juce::Rectangle<int> area);

private:
    juce::Image source, scaled;
};
