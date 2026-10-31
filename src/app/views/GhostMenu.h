#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// A see-through glimpse of the pattern menu while browsing with the keys: a short tree down to where one
// is (area, folders), below it the entries or folders there, as many as fit (all of them if possible),
// the current one highlighted and the ends of the list marked. Neon green on almost nothing, left-aligned
// at the top of its area, so it stays put from key to key. The main window fades what lies beneath while
// it shows (see onAlpha). It takes no clicks and no focus and fades out a second after the last key.
class GhostMenu : public juce::Component, private juce::Timer
{
public:
    struct Content
    {
        juce::StringArray path;   // the tree down to the list, one line per level
        juce::StringArray list;   // what lies at the end of the path: entries, or folders
        int current = -1;         // in list, -1 for none
        juce::String count;       // beside the last path line, e.g. "3 / 7"
    };

    GhostMenu();

    void show (const Content&, juce::Rectangle<int> area);   // area in the parent's coordinates: it may fill it
    std::function<void (float alpha)> onAlpha;              // 1 while it shows, falling to 0 as it fades
    void setHoldSeconds (double s)                          { holdSeconds = s; }   // System > Menü-Ordner

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void setGhostAlpha (float);

    Content content;
    int first = 0, shown = 0;   // the list lines shown: from first, as many as fit
    double shownAt = 0.0, holdSeconds = 1.0;   // then it fades out in a quarter of a second
    float alpha = 0.0f;
    static constexpr int rowH = 23, indentW = 24, padX = 20, padY = 10;
};
