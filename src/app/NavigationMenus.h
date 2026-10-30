#pragma once

#include "app/FolderTree.h"
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
    std::function<void (const std::string& id)> rename;   // right click on an entry
};

juce::PopupMenu buildPatternMenu (const Library&, int currentIndex, const PatternMenuActions&);

// A folder tree in a menu: subfolders as submenus (empty ones left out), then the entries.
// addEntry adds one entry and says whether it is the current one; the result says whether any below is.
bool addFolderMenu (juce::PopupMenu&, const FolderNode&,
                    const std::function<bool (juce::PopupMenu&, const std::string& id, const FolderPath&)>& addEntry,
                    const FolderPath& path = {});

// Items for the quick search (patterns, songs, meters). Actions refer to ids, not indices,
// because the library may be reloaded while the search is open.
std::vector<QuickSearch::Item> buildSearchItems (const Library&, const MeterLibrary&,
                                                 std::function<void (const std::string& patternId)> selectPattern,
                                                 std::function<void (int meterIndex)> selectMeter,
                                                 std::function<void()> selectMetronome);
