#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// Semi-transparent key overview over the window; click closes it.
class HelpOverlay : public juce::Component
{
public:
    struct Entry
    {
        juce::String keys, description;
    };

    void setEntries (std::vector<Entry> e) { entries = std::move (e); repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }

private:
    std::vector<Entry> entries;
};
