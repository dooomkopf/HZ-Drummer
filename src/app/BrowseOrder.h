#pragma once

#include "app/FolderTree.h"

#include <functional>
#include <string>
#include <vector>

// Browsing with the arrow keys follows the pattern menu: the same order (arrangements, own patterns, the
// library) and the same folders. A stop is a folder that holds entries itself; a folder that only sorts
// (subfolders, no entries of its own) leads on to its first subfolder, empty folders are left out.
// Folders below a top-level folder switched off under System > Menü-Ordner are marked hidden.
struct BrowseFolder
{
    int area = 0;                    // index into the areas given to browseFolders
    FolderPath path;                 // folder names from the top; empty: the area's own top level
    std::vector<std::string> ids;    // its own entries, in menu order
    bool hidden = false;             // below a top-level folder switched off in the menu
};

struct BrowseArea
{
    FolderNode root;                                               // as Library::folders gives it
    std::function<bool (const juce::String& topFolder)> isHidden;  // Menü-Ordner
};

// All stops depth-first in menu order: in each folder first its subfolders, then its own entries,
// as addFolderMenu builds the menu.
std::vector<BrowseFolder> browseFolders (const std::vector<BrowseArea>& areasInMenuOrder);

int browseFolderOf (const std::vector<BrowseFolder>&, const std::string& id);   // -1: in none

// The next visible stop from 'from' in direction dir (+1 / -1), -1 if there is none: no wrapping.
// from = -1 with dir +1 gives the first one, from = size() with dir -1 the last one.
int nextBrowseFolder (const std::vector<BrowseFolder>&, int from, int dir);
