#pragma once

#include "app/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// The "Ablage": collected building blocks as cards (name, meter, length, mini preview).
// Click = audition (again = stop), "+ Ablauf" inserts into the sequence, "x" removes. Cards can
// be dragged into the sequence; rows from "Finden" can be dropped here. Blocks that are part of
// the sequence get a green frame. Nothing is ever removed automatically, only "Ablage leeren"
// (in the toolbar) empties the list.
class ScratchView : public juce::Component,
                    public juce::DragAndDropTarget
{
public:
    std::function<void (int index)> onAudition, onToSequence, onRemove, onContextMenu;   // onContextMenu: right-click on a card
    std::function<void (const juce::var& description)> onDrop;   // from "Finden"
    std::function<bool (int index)> isAuditioning;

    ScratchView();

    // rebuilds the cards; used[i] = this block occurs in the sequence
    void setParts (const std::vector<SongPart>&, const std::vector<bool>& used);

    void resized() override;
    void paint (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    struct Card
    {
        juce::String title, detail;
        hz::Pattern played;     // what the card plays (for the preview); empty for rest/click
        PartKind kind = PartKind::Pattern;
        bool used = false;
        int colour = 0, shape = 0;   // partColourIndex / partShapeIndex: the badge of its blocks
    };

    // The cards, wrapped into rows; scrolls inside the viewport
    class Content : public juce::Component
    {
    public:
        explicit Content (ScratchView& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    private:
        ScratchView& owner;
        int hover = -1;
        int pressedCard = -1;   // card body under the button (drag source / audition target), -1 = none
        bool dragging = false;
    };

    juce::Rectangle<int> cardBounds (int index) const;
    int cardAt (juce::Point<int>) const;
    int columns() const;
    void layoutCards();

    std::vector<Card> cards;
    juce::Label header;
    juce::Viewport viewport;
    Content content;
    bool dropHover = false;
};
