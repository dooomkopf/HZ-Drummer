#pragma once

#include "engine/Pattern.h"
#include "app/SearchQuery.h"

#include <juce_core/juce_core.h>

#include <set>
#include <string>
#include <vector>

class Library;

// Search core shared by the quick search and the arranger's "Finden" column: everything in
// memory, prepared once per library reload, searched linearly (a few thousand entries stay
// well under a few milliseconds). See docs/ARRANGER.md, "Intelligente Suche".
struct SearchItem
{
    enum class Group { Songs, Own, Bundled };   // display order (as in the main window)

    std::string id;                  // pattern/song id (stable across reloads)
    Group group = Group::Bundled;
    juce::String title;              // pattern name
    juce::String origin;             // "Carcass CJQ" (sections) or the style (bundled)
    juce::String detail;             // "4/4 · 4 Takte · 142 BPM"
    juce::String haystack;           // what the words are matched against
    std::vector<juce::String> meters;   // meter labels of its bars, e.g. "7/8 (2+2+3)"
    std::vector<std::pair<int, int>> meterNumbers;   // numerator/denominator per label
    double tempo = 0.0;
    int bars = 0;
    bool favorite = false;
    int colour = -1;                 // sourceColourIndex of a pattern, -1 for songs
    int shape = 0;                   // sourceShapeIndex
};

struct SearchGroup
{
    SearchItem::Group group;
    juce::String title;              // "Arrangements", "Bausteine", "Bibliothek"
    std::vector<int> items;          // indices into SearchIndex, best first (cut to perGroup)
    int total = 0;                   // matches before the cut
};

class SearchIndex
{
public:
    void build (const Library&);

    int size() const                       { return (int) items.size(); }
    const SearchItem& item (int i) const   { return items[(size_t) i]; }
    int indexOf (const std::string& id) const;

    // Groups in display order, empty groups left out. perGroup limits each group unless it is
    // in 'expanded' (0 = no limit). An empty text lists everything.
    std::vector<SearchGroup> search (const juce::String& text, int perGroup,
                                     const std::set<SearchItem::Group>& expanded = {}) const;

    static juce::String groupTitle (SearchItem::Group);

private:
    std::vector<SearchItem> items;
};
