#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The program's look: pill-shaped buttons with a soft drop shadow on the dark background.
// Everything else (sliders, combo boxes, text fields) stays JUCE's dark default,
// except a combo box with the property "pill": it looks like the selection buttons beside it,
// and a text editor with the property "pill" (the search fields), which gets the pill shape.
// Installed as the default look-and-feel in Main.cpp; the arranger's Look derives from it.
// A button with the property "metronome" gets a metronome behind its text (the count-in button).
// A button with "transport" gets > or || behind its text, selected by its "playing" property.
// A button with "tap" gets a speedometer behind its text (the tap-tempo button), "mixer" four faders,
// "system" a gear outline. A text button with "transportIcon" shows > or || (its "playing") instead of its text.
class HZLook : public juce::LookAndFeel_V4
{
public:
    HZLook();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;   // bold only with the property "bold"

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // a slider with the property "pill" gets its value box as a pill (set it before setTextBoxStyle)
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // every JUCE message box appears inside its window, see InWindowDialog
    juce::AlertWindow* createAlertWindow (const juce::String& title, const juce::String& message,
                                          const juce::String& button1, const juce::String& button2, const juce::String& button3,
                                          juce::MessageBoxIconType iconType, int numButtons,
                                          juce::Component* associatedComponent) override;

    static const juce::Colour buttonFill, buttonOn, textColour;
};
