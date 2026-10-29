#pragma once

#include "app/views/PlaybackView.h"
#include "app/views/SongLayout.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// Song structure as coloured blocks: one block per part, thin lines per bar, stronger lines between
// repeats, a frame around the running part and a moving position marker.
// Long songs wrap into up to three rows (see SongLayout); the height follows from the width.
//
// Click or drag in the strip to move the position; on release onSeek is called with the bar under
// the mouse (index into the song's bars).
class SongStrip : public juce::Component
{
public:
    std::function<void (int bar)> onSeek;

    void setView (const PlaybackView* v) { view = v; repaint(); }
    int preferredHeight (int width) const;   // for the current song at this width

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    static constexpr int rowH = 40, rowGap = 4, margin = 7;

    juce::Rectangle<int> stripArea() const { return getLocalBounds().reduced (6, margin); }
    void relayout();
    juce::Rectangle<float> rowBounds (int row) const;
    int barAt (juce::Point<int>) const;

    const PlaybackView* view = nullptr;
    SongLayout layout;
    int dragBar = -1;   // bar under the mouse while dragging, -1 otherwise
};
