#pragma once

#include "app/views/PlaybackView.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The three big panels: meter | tempo | bar (bar within song part, or loop count)
class StatusPanels : public juce::Component
{
public:
    void setView (const PlaybackView* v) { view = v; repaint(); }
    void paint (juce::Graphics&) override;

private:
    const PlaybackView* view = nullptr;
};
