#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Button with text, optional golden favorite star and menu arrow
class StarButton : public juce::Button
{
public:
    static inline const juce::Colour gold { 0xffffc23d };

    StarButton() : juce::Button ({}) {}

    void setContent (const juce::String& newLabel, bool favorite)
    {
        label = newLabel;
        star = favorite;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto& lf = getLookAndFeel();
        lf.drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);

        auto area = getLocalBounds().reduced (10, 0);
        const float h = (float) getHeight();

        g.setColour (findColour (juce::TextButton::textColourOffId));
        g.setFont (juce::FontOptions (h * 0.36f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), area.removeFromRight (14), juce::Justification::centred);

        if (star)
        {
            g.setColour (gold);
            g.setFont (juce::FontOptions (h * 0.48f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), area.removeFromRight (22), juce::Justification::centred);
        }

        g.setColour (findColour (juce::TextButton::textColourOffId));
        g.setFont (juce::FontOptions (h * 0.36f));
        g.drawFittedText (label, area, juce::Justification::centredLeft, 1);
    }

private:
    juce::String label;
    bool star = false;
};
