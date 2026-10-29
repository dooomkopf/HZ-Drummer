#include "app/views/HZLook.h"

const juce::Colour HZLook::buttonFill  { 0xff34343d };
const juce::Colour HZLook::buttonOn    { 0xff2e7a4a };
const juce::Colour HZLook::textColour  { 0xffeeeeee };

HZLook::HZLook()
{
    setColour (juce::TextButton::buttonColourId, buttonFill);
    setColour (juce::TextButton::buttonOnColourId, buttonOn);
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, textColour);
}

void HZLook::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                   bool highlighted, bool down)
{
    // 'backgroundColour' already reflects the toggle state (buttonColourId / buttonOnColourId)
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const float radius = bounds.getHeight() * 0.5f;
    juce::Path pill;
    pill.addRoundedRectangle (bounds, radius);

    if (button.isEnabled())
    {
        // soft shadow below, so the pill lifts off the dark panel
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 6, { 0, 2 }).drawForPath (g, pill);
    }

    auto fill = backgroundColour;
    if (! button.isEnabled())
        fill = fill.withMultipliedAlpha (0.45f);
    else if (down)
        fill = fill.darker (0.25f);
    else if (highlighted)
        fill = fill.brighter (0.18f);

    // a slight top-to-bottom gradient keeps the pill from looking flat
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.08f), bounds.getX(), bounds.getY(),
                                             fill.darker (0.08f), bounds.getX(), bounds.getBottom(), false));
    g.fillPath (pill);
    g.setColour (juce::Colours::white.withAlpha (button.isEnabled() ? 0.10f : 0.04f));
    g.strokePath (pill, juce::PathStrokeType (1.0f));
}

void HZLook::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    auto colour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    g.setColour (button.isEnabled() ? colour : colour.withAlpha (0.4f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (10, 2), juce::Justification::centred, 1);
}

juce::Font HZLook::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (16.0f, (float) buttonHeight * 0.45f), juce::Font::bold));
}
