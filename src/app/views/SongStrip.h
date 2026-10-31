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
// the mouse (index into the song's bars). Shift + drag picks a loop range of whole bars (onLoopRange);
// Shift + drag near one of its edges moves just that edge, inside it does nothing, outside it starts a new range. The range
// is drawn only while the loop is on; a double click on it clears it (onLoopClear).
class SongStrip : public juce::Component
{
public:
    std::function<void (int bar)> onSeek;
    std::function<void (int firstBar, int lastBar)> onLoopRange;
    std::function<void()> onLoopClear;
    void setLoopRange (int firstBar, int lastBar, bool shown);

    void setView (const PlaybackView* v) { view = v; repaint(); }
    int preferredHeight (int width) const;   // for the current song at this width

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    static constexpr int rowH = 40, rowGap = 4, margin = 7;

    juce::Rectangle<int> stripArea() const { return getLocalBounds().reduced (6, margin); }
    void relayout();
    juce::Rectangle<float> rowBounds (int row) const;
    int barAt (juce::Point<int>) const;

    const PlaybackView* view = nullptr;
    SongLayout layout;
    int dragBar = -1;   // bar under the mouse while dragging, -1 otherwise
    int rangeFrom = -1, rangeTo = -1;   // Shift + drag in progress: the bars picked so far
    bool ignoreGesture = false;         // Shift + drag inside the range: nothing at all, not even a seek
    int loopFirst = -1, loopLast = -1;  // the loop range
    bool loopShown = false;             // ... drawn only while the loop is on
    void paintRange (juce::Graphics&, int first, int last);
    bool nearEdge (juce::Point<float>, int bar, bool atItsEnd) const;   // close to where the range begins or ends
};
