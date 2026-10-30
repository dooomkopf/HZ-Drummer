#pragma once

#include "app/FolderTree.h"
#include "app/Text.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

class Library;

// System → Ordnerstruktur: the folders of songs and own patterns as a tree. The buttons make,
// rename, delete (empty ones only) and sort folders; "Löschen" on an own pattern or a song moves it
// to the trash (onDeleteEntry, after asking). Entries and folders move by dragging them onto a
// folder or between other entries. Every change is saved at once (Library::editFolders).
class FolderPanel : public juce::Component,
                    public juce::DragAndDropContainer
{
public:
    explicit FolderPanel (Library&);
    ~FolderPanel() override;

    void refresh();   // the library or the folders changed elsewhere

    // moves an own pattern or a song to the trash; returns "" or why it cannot
    std::function<juce::String (const std::string& id)> onDeleteEntry;

    void paint (juce::Graphics&) override;
    void resized() override;

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
    void apply (FolderTree::Area, std::function<juce::String (FolderNode&)> edit, std::optional<Place> select);
    void drop (const Place& source, const Place& folder, int folderIndex, int itemIndex);
    void askName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> done);
    void addFolder();
    void renameFolder();
    void removeFolder();
    void removeEntry();   // an own pattern or a song: to the trash, after asking
    bool deletableEntry (const std::string& id) const;
    void shift (int delta);
    void updateButtons();
    Item* selected() const;

    Library& library;
    juce::TreeView tree;
    std::unique_ptr<Item> root;
    juce::Label hint;
    juce::TextButton newButton { "Neuer Ordner" }, renameButton { "Umbenennen" }, removeButton { "Löschen"_u },
                     upButton { "Hoch" }, downButton { "Runter" };
};
