#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The help line at the foot of the arranger and the raster: the built-in text or the user's own,
// light red like the key hints in the pattern menu. A small "Edit" at its end opens a dialog: own
// text (empty = no help line) or Reset to the built-in one. The own text is a file in the data
// folder (~/.config/HZ-Drummer/<fileName>), so it travels with the data repo. No file = the
// built-in text, so improvements of it reach everyone who did not write their own.
class HelpLine : public juce::Component
{
public:
    // defaultText is asked on every paint: it may depend on state (the raster's block mode)
    HelpLine (juce::String fileName, std::function<juce::String()> defaultText, float fontSize, int maxLines);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::File file() const;
    juce::String text() const;
    void askText();

    juce::String fileName;
    std::function<juce::String()> defaultText;
    float fontSize;
    int maxLines;
    bool hasOwn = false;      // the file exists (its text may be empty)
    juce::String ownText;
    juce::TextButton editButton { "Edit" };
};
