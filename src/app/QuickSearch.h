#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "app/SearchQuery.h"

#include <functional>
#include <vector>

// Fuzzy search: substring ("arc" -> "Carcass") or letter sequence ("cjq" -> "Corporal Jigsore Quandary").
// Several words must all match. Returns 0 = no match, otherwise higher is better.
int fuzzyScore (const juce::String& query, const juce::String& text, bool allowInitials = true);

// Quick search over everything (Ctrl+K or /): text field + result list on top of the main window.
// The list scrolls: arrows, PageUp/PageDown, Ctrl+Home/End and the mouse wheel reach every result.
class QuickSearch : public juce::Component,
                    private juce::KeyListener
{
public:
    struct Item
    {
        juce::String kind;       // "Arrangement", "Pattern", "Bibliothek", "Metronom"
        juce::String title, detail;
        juce::String haystack;   // searched text (name, tags, meter, tempo ...)
        bool favorite = false;
        double tempo = 0.0;
        std::vector<std::pair<int, int>> meters;
        int colour = -1, shape = 0;
        std::function<void()> action;
        std::function<void()> preview;        // the play button behind the kind (mouse over the row): plays it, again: stops
        std::function<bool()> isPreviewing;   // shown as a stop mark
    };

    QuickSearch();

    void open (std::vector<Item> items);
    void close (bool chosen = false);   // dismissed (Esc, click outside): onDismiss, e.g. to end a preview

    std::function<void()> onDismiss;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;   // the scroll bar on the right can be dragged
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    void update();
    void choose (int index);
    juce::Rectangle<int> panelBounds() const;
    int rowAt (juce::Point<int>) const;   // result index under the point, or -1
    juce::Rectangle<int> playButton (int row) const;   // behind the kind column of a shown row
    juce::Rectangle<int> scrollTrack() const;          // the thin bar on the right of the list
    void scrollToY (int y);                            // the thumb's centre to y (click or drag on the bar)
    int visibleRows() const;              // rows that fit, at most maxRows
    void select (int index);              // clamps and scrolls the selection into view

    static constexpr int maxRows = 12;
    static constexpr int rowH = 48;

    juce::TextEditor field;
    SearchQuery query;
    std::vector<Item> all;
    std::vector<int> results;   // indices into 'all', best match first
    int selected = 0;
    int first = 0;              // first result shown (scroll position)
    int hoverRow = -1;          // the row under the mouse (its play button shows)
    bool draggingScroll = false;
};
