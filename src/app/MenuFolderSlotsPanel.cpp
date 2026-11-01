#include "app/MenuFolderSlotsPanel.h"

#include "app/Library.h"
#include "app/Text.h"
#include "app/views/Theme.h"

namespace
{
    const juce::Colour activeYellow { 0xffd8c26e };   // the muted yellow of the folders

    bool sameNames (juce::StringArray a, juce::StringArray b)
    {
        a.sortNatural();
        b.sortNatural();
        if (a.size() != b.size())
            return false;
        for (int i = 0; i < a.size(); ++i)
            if (! a[i].equalsIgnoreCase (b[i]))
                return false;
        return true;
    }
}

// One slot: its number, its name (or "leer"), × to clear an own one. Built like the trainer's slots.
class MenuFolderSlotsPanel::Cell : public juce::Component
{
public:
    std::function<void()> onClick, onMenu, onClear;
    std::function<void (const juce::String&)> onRenamed;

    explicit Cell (int i) : index (i), fixed (menufolderslots::isFixed (i))
    {
        addAndMakeVisible (number);
        number.setText (juce::String (index + 1) + ".", juce::dontSendNotification);
        number.setBorderSize ({ 1, 2, 1, 2 });   // "10." must not be cut
        number.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (name);
        name.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        name.onUp = [this] (const juce::MouseEvent& e)
        {
            if (e.mods.isPopupMenu())
            {
                ++clickToken;   // a click still waiting is void
                if (! fixed && onMenu) onMenu();
                return;
            }
            if (e.mouseWasDraggedSinceMouseDown())
                return;
            if (e.getNumberOfClicks() > 1)
            {
                ++clickToken;   // a double click renames (the label does that), it does not call up
                return;
            }
            if (fixed || ! used)
            {
                if (onClick) onClick();   // nothing to rename: at once
                return;
            }
            // an own slot: called up once it is clear that no double click follows
            const int token = ++clickToken;
            juce::Timer::callAfterDelay (juce::MouseEvent::getDoubleClickTimeout() + 20,
                                         [safe = juce::Component::SafePointer<Cell> (this), token]
            {
                if (safe != nullptr && safe->clickToken == token && safe->used && safe->onClick)   // cleared meanwhile: nothing
                    safe->onClick();
            });
        };
        name.onTextChange = [this] { if (onRenamed) onRenamed (name.getText()); };
        addChildComponent (clearButton);
        clearButton.setTooltip ("Platz leeren");
        clearButton.setWantsKeyboardFocus (false);
        clearButton.onClick = [this] { ++clickToken; if (onClear) onClear(); };   // a click still waiting must not store again
    }

    void show (const MenuFolderSlot& s, bool isActive)
    {
        used = ! s.isFree();
        active = isActive && used;
        name.setText (used ? s.name : juce::String ("leer"), juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, used ? theme::textMain : theme::textDim);
        name.setColour (juce::Label::outlineColourId, active ? activeYellow : juce::Colour (0xff3a3a42));
        name.setColour (juce::Label::backgroundColourId, active ? juce::Colour (0xff35321f) : juce::Colour (0xff26262c));
        name.setFont (juce::FontOptions (15.0f, active ? juce::Font::bold : juce::Font::plain));
        name.setEditable (false, used && ! fixed, false);   // double click renames an own slot
        name.setTooltip (index == menufolderslots::minimal ? "Klick: nur die Bibliothek und die Beispiel-Arrangements zeigen"_u
                         : index == menufolderslots::maximal ? "Klick: alle Ordner zeigen, auch neue"_u
                         : ! used ? "Klick: die jetzigen Haken hier ablegen"_u
                                  : "Klick: abrufen · Doppelklick: umbenennen · Rechtsklick: überschreiben oder leeren"_u);
        clearButton.setVisible (used && ! fixed);
        repaint();
    }

    void startRename()
    {
        name.showEditor();
        if (auto* editor = name.getCurrentTextEditor())
            editor->selectAll();
    }

    void paint (juce::Graphics& g) override
    {
        if (active)   // the set in effect: a yellow frame
        {
            g.setColour (activeYellow);
            g.drawRoundedRectangle (name.getBounds().toFloat().expanded (1.5f), 3.0f, 2.0f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        number.setBounds (r.removeFromLeft (30));    // room for "10."
        clearButton.setBounds (r.removeFromRight (34));
        r.removeFromRight (6);
        name.setBounds (r);
    }

private:
    struct NameLabel : juce::Label
    {
        std::function<void (const juce::MouseEvent&)> onUp;
        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Label::mouseUp (e);
            if (onUp) onUp (e);
        }
    };
    const int index;
    const bool fixed;
    juce::Label number;
    NameLabel name;
    juce::TextButton clearButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };
    bool used = false, active = false;
    int clickToken = 0;   // a click waits for a possible double click; a newer click or the double click voids it
};

//==============================================================================
MenuFolderSlotsPanel::MenuFolderSlotsPanel (Library& lib) : library (lib), slots (menufolderslots::load (menufolderslots::file()))
{
    for (int i = 0; i < menufolderslots::count; ++i)
    {
        auto* cell = cells.add (new Cell (i));
        addAndMakeVisible (cell);
        cell->onClick = [this, i] { if (slots[(size_t) i].isFree()) store (i, true); else apply (i); };
        cell->onMenu = [this, i] { showMenu (i); };
        cell->onClear = [this, i] { clear (i); };
        cell->onRenamed = [this, i] (const juce::String& text) { rename (i, text); };
    }
    update();
}

MenuFolderSlotsPanel::~MenuFolderSlotsPanel() = default;

void MenuFolderSlotsPanel::resized()
{
    // two columns of five: 1 to 5 on the left, 6 to 10 on the right
    const int columnWidth = (getWidth() - 16) / 2, rowHeight = getHeight() / 5;
    for (int i = 0; i < cells.size(); ++i)
        cells[i]->setBounds ((i / 5) * (columnWidth + 16), (i % 5) * rowHeight, columnWidth, rowHeight);
}

void MenuFolderSlotsPanel::current (AreaNames& folders, AreaNames& hidden) const
{
    for (int a = 0; a < FolderTree::NumAreas; ++a)
    {
        const auto area = (FolderTree::Area) a;
        for (const auto& name : library.topLevelFolders (area))   // of both views: a slot holds the whole state
        {
            folders[(size_t) a].add (name);
            if (library.folderHidden (area, name))
                hidden[(size_t) a].add (name);
        }
    }
}

void MenuFolderSlotsPanel::update()
{
    // read anew: a rename in Ordnerstruktur may have changed the file (every change here is saved at once)
    slots = menufolderslots::load (menufolderslots::file());
    AreaNames folders, hidden;
    current (folders, hidden);
    for (int i = 0; i < cells.size(); ++i)
    {
        const auto& s = slots[(size_t) i];
        bool matches = ! s.isFree();
        const auto hide = menufolderslots::hiddenBy (s, folders);
        for (int a = 0; matches && a < FolderTree::NumAreas; ++a)
            matches = sameNames (hide[(size_t) a], hidden[(size_t) a]);
        cells[i]->show (s, matches);
    }
}

void MenuFolderSlotsPanel::apply (int index)
{
    AreaNames folders, hidden;
    current (folders, hidden);
    if (const auto error = library.setHiddenFolders (menufolderslots::hiddenBy (slots[(size_t) index], folders)); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Nicht gespeichert", error, "OK", this);
    update();
    if (onApplied)
        onApplied();
}

void MenuFolderSlotsPanel::store (int index, bool askName)
{
    if (menufolderslots::isFixed (index))
        return;
    AreaNames folders, hidden;
    current (folders, hidden);
    const auto& old = slots[(size_t) index];
    slots[(size_t) index] = menufolderslots::capture (old.isFree() ? "Platz " + juce::String (index + 1) : old.name, folders, hidden);
    save();
    update();
    if (askName)
        cells[index]->startRename();   // a new slot: a name of one's own right away (Jazz, Metallica …)
}

void MenuFolderSlotsPanel::clear (int index)
{
    if (menufolderslots::isFixed (index))
        return;
    slots[(size_t) index] = {};
    save();
    update();
}

void MenuFolderSlotsPanel::rename (int index, const juce::String& text)
{
    if (menufolderslots::isFixed (index) || slots[(size_t) index].isFree())
        return;
    if (text.trim().isNotEmpty())   // an empty name would free the slot: then the old one stays
    {
        slots[(size_t) index].name = text.trim();
        save();
    }
    update();
}

void MenuFolderSlotsPanel::showMenu (int index)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Jetzige Haken hier ablegen");
    menu.addItem (2, "Umbenennen", ! slots[(size_t) index].isFree());
    menu.addItem (3, "Leeren", ! slots[(size_t) index].isFree());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (cells[index]),
                        [safe = juce::Component::SafePointer<MenuFolderSlotsPanel> (this), index] (int choice)
    {
        if (safe == nullptr)
            return;
        if (choice == 1)
            safe->store (index, safe->slots[(size_t) index].isFree());
        else if (choice == 2)
            safe->cells[index]->startRename();
        else if (choice == 3)
            safe->clear (index);
    });
}

void MenuFolderSlotsPanel::save()
{
    if (! menufolderslots::save (slots, menufolderslots::file()))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Nicht gespeichert",
                                                "Die Konfigurationen ließen sich nicht schreiben: "_u + menufolderslots::file().getFullPathName(),
                                                "OK", this);
}
