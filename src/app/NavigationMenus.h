#pragma once

#include "app/QuickSearch.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class Library;
class MeterLibrary;

// Pattern menu (P): metronome, favorites, songs, own patterns grouped by song, bundled library by style.
struct PatternMenuActions
{
    std::function<void (int)> select;          // library index, -1 = metronome
    std::function<void (int)> preview;         // audition this entry while the menu stays open (toggle)
    std::function<bool (int)> isPreviewing;
    std::function<void()> toggleFavorite;
    std::function<void()> showErrors;
};

juce::PopupMenu buildPatternMenu (const Library&, int currentIndex, const PatternMenuActions&);

// Items for the quick search (patterns, songs, meters). Actions refer to ids, not indices,
// because the library may be reloaded while the search is open.
std::vector<QuickSearch::Item> buildSearchItems (const Library&, const MeterLibrary&,
                                                 std::function<void (const std::string& patternId)> selectPattern,
                                                 std::function<void (int meterIndex)> selectMeter,
                                                 std::function<void()> selectMetronome);
