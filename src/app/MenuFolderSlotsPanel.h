#pragma once

#include "app/MenuFolderSlots.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class Library;

// The ten slots of System → Menü-Ordner (menufolderslots), two columns of five. A click calls a slot up; a click on
// a free one stores the ticks as they are and asks for a name. Double click renames, right click stores over it or
// clears it, × clears it. The slot that matches the ticks has a yellow frame. Minimal (1) and Maximal (10) are fixed.
class MenuFolderSlotsPanel : public juce::Component
{
public:
    std::function<void()> onApplied;   // a slot was called up: the ticks below show it

    explicit MenuFolderSlotsPanel (Library&);
    ~MenuFolderSlotsPanel() override;

    void update();   // the ticks changed: which slot matches them now
    void resized() override;

private:
    class Cell;

    void current (AreaNames& folders, AreaNames& hidden) const;   // the top-level folders and the hidden ones
    void apply (int index);
    void store (int index, bool askName);
    void clear (int index);
    void rename (int index, const juce::String& name);
    void showMenu (int index);
    void save();

    Library& library;
    menufolderslots::Slots slots;
    juce::OwnedArray<Cell> cells;
};
