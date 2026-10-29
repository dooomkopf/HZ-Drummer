#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The program's look: pill-shaped buttons with a soft drop shadow on the dark background.
// Everything else (sliders, combo boxes, text fields) stays JUCE's dark default.
// Installed as the default look-and-feel in Main.cpp; the arranger's Look derives from it.
class HZLook : public juce::LookAndFeel_V4
{
public:
    HZLook();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // every JUCE message box appears inside its window, see InWindowDialog
    juce::AlertWindow* createAlertWindow (const juce::String& title, const juce::String& message,
                                          const juce::String& button1, const juce::String& button2, const juce::String& button3,
                                          juce::MessageBoxIconType iconType, int numButtons,
                                          juce::Component* associatedComponent) override;

    static const juce::Colour buttonFill, buttonOn, textColour;
};
