#pragma once

#include "app/SearchIndex.h"
#include "app/Text.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

// "Finden": search field, recognised filters as chips, results grouped by category with a
// checkbox per row, and an action row for the marked rows. Click on a row = audition (again =
// stop), click on the box = mark without playing. Rows can be dragged into the Ablage or the
// sequence. Keyboard in the list: arrows move (Shift extends the marks), Space auditions,
// X marks, Enter adds the marked rows (or the focused one) to the Ablage, Ctrl+K goes back
// to the field.
class FindView : public juce::Component
{
public:
    std::function<void (const std::string& id)> onAudition;                       // toggle
    std::function<void (const std::vector<std::string>& ids)> onAddToScratch, onAddToSequence;
    std::function<bool (const std::string& id)> isAuditioning;
    std::function<bool (const std::string& id)> isUsed;   // the pattern occurs in the sequence (green ring)
    std::function<void()> onEscape;

    FindView();

    void setIndex (const SearchIndex*);   // (re)builds the result list
    void focusField();
    void stopAllAuditionMarks()           { list.repaint(); }

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Row
    {
        enum Type { Header, Item, More } type = Item;
        SearchItem::Group group = SearchItem::Group::Bundled;
        int item = -1;        // index into the search index (Item rows)
        int total = 0;        // matches in the group (Header/More rows)
    };

    // The result rows: painting, mouse and keyboard. Scrolls inside the viewport.
    class List : public juce::Component
    {
    public:
        explicit List (FindView& o) : owner (o) { setWantsKeyboardFocus (true); }
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override   { repaint(); }
    private:
        FindView& owner;
        int hoverRow = -1;
        int pressedRow = -1;        // where the button went down (drag source / click target)
        bool pressedOnBox = false;
        bool dragging = false;
    };

    void rebuild();
    int rowAt (int y) const;
    int rowY (int row) const;
    int rowHeight (const Row&) const;
    void moveCursor (int delta, bool extendMarks);
    void toggleMark (int row);
    std::vector<std::string> targets() const;   // visible marked ids, else the cursor item
    int hiddenMarks() const;                    // marks the current filter hides
    std::vector<std::string> dragIds (int row) const;   // the marked rows if this row is marked, else this row
    void updateActionRow();
    void scrollToCursor();

    const SearchIndex* index = nullptr;
    std::vector<Row> rows;
    std::set<SearchItem::Group> expanded;
    std::set<std::string> marked;
    int cursor = -1;

    juce::TextEditor field;
    juce::Label chipLabel, markedLabel;
    juce::TextButton toScratch { "+ Ablage" }, toSequence { "+ in Ablauf" }, clearMarks { "Abwählen"_u };
    juce::Viewport viewport;
    List list;

    static constexpr int perGroup = 5;
};
