#include "app/views/HZLook.h"
#include "app/views/Theme.h"

#include "app/views/InWindowDialog.h"

#include "app/views/ButtonSymbols.h"

using namespace buttonsymbols;

const juce::Colour HZLook::buttonFill  { 0xff34343d };
const juce::Colour HZLook::buttonOn    { 0xff2e7a4a };
const juce::Colour HZLook::textColour  { 0xffeeeeee };

HZLook::HZLook()
{
    setColour (juce::TextButton::buttonColourId, buttonFill);
    setColour (juce::TextButton::buttonOnColourId, buttonOn);
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, textColour);
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffe07a70));   // reddish, as the help lines
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
    const bool glow = button.getProperties()["glow"];   // the workshop is open (MainComponent draws the halo)
    if (glow)
        fill = fill.interpolatedWith (theme::workshopGlow, 0.5f);
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
    if (glow)
    {
        g.setColour (theme::workshopGlow.brighter (0.35f));
        g.strokePath (pill, juce::PathStrokeType (2.0f));
    }

    const auto& props = button.getProperties();
    if (props["metronome"] || props["transport"] || props["tap"] || props["mixer"] || props["system"])
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (pill);
        const float opacity = button.isEnabled() ? 1.0f : 0.5f;
        if (props["transport"])
            drawTransport (g, bounds, props["playing"], opacity);
        else if (props["tap"])
            drawTap (g, bounds, opacity);
        else if (props["mixer"])
        {
            // the text laid out as drawButtonText draws it, so the faders can make way for the letters
            juce::GlyphArrangement text;
            if (auto* textButton = dynamic_cast<juce::TextButton*> (&button))
                text.addFittedText (getTextButtonFont (*textButton, button.getHeight()), button.getButtonText(),
                                    10.0f, 2.0f, (float) button.getWidth() - 20.0f, (float) button.getHeight() - 4.0f,
                                    juce::Justification::centred, 1);
            drawMixer (g, bounds, opacity, text);
        }
        else if (props["system"])
            drawSystem (g, bounds, opacity);
        else
            drawMetronome (g, bounds, opacity);
    }

    g.setColour (juce::Colours::white.withAlpha (button.isEnabled() ? 0.10f : 0.04f));
    g.strokePath (pill, juce::PathStrokeType (1.0f));
}

void HZLook::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    auto colour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    g.setColour (button.isEnabled() ? colour : colour.withAlpha (0.4f));
    if (button.getProperties()["transportIcon"])
        drawTransportIcon (g, button.getLocalBounds().toFloat().reduced (1.0f, 2.0f), button.getProperties()["playing"]);
    else
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (10, 2), juce::Justification::centred, 1);
}

juce::Font HZLook::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (16.0f, (float) buttonHeight * 0.45f),
                                          button.getProperties()["bold"] ? juce::Font::bold : juce::Font::plain));
}

void HZLook::drawComboBox (juce::Graphics& g, int width, int height, bool down,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    if (! box.getProperties()["pill"])
    {
        LookAndFeel_V4::drawComboBox (g, width, height, down, buttonX, buttonY, buttonW, buttonH, box);
        return;
    }

    // the pill of drawButtonBackground and the arrow of StarButton
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height).reduced (1.0f, 2.0f);
    juce::Path pill;
    pill.addRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    if (box.isEnabled())
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 6, { 0, 2 }).drawForPath (g, pill);

    auto fill = buttonFill;
    if (! box.isEnabled())
        fill = fill.withMultipliedAlpha (0.45f);
    else if (down)
        fill = fill.darker (0.25f);

    g.setGradientFill (juce::ColourGradient (fill.brighter (0.08f), bounds.getX(), bounds.getY(),
                                             fill.darker (0.08f), bounds.getX(), bounds.getBottom(), false));
    g.fillPath (pill);
    g.setColour (juce::Colours::white.withAlpha (box.isEnabled() ? 0.10f : 0.04f));
    g.strokePath (pill, juce::PathStrokeType (1.0f));

    g.setColour (box.isEnabled() ? textColour : textColour.withAlpha (0.4f));
    g.setFont (juce::FontOptions ((float) height * 0.36f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")),
                juce::Rectangle<int> (width, height).reduced (10, 0).removeFromRight (14), juce::Justification::centred);
}

void HZLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    if (! box.getProperties()["pill"])
    {
        LookAndFeel_V4::positionComboBoxText (box, label);
        return;
    }
    label.setBounds (box.getLocalBounds().reduced (10, 0).withTrimmedRight (14));
    label.setBorderSize ({});
    label.setFont (getComboBoxFont (box));
}

juce::Font HZLook::getComboBoxFont (juce::ComboBox& box)
{
    if (! box.getProperties()["pill"])
        return LookAndFeel_V4::getComboBoxFont (box);
    return juce::Font (juce::FontOptions ((float) box.getHeight() * 0.36f,    // as StarButton's label
                                          box.getProperties()["bold"] ? juce::Font::bold : juce::Font::plain));
}

void HZLook::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.getProperties()["pill"])
    {
        LookAndFeel_V4::fillTextEditorBackground (g, width, height, editor);
        return;
    }
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height).reduced (1.0f);
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
}

void HZLook::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.getProperties()["pill"])
    {
        LookAndFeel_V4::drawTextEditorOutline (g, width, height, editor);
        return;
    }
    if (! editor.isEnabled())
        return;
    // the outlines of LookAndFeel_V4, only rounded: 2 px in the focus colour while typing
    const bool focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height).reduced (focused ? 1.0f : 0.5f);
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, focused ? 2.0f : 1.0f);
}

juce::Label* HZLook::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    if (slider.getProperties()["pill"])
    {
        label->getProperties().set ("pill", true);
        // typing a value: the text field the label opens is a pill too
        label->onEditorShow = [label]
        {
            if (auto* editor = label->getCurrentTextEditor())
                editor->getProperties().set ("pill", true);
        };
    }
    return label;
}

void HZLook::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (! label.getProperties()["pill"])
    {
        LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    // LookAndFeel_V2::drawLabel with round ends
    const auto bounds = label.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = bounds.getHeight() * 0.5f;
    const float alpha = label.isEnabled() ? 1.0f : 0.5f;
    g.setColour (label.findColour (juce::Label::backgroundColourId));
    g.fillRoundedRectangle (bounds, radius);

    if (! label.isBeingEdited())
    {
        const auto font = getLabelFont (label);
        const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());
    }

    g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
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
