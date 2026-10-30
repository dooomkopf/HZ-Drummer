#pragma once

#include "app/Song.h"
#include "app/views/SongLayout.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// The "Ablauf": the song's parts as proportional blocks (same picture as the song strip in
// the main window, wrapped into rows). Click selects, dragging a block reorders it, blocks from
// "Finden" or the Ablage can be dropped at the insertion mark, +/- keys change the repeats,
// double-click renames, right-click opens the block menu. Shows the playhead.
class SequenceView : public juce::Component,
                     public juce::DragAndDropTarget
{
public:
    std::function<void (int part)> onSelect, onContextMenu;   // right-click on a block
    std::function<void (int part)> onClick;   // a completed click on a block (no drag, not on [-] [+])
    std::function<void (int from, int to)> onMove;
    std::function<void (const juce::var& description, int insertIndex)> onDrop;
    std::function<void (int part, int delta)> onRepeats;

    void setSpans (const std::vector<SongSpan>*);   // nullptr or empty = no parts
    void setErrorText (const juce::String&);        // why the sequence cannot be built ("" = fine)
    juce::Rectangle<int> boundsOfPart (int part) const;   // empty if not laid out
    void setSelected (int part);
    int getSelected() const            { return selectedPart; }
    void setPlayPosition (double bar); // -1 = none
    int preferredHeight (int width) const;
    static int heightForRows (int rows)   { return 2 * margin + rows * rowH + (rows - 1) * rowGap; }

    void paint (juce::Graphics&) override;
    void resized() override            { relayout(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override  { insertMark = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    static constexpr int rowH = 54, rowGap = 6, margin = 6;

    void relayout();
    juce::Rectangle<float> rowBounds (int row) const;
    juce::Rectangle<float> blockRect (const SongLayout::Block&) const;
    const SongLayout::Block* blockAt (juce::Point<int>) const;
    int insertIndexAt (juce::Point<int>) const;   // part index to insert before (size() = append)

    const std::vector<SongSpan>* spans = nullptr;
    SongLayout layout;
    juce::String errorText;
    int selectedPart = -1;
    int insertMark = -1;    // while something hovers: part index to insert before
    double position = -1.0;
    bool dragging = false;
    int pressedPart = -1;           // block under the button, for onClick
};
