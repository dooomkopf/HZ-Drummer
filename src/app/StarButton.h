#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

// The main window's menu buttons: the name, a gold star when it is a favourite, the arrow of a menu. The pattern
// button also carries a small disc symbol with its own click, the switch between the own folders and the
// view Band › Album › Song (lit in the album view).
class StarButton : public juce::Button
{
public:
    static inline const juce::Colour gold { 0xffffc23d };
    static inline const juce::Colour discLit { 0xffa9dfe8 };   // the album view's light blue-green, as its menu rows

    StarButton() : juce::Button ({})
    {
        addChildComponent (disc);
        disc.onClick = [this] { if (onDisc) onDisc(); };
    }

    void setContent (const juce::String& newLabel, bool favorite)
    {
        label = newLabel;
        star = favorite;
        repaint();
    }

    // the disc symbol left of the arrow: its own click (the view switch), not the button's
    std::function<void()> onDisc;
    void setDisc (bool shown, bool lit, const juce::String& tooltip)
    {
        disc.setVisible (shown);
        disc.lit = lit;
        disc.setTooltip (tooltip);
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        area.removeFromRight (14);                    // the arrow
        disc.setBounds (area.removeFromRight (24));
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
        if (disc.isVisible())
            area.removeFromRight (24);

        if (star)
        {
            g.setColour (gold);
            g.setFont (juce::FontOptions (h * 0.48f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), area.removeFromRight (22), juce::Justification::centred);
        }

        g.setColour (findColour (juce::TextButton::textColourOffId));
        g.setFont (juce::FontOptions (h * 0.36f));   // plain: only the first row of the main window is bold
        g.drawFittedText (label, area, juce::Justification::centredLeft, 1);
    }

private:
    struct Disc : juce::Component, juce::SettableTooltipClient
    {
        std::function<void()> onClick;
        bool lit = false;

        Disc() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            const auto colour = lit ? StarButton::discLit : juce::Colour (0xff6a6a74);
            const auto b = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 14.0f);
            g.setColour (colour);
            g.drawEllipse (b, 1.8f);
            g.fillEllipse (b.withSizeKeepingCentre (4.0f, 4.0f));
            if (lit)   // a faint ring: it glows a little
            {
                g.setColour (colour.withAlpha (0.35f));
                g.drawEllipse (b.expanded (2.5f), 1.5f);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mods.isLeftButtonDown() && ! e.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains (e.getPosition()) && onClick)
                onClick();
        }
    } disc;

    juce::String label;
    bool star = false;
};
