#include "app/FolderPanel.h"

#include "app/Library.h"
#include "app/views/InWindowDialog.h"
#include "app/views/Theme.h"

namespace
{
    const juce::Colour folderColour { 0xffe8c547 };
    const juce::Colour selectedColour { 0xff2f4f7a };

    FolderPath parentOf (FolderPath path)
    {
        path.removeRange (path.size() - 1, 1);
        return path;
    }
}

//==============================================================================
class FolderPanel::Item : public juce::TreeViewItem
{
public:
    enum class Kind { Root, Area, Folder, Entry };

    Item (FolderPanel& owner, Kind k, Place where, juce::String label, int entries)
        : kind (k), place (std::move (where)), count (entries), panel (owner), text (std::move (label)) {}

    const Kind kind;
    const Place place;   // an entry: the folder it is in
    const int count;     // entries in a folder and below

    bool mightContainSubItems() override   { return kind != Kind::Entry; }
    int getItemHeight() const override     { return 28; }
    bool canBeSelected() const override    { return kind != Kind::Root; }

    juce::String getUniqueName() const override
    {
        if (kind == Kind::Entry)
            return "#" + juce::String (place.id);
        if (kind == Kind::Folder)
            return place.path[place.path.size() - 1].toLowerCase();
        const char* const areaNames[] = { "songs", "patterns", "library" };
        return kind == Kind::Area ? juce::String (areaNames[place.area]) : juce::String ("root");
    }

    void paintItem (juce::Graphics& g, int width, int height) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).reduced (4, 0);
        if (kind != Kind::Entry)
        {
            const auto icon = r.removeFromLeft (22).toFloat().withSizeKeepingCentre (18.0f, 14.0f);
            juce::Path shape;
            shape.addRoundedRectangle (icon.getX(), icon.getY() + 3.0f, icon.getWidth(), icon.getHeight() - 3.0f, 2.0f);
            shape.addRoundedRectangle (icon.getX(), icon.getY(), icon.getWidth() * 0.45f, 5.0f, 1.5f);
            g.setColour (kind == Kind::Area ? theme::progress : folderColour);
            g.fillPath (shape);
            r.removeFromLeft (6);

            g.setColour (theme::textDim);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (count > 0 ? juce::String (count) : juce::String ("leer"), r.removeFromRight (50), juce::Justification::centredRight);
        }
        g.setColour (theme::textMain);
        g.setFont (juce::FontOptions (kind == Kind::Area ? 17.0f : 16.0f, kind == Kind::Entry ? juce::Font::plain : juce::Font::bold));
        g.drawFittedText (text, r, juce::Justification::centredLeft, 1);
    }

    void itemSelectionChanged (bool) override   { panel.updateButtons(); }

    // dragging: folders and entries; the description says what moves (the item may be gone at the drop)
    juce::var getDragSourceDescription() override
    {
        if (kind != Kind::Folder && kind != Kind::Entry)
            return {};
        juce::Array<juce::var> path;
        for (auto& name : place.path)
            path.add (name);
        auto* o = new juce::DynamicObject();
        o->setProperty ("area", (int) place.area);
        o->setProperty ("path", path);
        o->setProperty ("id", juce::String (place.id));
        return juce::var (o);
    }

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override
    {
        const auto source = placeOf (details.description);
        if (! source || (kind != Kind::Area && kind != Kind::Folder) || source->area != place.area)
            return false;
        return ! source->id.empty() || ! folders::startsWith (place.path, source->path);   // a folder not into itself
    }

    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details, int insertIndex) override
    {
        const auto source = placeOf (details.description);
        if (! source)
            return;
        int folderCount = 0;
        for (int i = 0; i < getNumSubItems(); ++i)
            if (static_cast<Item*> (getSubItem (i))->kind == Kind::Folder)
                ++folderCount;
        // between the sub-items of an open folder: right there (its folders come before its entries);
        // onto a closed or empty one: at the end
        const bool between = isOpen() && getNumSubItems() > 0;
        panel.drop (*source, place, between ? juce::jmin (insertIndex, folderCount) : -1,
                    between ? juce::jmax (0, insertIndex - folderCount) : -1);
    }

private:
    static std::optional<Place> placeOf (const juce::var& description)
    {
        const auto* path = description["path"].getArray();
        if (path == nullptr)
            return {};
        Place p;
        p.area = (FolderTree::Area) juce::jlimit (0, (int) FolderTree::NumAreas - 1, (int) description["area"]);
        for (auto& name : *path)
            p.path.add (name.toString());
        p.id = description["id"].toString().toStdString();
        return p;
    }

    FolderPanel& panel;
    juce::String text;
};

//==============================================================================
FolderPanel::FolderPanel (Library& lib) : library (lib)
{
    addAndMakeVisible (hint);
    hint.setText ("Ziehen: Einträge und Ordner verschieben   ·   Hoch/Runter: Reihenfolge   ·   löschen lassen sich nur leere Ordner"_u,
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, theme::textDim);
    hint.setFont (juce::FontOptions (14.0f));

    addAndMakeVisible (tree);
    tree.setRootItemVisible (false);
    tree.setIndentSize (22);
    tree.setColour (juce::TreeView::backgroundColourId, theme::background);
    tree.setColour (juce::TreeView::selectedItemBackgroundColourId, selectedColour);
    tree.setColour (juce::TreeView::linesColourId, theme::textDim.withAlpha (0.4f));
    tree.setColour (juce::TreeView::dragAndDropIndicatorColourId, theme::progress);

    for (auto* b : { &newButton, &renameButton, &removeButton, &upButton, &downButton })
        addAndMakeVisible (*b);
    newButton.setTooltip ("Neuer Ordner im gewählten Ordner");
    newButton.onClick = [this] { addFolder(); };
    renameButton.onClick = [this] { renameFolder(); };
    removeButton.onClick = [this] { removeFolder(); };
    upButton.onClick = [this] { shift (-1); };
    downButton.onClick = [this] { shift (1); };

    rebuild ({});
}

FolderPanel::~FolderPanel()
{
    tree.setRootItem (nullptr);
}

void FolderPanel::refresh()
{
    rebuild ({});
}

void FolderPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void FolderPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    hint.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    auto buttons = r.removeFromBottom (36);
    newButton.setBounds (buttons.removeFromLeft (150));    buttons.removeFromLeft (8);
    renameButton.setBounds (buttons.removeFromLeft (130)); buttons.removeFromLeft (8);
    removeButton.setBounds (buttons.removeFromLeft (110)); buttons.removeFromLeft (24);
    upButton.setBounds (buttons.removeFromLeft (90));      buttons.removeFromLeft (8);
    downButton.setBounds (buttons.removeFromLeft (90));
    r.removeFromBottom (10);
    tree.setBounds (r);
}

//==============================================================================
void FolderPanel::rebuild (const std::optional<Place>& select)
{
    const auto state = tree.getOpennessState (true);
    tree.setRootItem (nullptr);
    root = std::make_unique<Item> (*this, Item::Kind::Root, Place(), juce::String(), 0);
    for (auto area : { FolderTree::Songs, FolderTree::Patterns, FolderTree::Bundled })
    {
        const auto node = library.folders (area);
        auto* item = new Item (*this, Item::Kind::Area, Place { area, {}, {} },
                               area == FolderTree::Songs ? "Songs (Abläufe)"_u
                               : area == FolderTree::Patterns ? juce::String ("Eigene Patterns (Bausteine)") : juce::String ("Bibliothek (mitgeliefert)"),
                               folders::countItems (node));
        root->addSubItem (item);
        fill (*item, node, {});
        item->setOpen (area != FolderTree::Bundled);   // the long library starts closed
    }
    tree.setRootItem (root.get());
    root->setOpen (true);
    if (state != nullptr)
        tree.restoreOpennessState (*state, true);
    if (select)
        selectPlace (*select);
    updateButtons();
}

void FolderPanel::fill (Item& parent, const FolderNode& node, const FolderPath& path)
{
    for (auto& f : node.folders)
    {
        auto sub = path;
        sub.add (f.name);
        auto* item = new Item (*this, Item::Kind::Folder, Place { parent.place.area, sub, {} }, f.name, folders::countItems (f));
        parent.addSubItem (item);
        fill (*item, f, sub);
    }
    for (auto& id : node.items)
        if (const int i = library.indexOf (id); i >= 0)
            parent.addSubItem (new Item (*this, Item::Kind::Entry, Place { parent.place.area, path, id },
                                         folders::shortName (juce::String (library.get (i).name), path), 0));
}

void FolderPanel::selectPlace (const Place& where)
{
    auto findChild = [] (Item* parent, auto matches) -> Item*
    {
        for (int i = 0; parent != nullptr && i < parent->getNumSubItems(); ++i)
            if (auto* sub = static_cast<Item*> (parent->getSubItem (i)); matches (*sub))
                return sub;
        return nullptr;
    };
    auto* item = findChild (root.get(), [&] (const Item& a) { return a.place.area == where.area; });
    for (int depth = 0; depth < where.path.size(); ++depth)
        item = findChild (item, [&] (const Item& f)
        {
            return f.kind == Item::Kind::Folder && f.place.path[depth].equalsIgnoreCase (where.path[depth]);
        });
    if (! where.id.empty())
        item = findChild (item, [&] (const Item& e) { return e.kind == Item::Kind::Entry && e.place.id == where.id; });
    if (item == nullptr)
        return;

    for (auto* p = item->getParentItem(); p != nullptr; p = p->getParentItem())
        p->setOpen (true);
    item->setSelected (true, true);
    tree.scrollToKeepItemVisible (item);
}

FolderPanel::Item* FolderPanel::selected() const
{
    return dynamic_cast<Item*> (tree.getSelectedItem (0));
}

void FolderPanel::updateButtons()
{
    const auto* item = selected();
    const auto kind = item != nullptr ? item->kind : Item::Kind::Root;
    newButton.setEnabled (item != nullptr);
    renameButton.setEnabled (kind == Item::Kind::Folder);
    removeButton.setEnabled (kind == Item::Kind::Folder && item->count == 0);

    // up/down among the siblings of the same kind (folders and entries are sorted apart)
    int index = -1, siblings = 0;
    if (kind == Item::Kind::Folder || kind == Item::Kind::Entry)
        if (auto* parent = item->getParentItem())
            for (int i = 0; i < parent->getNumSubItems(); ++i)
                if (auto* sub = static_cast<Item*> (parent->getSubItem (i)); sub->kind == kind)
                {
                    if (sub == item)
                        index = siblings;
                    ++siblings;
                }
    upButton.setEnabled (index > 0);
    downButton.setEnabled (index >= 0 && index < siblings - 1);
}

//==============================================================================
void FolderPanel::apply (FolderTree::Area area, std::function<juce::String (FolderNode&)> edit, std::optional<Place> select)
{
    if (const auto error = library.editFolders (area, edit); error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Ordnerstruktur", error, "OK", this);
        return;
    }
    rebuild (select);
}

void FolderPanel::drop (const Place& source, const Place& folder, int folderIndex, int itemIndex)
{
    // rebuilding the tree deletes the item that is still handling the drop: right after it instead
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FolderPanel> (this), source, folder, folderIndex, itemIndex]
    {
        if (safe == nullptr)
            return;
        if (source.id.empty())
        {
            auto moved = folder.path;
            moved.add (source.path[source.path.size() - 1]);
            safe->apply (folder.area, [=] (FolderNode& r) { return folders::moveFolder (r, source.path, folder.path, folderIndex); },
                         Place { folder.area, moved, {} });
        }
        else
        {
            safe->apply (folder.area, [=] (FolderNode& r) { return folders::moveItem (r, source.id, folder.path, itemIndex); },
                         Place { folder.area, folder.path, source.id });
        }
    });
}

void FolderPanel::askName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> done)
{
    auto* w = new InWindowDialog (title, "Name des Ordners:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", initial, "Name");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<FolderPanel> (this), w, done] (int r)
    {
        const auto name = w->getTextEditorContents ("name").trim();
        if (safe != nullptr && r == 1 && name.isNotEmpty())
            done (name);
    }), true);
}

void FolderPanel::addFolder()
{
    const auto* item = selected();
    if (item == nullptr)
        return;
    const auto parent = item->place;   // an entry: next to it
    askName ("Neuer Ordner", {}, [this, parent] (const juce::String& name)
    {
        auto path = parent.path;
        path.add (name);
        apply (parent.area, [parent, name] (FolderNode& r) { return folders::addFolder (r, parent.path, name); },
               Place { parent.area, path, {} });
    });
}

void FolderPanel::renameFolder()
{
    const auto* item = selected();
    if (item == nullptr || item->kind != Item::Kind::Folder)
        return;
    const auto place = item->place;
    askName ("Ordner umbenennen", place.path[place.path.size() - 1], [this, place] (const juce::String& name)
    {
        auto renamed = place.path;
        renamed.set (renamed.size() - 1, name);
        apply (place.area, [place, name] (FolderNode& r) { return folders::renameFolder (r, place.path, name); },
               Place { place.area, renamed, {} });
    });
}

void FolderPanel::removeFolder()
{
    const auto* item = selected();
    if (item == nullptr || item->kind != Item::Kind::Folder)
        return;
    const auto place = item->place;
    apply (place.area, [place] (FolderNode& r) { return folders::removeFolder (r, place.path); },
           Place { place.area, parentOf (place.path), {} });
}

void FolderPanel::shift (int delta)
{
    const auto* item = selected();
    if (item == nullptr || (item->kind != Item::Kind::Folder && item->kind != Item::Kind::Entry))
        return;
    const auto place = item->place;
    const bool folder = item->kind == Item::Kind::Folder;
    apply (place.area, [place, delta, folder] (FolderNode& r)
    {
        const bool moved = folder ? folders::shiftFolder (r, place.path, delta) : folders::shiftItem (r, place.id, delta);
        return moved ? juce::String() : juce::String ("Weiter geht es nicht.");
    }, place);
}
