#pragma once

#include "app/FolderTree.h"
#include "app/Text.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

class Library;

// System → Ordnerstruktur: the folders of songs and own patterns as a tree. The buttons make,
// rename, delete (empty ones only) and sort folders; "Löschen" on own patterns and songs moves them
// to the trash (onDeleteEntries, after asking once). Several entries are marked as in a file manager:
// Shift+click a range, Strg+click one more or less, Strg+A all in the folder. Entries and folders move by
// dragging them onto a folder or between other entries. Every change is saved at once (Library::editFolders).
class FolderPanel : public juce::Component,
                    public juce::DragAndDropContainer
{
public:
    explicit FolderPanel (Library&);
    ~FolderPanel() override;

    void refresh();   // the library or the folders changed elsewhere

    // moves own patterns and songs to the trash; returns "" or what stayed and why
    std::function<juce::String (const std::vector<std::string>& ids)> onDeleteEntries;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;   // Strg+A: every entry of the folder

private:
    class Item;
    struct Place   // a folder (no id) or an entry in a folder
    {
        FolderTree::Area area = FolderTree::Songs;
        FolderPath path;
        std::string id;
    };

    void rebuild (const std::optional<Place>& select);
    void fill (Item& parent, const FolderNode&, const FolderPath&);
    void selectPlace (const Place&);
    void apply (FolderTree::Area, std::function<juce::String (FolderNode&)> edit, std::optional<Place> select,
                std::optional<std::pair<juce::String, juce::String>> renamedTop = {});   // see Library::editFolders
    void drop (const Place& source, const Place& folder, int folderIndex, int itemIndex);
    void askName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> done);
    void addFolder();
    void renameFolder();
    void removeFolder();
    void removeEntries();   // the marked own patterns and songs: to the trash, after asking once
    bool deletableEntry (const std::string& id) const;
    void shift (int delta);
    void sortSelected();   // the selected folder (an entry: its folder) alphabetically, after asking
    void updateButtons();
    Item* selected() const;                    // the first marked one
    std::vector<Item*> selectedItems() const;

    Library& library;
    juce::TreeView tree;
    std::unique_ptr<Item> root;
    juce::Label hint;
    juce::TextButton newButton { "Neuer Ordner" }, renameButton { "Umbenennen" }, removeButton { "Löschen"_u },
                     upButton { "Hoch" }, downButton { "Runter" }, sortButton { "Sortieren" };
    juce::ToggleButton deepToggle { "mit Unterordnern" };   // "Sortieren" goes down into all subfolders
};
