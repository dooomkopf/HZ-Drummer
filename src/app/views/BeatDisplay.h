#pragma once

#include "app/views/PlaybackView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// The pulse circles of the current bar (with group gaps and accent colours) and, optionally below
// them, every hit of the bar at its exact position in three lanes (cymbals, snare/toms, feet).
// Hits between the sixteenths (polyrhythms) are drawn as diamonds; hits light up when heard, and
// a progress bar runs through the bar.
class BeatDisplay : public juce::Component
{
public:
    void setView (const PlaybackView* v) { view = v; repaint(); }
    void setShowHits (bool show)         { showHits = show; repaint(); }
    bool getShowHits() const             { return showHits; }

    void paint (juce::Graphics&) override;

private:
    void paintCircles (juce::Graphics&, juce::Rectangle<int>, std::vector<float>& centres, float& cell);
    void paintHits (juce::Graphics&, juce::Rectangle<int>, const std::vector<float>& centres, float cell);

    const PlaybackView* view = nullptr;
    bool showHits = true;
};
