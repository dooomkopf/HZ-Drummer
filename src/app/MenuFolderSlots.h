#pragma once

#include "app/FolderTree.h"

#include <array>

// System → Menü-Ordner: ten sets of the folders the Baustein menu shows, called up with one click. 1 "Minimal"
// (the library and the start package's examples) and 10 "Maximal" (everything) are fixed; 2 to 9 are one's own,
// 8 "Nur Bausteine" and 9 "Nur Arrangements" come preset and can be cleared. Per area a slot keeps "all" (folders
// made later come along) or exactly the folders that were visible (later ones stay hidden).
// Kept in ~/.config/HZ-Drummer/menue-ordner.json.
using AreaNames = std::array<juce::StringArray, FolderTree::NumAreas>;   // folder names per area

struct MenuFolderSlot
{
    juce::String name;                               // empty: the slot is free
    std::array<bool, FolderTree::NumAreas> all {};   // the whole area, also folders made later ...
    AreaNames shown;                                 // ... otherwise exactly these (names compare without case)

    bool isFree() const { return name.isEmpty(); }
};

namespace menufolderslots
{
    constexpr int count = 10, minimal = 0, maximal = 9;
    using Slots = std::array<MenuFolderSlot, count>;

    bool isFixed (int index);                      // 1 and 10: not stored, renamed or cleared
    juce::File file();                             // ~/.config/HZ-Drummer/menue-ordner.json
    Slots load (const juce::File&);                // no file yet (or a broken one): 8 and 9 preset
    bool save (const Slots&, const juce::File&);   // false if it could not be written; the fixed ones are not written

    // the visible folders as a slot: an area with nothing hidden becomes "all"
    MenuFolderSlot capture (const juce::String& name, const AreaNames& folders, const AreaNames& hidden);
    AreaNames hiddenBy (const MenuFolderSlot&, const AreaNames& folders);   // which of these folders it hides
}
