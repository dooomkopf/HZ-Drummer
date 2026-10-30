#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The symbols HZLook draws behind (or instead of) the text of some buttons; 'pill' is the button's pill area,
// opacity 1 for an enabled button and 0.5 for a disabled one. White with 30 % opacity unless noted.
namespace buttonsymbols
{
    void drawMetronome (juce::Graphics&, juce::Rectangle<float> pill, float opacity);                // count-in ("Click")
    void drawTransport (juce::Graphics&, juce::Rectangle<float> pill, bool playing, float opacity);  // > or ||
    void drawTap (juce::Graphics&, juce::Rectangle<float> pill, float opacity);                      // speedometer
    void drawTransportIcon (juce::Graphics&, juce::Rectangle<float> pill, bool playing);             // centred, current colour
    void drawMixer (juce::Graphics&, juce::Rectangle<float> pill, float opacity, const juce::GlyphArrangement& text);   // faders, gaps at the letters
    void drawSystem (juce::Graphics&, juce::Rectangle<float> pill, float opacity);                   // gear outline
}
