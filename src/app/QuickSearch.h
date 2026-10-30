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
    };

    QuickSearch();

    void open (std::vector<Item> items);
    void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    void update();
    void choose (int index);
    juce::Rectangle<int> panelBounds() const;
    int rowAt (juce::Point<int>) const;   // result index under the point, or -1
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
};
