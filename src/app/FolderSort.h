#pragma once

#include "app/FolderTree.h"

#include <functional>
#include <string>

// System → Ordnerstruktur, "Sortieren": the subfolders of a folder and its entries, each in alphabetical order
// (folders always stand before entries). Without case, numbers by value ("Teil 2" before "Teil 10"), ä ö ü like
// a o u, signs in front ignored ("...And Justice for All" under A). A one-time order: moving by hand goes on as before.
// System → Menü-Ordner lists the top-level folders in the same order.
namespace folders
{
    bool sortsBefore (const juce::String& a, const juce::String& b);

    // label: what an entry is called in its folder (FolderPanel shows the short name of the pattern or song)
    juce::String sortFolder (FolderNode& root, const FolderPath&, bool withSubfolders,
                             const std::function<juce::String (const std::string& id, const FolderPath& folder)>& label);
}
