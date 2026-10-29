#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Thin blue glow along the top edge of a window while it is the active one, so it is clear at
// a glance which window gets the keys. The native title bar belongs to the window manager and
// cannot be coloured, hence this strip right below it. Lets all mouse clicks through.
class ActiveGlow : public juce::Component,
                   private juce::Timer
{
public:
    static constexpr int height = 6;

    ActiveGlow()
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        if (! active)
            return;
        const auto r = getLocalBounds().toFloat();
        const juce::Colour glow (0xff48a0ff);
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.55f), 0.0f, r.getY(),
                                                 glow.withAlpha (0.0f), 0.0f, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (glow);
        g.fillRect (r.withHeight (2.0f));
    }

private:
    void timerCallback() override
    {
        auto* window = dynamic_cast<juce::TopLevelWindow*> (getTopLevelComponent());
        const bool now = window != nullptr && window->isActiveWindow();
        if (now != active)
        {
            active = now;
            repaint();
        }
    }

    bool active = false;
};

// Wraps a dialog's content so the glow runs along its top edge as well (the main window and the
// workshop own their glow directly). The dialog's content component is then the frame.
class GlowFrame : public juce::Component
{
public:
    explicit GlowFrame (juce::Component* contentToOwn) : inner (contentToOwn)
    {
        addAndMakeVisible (inner.get());
        addAndMakeVisible (glow);
        setSize (inner->getWidth(), inner->getHeight());
    }

    juce::Component* content() const { return inner.get(); }

    void resized() override
    {
        inner->setBounds (getLocalBounds());
        glow.setBounds (getLocalBounds().withHeight (ActiveGlow::height));
        glow.toFront (false);
    }

private:
    std::unique_ptr<juce::Component> inner;
    ActiveGlow glow;
};
