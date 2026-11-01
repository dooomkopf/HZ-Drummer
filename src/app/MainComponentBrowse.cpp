// MainComponent, part 3: browsing with the arrow keys (patterns, folders, favourites, meters), the ghost menu that
// shows where one is, and the history of Alt+left/right. Moved out of MainComponent.cpp as it was.

#include "app/MainComponent.h"
#include "app/Text.h"
#include "app/views/NeonGlow.h"
#include "app/views/Theme.h"
#include "engine/MeterCatalog.h"

#include <utility>

void MainComponent::stepPattern (int dir, bool favoritesOnly)
{
    const int n = library.size();
    if (n == 0)
        return;

    int i = currentPattern < 0 ? (dir > 0 ? -1 : 0) : currentPattern;
    for (int tries = 0; tries < n; ++tries)
    {
        i = ((i + dir) % n + n) % n;
        if (! favoritesOnly || library.isFavorite (library.get (i).id))
        {
            selectPattern (i);
            return;
        }
    }
}

void MainComponent::stepCurrent (int dir, bool favoritesOnly)
{
    if (patternMode())
        stepPattern (dir, favoritesOnly);
    else
        selectMeter (favoritesOnly ? meters.stepFavorite (currentMeter, dir) : meters.step (currentMeter, dir));
}

void MainComponent::stepStyle (int dir)
{
    const int n = library.size();
    if (n == 0)
        return;

    // entries are sorted by style: jump to the first entry of the next/previous style
    auto styleOf = [this] (int i) { const auto& t = library.get (i).tags; return t.empty() ? std::string() : t.front(); };
    if (! patternMode())
    {
        selectPattern (dir > 0 ? 0 : n - 1);
        if (dir > 0)
            return;
    }

    const int i = currentPattern;
    const auto start = styleOf (i);
    if (dir > 0)
    {
        for (int k = 1; k < n; ++k)
            if (const int j = (i + k) % n; styleOf (j) != start)
                return selectPattern (j);
        return;
    }

    // backwards: to the start of the current style; if already there, to the start of the previous one
    int first = i;
    while (first > 0 && styleOf (first - 1) == start)
        --first;
    if (first == i)
    {
        first = (i - 1 + n) % n;
        const auto prev = styleOf (first);
        while (first > 0 && styleOf (first - 1) == prev)
            --first;
    }
    selectPattern (first);
}

//==============================================================================
// Browsing with the keys, as the pattern menu is ordered

std::vector<BrowseFolder> MainComponent::browseOrder() const
{
    std::vector<BrowseArea> areas;
    for (auto area : { FolderTree::Songs, FolderTree::Patterns, FolderTree::Bundled })   // as buildPatternMenu
        areas.push_back ({ library.folders (area), [this, area] (const juce::String& folder) { return library.folderHidden (area, folder); } });
    return browseFolders (areas);
}

namespace
{
    const char* areaTitle (int area) { return area == 0 ? "Arrangements" : area == 1 ? "Bausteine" : "Bibliothek"; }

    // the tree down to a folder: its folders from the top; the area's own name only where there is no folder
    juce::StringArray treeOf (int area, const FolderPath& path)
    {
        return path.isEmpty() ? juce::StringArray (areaTitle (area)) : path;
    }

    juce::String counter (int at, int total) { return juce::String (at + 1) + " / " + juce::String (total); }
}

void MainComponent::showGhost (const GhostMenu::Content& content)
{
    if (! ghostEnabled)
        return;
    // left-aligned just below the title, as far down as the window goes; what lies beneath fades (see the constructor)
    const int top = titleLabel.getBottom() + 4;
    ghostMenu.show (content, { titleLabel.getX(), top, getWidth() - titleLabel.getX() - 24, getHeight() - top });
}

void MainComponent::browseInFolder (int dir)
{
    const auto order = browseOrder();
    const auto id = library.get (currentPattern).id;
    const int f = browseFolderOf (order, id);
    if (f < 0)
        return;
    const auto& folder = order[(size_t) f];
    int at = (int) (std::find (folder.ids.begin(), folder.ids.end(), id) - folder.ids.begin());
    if (const int to = at + dir; dir != 0 && to >= 0 && to < (int) folder.ids.size())   // at an end: it stops, the ghost shows why
        if (const int i = library.indexOf (folder.ids[(size_t) to]); i >= 0)
        {
            selectPattern (i);
            at = to;
        }

    GhostMenu::Content c;
    c.path = treeOf (folder.area, folder.path);
    for (const auto& entry : folder.ids)
        if (const int i = library.indexOf (entry); i >= 0)
            c.list.add (library.entryLabel (i, folder.path));
    c.current = at;
    c.count = counter (at, (int) folder.ids.size());
    showGhost (c);
}

void MainComponent::browseFolder (int dir)
{
    const auto order = browseOrder();
    const int from = patternMode() ? browseFolderOf (order, library.get (currentPattern).id) : -1;
    const int to = nextBrowseFolder (order, from >= 0 ? from : (dir > 0 ? -1 : (int) order.size()), dir);
    if (to >= 0)
        if (const int i = library.indexOf (order[(size_t) to].ids.front()); i >= 0)
            selectPattern (i);
    const int shownFolder = to >= 0 ? to : from;
    if (shownFolder < 0)
        return;

    // the tree down to the parent folder, below it the folders beside the one now in hand (visible ones,
    // with the number of entries in and below them); entries in no folder appear as "ohne Ordner"
    const auto& folder = order[(size_t) shownFolder];
    auto parent = folder.path;
    if (! parent.isEmpty())
        parent.remove (parent.size() - 1);
    const int depth = parent.size();
    GhostMenu::Content c;
    c.path = treeOf (folder.area, parent);
    std::vector<int> counts;
    auto add = [&] (const juce::String& name, int entries)
    {
        int k = c.list.indexOf (name);
        if (k < 0)
        {
            c.list.add (name);
            counts.push_back (0);
            k = c.list.size() - 1;
        }
        counts[(size_t) k] += entries;
    };
    const auto noFolder = juce::String ("ohne Ordner");
    for (const auto& stop : order)
        if (stop.area == folder.area && ! stop.hidden)
        {
            if (stop.path.size() > depth && folders::startsWith (stop.path, parent))
                add (stop.path[depth], (int) stop.ids.size());
            else if (depth == 0 && stop.path.isEmpty())
                add (noFolder, (int) stop.ids.size());
        }
    c.current = c.list.indexOf (folder.path.size() > depth ? folder.path[depth] : noFolder);
    for (int k = 0; k < c.list.size(); ++k)
        c.list.set (k, c.list[k] + "  (" + juce::String (counts[(size_t) k]) + ")");
    int visible = 0, index = -1;
    for (int k = 0; k < (int) order.size(); ++k)
        if (! order[(size_t) k].hidden)
        {
            if (k == shownFolder)
                index = visible;
            ++visible;
        }
    c.count = index >= 0 ? "Ordner " + counter (index, visible) : juce::String();
    showGhost (c);
}

void MainComponent::browseFavorites (int dir)
{
    // in the order of the menu's Favoriten section
    std::vector<int> favorites;
    for (int i = 0; i < library.size(); ++i)
        if (library.isFavorite (library.get (i).id))
            favorites.push_back (i);
    if (favorites.empty())
        return;
    const auto it = std::find (favorites.begin(), favorites.end(), currentPattern);
    int at = it != favorites.end() ? (int) (it - favorites.begin()) : -1;
    const int to = at >= 0 ? at + dir : (dir > 0 ? 0 : (int) favorites.size() - 1);
    if (to >= 0 && to < (int) favorites.size())
    {
        selectPattern (favorites[(size_t) to]);
        at = to;
    }

    GhostMenu::Content c;
    c.path.add ("Favoriten");
    for (int i : favorites)
        c.list.add (juce::String (library.get (i).name));
    c.current = at;
    c.count = at >= 0 ? counter (at, (int) favorites.size()) : juce::String();
    showGhost (c);
}

void MainComponent::browseMeters (int dir, bool favoritesOnly)
{
    const int next = dir == 0 ? currentMeter : favoritesOnly ? meters.stepFavorite (currentMeter, dir) : meters.step (currentMeter, dir);
    const bool wrapped = dir == 0 || next < 0 || (dir > 0 ? next <= currentMeter : next >= currentMeter);
    if (! wrapped)   // at an end: it stops
        selectMeter (next);

    GhostMenu::Content c;
    c.path.add (favoritesOnly ? "Taktart-Favoriten" : "Taktarten");
    for (int i = 0; i < meters.size(); ++i)
        if (! favoritesOnly || meters.isFavorite (i))
        {
            if (i == currentMeter)
                c.current = c.list.size();
            c.list.add (meters.label (i));
        }
    c.count = c.current >= 0 ? counter (c.current, c.list.size()) : juce::String();
    showGhost (c);
}

//==============================================================================
// Verlauf: back and forward through the entries chosen so far

void MainComponent::recordHistory()
{
    if (historyMoving)
        return;
    const HistoryEntry entry { patternMode() ? library.get (currentPattern).id : std::string(), patternMode() ? -1 : currentMeter };
    if (historyAt >= 0 && history[(size_t) historyAt].patternId == entry.patternId && history[(size_t) historyAt].meter == entry.meter)
        return;
    history.resize ((size_t) (historyAt + 1));   // a new choice drops what lay ahead, as in a browser
    history.push_back (entry);
    if (history.size() > 50)
        history.erase (history.begin());
    historyAt = (int) history.size() - 1;
}

void MainComponent::moveHistory (int dir)
{
    for (int i = historyAt + dir; i >= 0 && i < (int) history.size(); i += dir)
    {
        const auto& entry = history[(size_t) i];
        const int index = entry.patternId.empty() ? -1 : library.indexOf (entry.patternId);
        if (! entry.patternId.empty() && index < 0)
            continue;   // deleted meanwhile: passed over
        historyAt = i;
        historyMoving = true;
        if (index >= 0)
            selectPattern (index);
        else
            selectMeter (entry.meter);
        historyMoving = false;
        if (patternMode())   // where it is, as when browsing
            browseInFolder (0);
        else
            browseMeters (0, false);
        return;
    }
}
