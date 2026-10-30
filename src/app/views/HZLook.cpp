#include "app/views/HZLook.h"

#include "app/views/InWindowDialog.h"

namespace
{
    // The count-in button's background (the user's draft, variant C): a large stylised metronome
    // on the right, cut off by the pill, behind the centred text. Coordinates in units of the pill
    // height, x from the pill's right edge, y from its top, so it scales cleanly with size and DPI.
    void drawMetronome (juce::Graphics& g, juce::Rectangle<float> pill, float opacity)
    {
        const float h = pill.getHeight();
        auto at = [&] (float x, float y) { return juce::Point<float> (pill.getRight() + x * h, pill.getY() + y * h); };

        // housing sides, running past the top and bottom edge
        constexpr float leftTop = -0.571f, leftBottom = -0.830f, rightTop = -0.323f, rightBottom = -0.153f;
        auto left  = [] (float y) { return leftTop + (leftBottom - leftTop) * y; };
        auto right = [] (float y) { return rightTop + (rightBottom - rightTop) * y; };

        // pendulum: pivot exactly midway between the sides, leaning to the upper right
        constexpr float pivotY = 0.941f, lean = 0.514f, tipY = 0.18f, weightY = 0.3215f;
        const float pivotX = 0.5f * (left (pivotY) + right (pivotY));
        auto rodX = [&] (float y) { return pivotX + lean * (pivotY - y); };

        // the right side pauses where the pendulum and its weight pass it
        constexpr float gapFrom = 0.313f, gapTo = 0.485f, beyond = 0.05f;
        juce::Path housing;
        housing.startNewSubPath (at (left (-beyond), -beyond));
        housing.lineTo (at (left (1.0f + beyond), 1.0f + beyond));
        housing.startNewSubPath (at (right (-beyond), -beyond));
        housing.lineTo (at (right (gapFrom), gapFrom));
        housing.startNewSubPath (at (right (gapTo), gapTo));
        housing.lineTo (at (right (1.0f + beyond), 1.0f + beyond));

        g.setColour (juce::Colours::white.withAlpha (0.30f * opacity));
        g.strokePath (housing, juce::PathStrokeType (juce::jmax (1.0f, 0.028f * h)));

        juce::Path rod;
        rod.startNewSubPath (at (rodX (1.0f + beyond), 1.0f + beyond));
        rod.lineTo (at (rodX (tipY), tipY));
        g.strokePath (rod, juce::PathStrokeType (juce::jmax (1.0f, 0.033f * h)));

        // the small weight, upright on the rod
        juce::Path weight;
        weight.addRectangle (-0.045f * h, -0.0615f * h, 0.09f * h, 0.123f * h);
        weight.applyTransform (juce::AffineTransform::rotation (std::atan (lean)).translated (at (rodX (weightY), weightY)));
        g.fillPath (weight);
    }
}

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

    if (button.getProperties()["metronome"])
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (pill);
        drawMetronome (g, bounds, button.isEnabled() ? 1.0f : 0.5f);
    }

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

juce::AlertWindow* HZLook::createAlertWindow (const juce::String& title, const juce::String& message,
                                              const juce::String& button1, const juce::String& button2, const juce::String& button3,
                                              juce::MessageBoxIconType iconType, int numButtons,
                                              juce::Component* associatedComponent)
{
    // the buttons and margins of LookAndFeel_V2/V4, only the dialog is an InWindowDialog
    auto* aw = new InWindowDialog (title, message, iconType, associatedComponent);

    if (numButtons == 1)
    {
        aw->addButton (button1, 0, juce::KeyPress (juce::KeyPress::escapeKey), juce::KeyPress (juce::KeyPress::returnKey));
    }
    else
    {
        const juce::KeyPress shortcut1 ((int) juce::CharacterFunctions::toLowerCase (button1[0]), 0, 0);
        juce::KeyPress shortcut2 ((int) juce::CharacterFunctions::toLowerCase (button2[0]), 0, 0);
        if (shortcut1 == shortcut2)
            shortcut2 = juce::KeyPress();

        if (numButtons == 2)
        {
            aw->addButton (button1, 1, juce::KeyPress (juce::KeyPress::returnKey), shortcut1);
            aw->addButton (button2, 0, juce::KeyPress (juce::KeyPress::escapeKey), shortcut2);
        }
        else if (numButtons == 3)
        {
            aw->addButton (button1, 1, shortcut1);
            aw->addButton (button2, 2, shortcut2);
            aw->addButton (button3, 0, juce::KeyPress (juce::KeyPress::escapeKey));
        }
    }

    const int margin = 50;
    aw->setBounds (aw->getBounds().withSizeKeepingCentre (aw->getWidth() + margin, aw->getHeight() + margin));
    for (auto* child : aw->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child))
            button->setBounds (button->getBounds() + juce::Point<int> (margin / 2, 40));
    return aw;
}
