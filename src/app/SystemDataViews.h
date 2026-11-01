#pragma once
#include "app/Library.h"
#include "app/FolderSort.h"
#include "app/MenuFolderSlotsPanel.h"
#include "app/views/Theme.h"
#include "app/Text.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <functional>
#include <vector>

class TrashPanel : public juce::Component, private juce::ListBoxModel
{
public:
    TrashPanel (juce::File folder, std::function<void()> restored)
        : data (std::move (folder)), onRestored (std::move (restored))
    {
        addAndMakeVisible (list); list.setModel (this); list.setRowHeight (52);
        addAndMakeVisible (info); info.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (restore); addAndMakeVisible (reload); addAndMakeVisible (empty);
        empty.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff713a40));   // red like "Löschen" in the main window
        empty.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffeeee));
        empty.onClick = [this]
        {
            const auto n = (int) entries.size();
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Papierkorb leeren",
                juce::String (n) + (n == 1 ? juce::String (" Eintrag wird") : " Einträge werden"_u)
                    + " endgültig gelöscht. Das lässt sich nicht rückgängig machen."_u,
                "Endgültig löschen"_u, "Abbrechen", this,
                juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<TrashPanel> (this)] (int r)
                {
                    if (safe == nullptr || r != 1) return;
                    if (! emptyOwnTrash (safe->data))
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Papierkorb leeren",
                                                                "Der Papierkorb ließ sich nicht ganz leeren."_u, "OK", safe.getComponent());
                    safe->refresh();
                }));
        };
        reload.onClick = [this] { refresh(); };
        restore.onClick = [this]
        {
            const int row = list.getSelectedRow();
            if (row < 0 || row >= (int) entries.size()) return;
            const auto error = restoreOwnTrash (entries[(size_t) row].key, data);
            if (error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wiederherstellen", error, "OK", this);
                return;
            }
            refresh();
            if (onRestored) onRestored();
        };
        refresh();
    }
    ~TrashPanel() override { list.setModel (nullptr); }
    void refresh()
    {
        entries = listOwnTrash (data);
        list.deselectAllRows(); list.updateContent(); list.repaint(); restore.setEnabled (false); empty.setEnabled (! entries.empty());
        info.setText (entries.empty() ? "Der Papierkorb ist leer." : juce::String (entries.size()) + " Einträge – auswählen und wiederherstellen."_u,
                      juce::dontSendNotification);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        info.setBounds (r.removeFromTop (28));
        auto buttons = r.removeFromBottom (36);
        restore.setBounds (buttons.removeFromLeft (180)); buttons.removeFromLeft (10);
        reload.setBounds (buttons.removeFromLeft (140));
        empty.setBounds (buttons.removeFromRight (180));
        r.removeFromBottom (10); list.setBounds (r);
    }
private:
    int getNumRows() override { return (int) entries.size(); }
    void selectedRowsChanged (int row) override { restore.setEnabled (row >= 0 && row < (int) entries.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) entries.size()) return;
        const auto& entry = entries[(size_t) row];
        g.fillAll (selected ? juce::Colour (0xff2f4f7a) : theme::panel);
        g.setColour (theme::textMain); g.setFont (juce::FontOptions (17.0f));
        g.drawFittedText (entry.name.isEmpty() ? entry.libraryId : entry.name, 10, 3, w - 20, h / 2, juce::Justification::centredLeft, 1);
        g.setColour (theme::textDim); g.setFont (juce::FontOptions (13.0f));
        g.drawText ((entry.song ? "Arrangement" : "Baustein") + "  ·  "_u + entry.deletedAt,
                    10, h / 2, w - 20, h / 2, juce::Justification::centredLeft);
    }
    juce::File data;
    std::function<void()> onRestored;
    std::vector<LibraryTrashEntry> entries;
    juce::ListBox list;
    juce::Label info;
    juce::TextButton restore { "Wiederherstellen" }, reload { "Aktualisieren" }, empty { "Papierkorb leeren" };
};

class LogPanel : public juce::Component
{
public:
    LogPanel()
    {
        addAndMakeVisible (choice); addAndMakeVisible (output); addAndMakeVisible (reload); addAndMakeVisible (clear);
        choice.addItem ("App-Protokoll (app.log)", 1); choice.addItem ("Start und Abstürze (run.log)"_u, 2);
        choice.addItem ("Import aus Quellen (import.log)", 3);
        choice.onChange = [this] { refresh(); }; choice.setSelectedId (2);
        output.setMultiLine (true); output.setReadOnly (true); output.setScrollbarsShown (true);
        output.setFont (juce::FontOptions (18.0f));
        reload.onClick = [this] { refresh(); };
        clear.onClick = [this]
        {
            const auto file = logFile();
            auto stream = file.createOutputStream();
            if (stream == nullptr || ! stream->setPosition (0) || stream->truncate().failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Log leeren",
                    "Die Logdatei konnte nicht geleert werden.", "OK", this);
                return;
            }
            stream->flush(); stream.reset(); refresh();
        };
        refresh();
    }
    void refresh()
    {
        const auto file = logFile();
        juce::String text;
        if (auto input = file.createInputStream())
        {
            input->setPosition (juce::jmax ((juce::int64) 0, input->getTotalLength() - 512 * 1024));
            text = input->readEntireStreamAsString();
        }
        else text = "Noch keine Logdatei vorhanden: " + file.getFullPathName();
        output.setText (text, false); output.moveCaretToEnd();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14); auto top = r.removeFromTop (32);
        clear.setBounds (top.removeFromRight (130)); top.removeFromRight (10);
        reload.setBounds (top.removeFromRight (140)); top.removeFromRight (10); choice.setBounds (top);
        r.removeFromTop (10); output.setBounds (r);
    }
private:
    juce::File logFile() const
    {
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (choice.getSelectedId() == 1 ? ".cache/HZ-Drummer/app.log"
                           : choice.getSelectedId() == 3 ? ".cache/HZ-Drummer/import.log" : ".cache/HZ-Drummer/run.log");
    }
public:
    void showLog (int id) { choice.setSelectedId (id); }   // 1 app, 2 start, 3 import
private:
    juce::ComboBox choice;
    juce::TextEditor output;
    juce::TextButton reload { "Aktualisieren" }, clear { "Log leeren" };
};

// System → Menü-Ordner: a tick per top-level folder says whether the Baustein menu of the main window shows it;
// saved at once (ordner.json). The search (Strg+K) and the arranger still know every entry. Above the ticks the ten
// slots that call up a whole set of them (MenuFolderSlotsPanel).
class MenuFoldersPanel : public juce::Component
{
public:
    explicit MenuFoldersPanel (Library& lib) : library (lib)
    {
        // on top: the green pop-up menus while browsing with the arrow keys (GhostMenu), and how long they stay
        addAndMakeVisible (ghostToggle);
        ghostToggle.setColour (juce::ToggleButton::textColourId, theme::textMain);
        ghostToggle.setColour (juce::ToggleButton::tickColourId, theme::textMain);
        ghostToggle.onClick = [this] { ghostSeconds.setEnabled (ghostToggle.getToggleState()); if (onGhostChanged) onGhostChanged(); };
        addAndMakeVisible (ghostLabel);
        ghostLabel.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (ghostSeconds);
        ghostSeconds.setRange (0.5, 5.0, 0.1);
        ghostSeconds.setTextValueSuffix (" s");
        ghostSeconds.getProperties().set ("pill", true);   // value box drawn by HZLook as a pill
        ghostSeconds.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
        ghostSeconds.onValueChange = [this] { if (onGhostChanged) onGhostChanged(); };
        addAndMakeVisible (slotsTitle);
        slotsTitle.setText ("Konfigurationen", juce::dontSendNotification);
        slotsTitle.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        slotsTitle.setColour (juce::Label::textColourId, theme::textMain);
        addAndMakeVisible (slotsPanel);
        slotsPanel.onApplied = [this] { refresh(); };
        addAndMakeVisible (hint);
        hint.setText ("Haken weg: Der Ordner fehlt im Baustein-Menü des Hauptfensters. Suchen (Strg+K) und der Arranger finden seine Bausteine weiterhin."_u,
                      juce::dontSendNotification);
        hint.setFont (juce::FontOptions (14.0f));
        hint.setColour (juce::Label::textColourId, juce::Colour (0xffe07a70));   // the light red of all help lines
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
    }

    void refresh()
    {
        list.removeAllChildren();
        rows.clear();
        const int width = juce::jmax (320, viewport.getWidth() - 24);
        int y = 4;
        for (auto area : { FolderTree::Songs, FolderTree::Patterns, FolderTree::Bundled })
        {
            auto* title = new juce::Label();
            rows.add (title);
            title->setText (area == FolderTree::Songs ? "Arrangements (Songs)"_u : area == FolderTree::Patterns ? "Bausteine (Pattern)" : "Bibliothek-Bausteine",
                            juce::dontSendNotification);
            title->setFont (juce::FontOptions (15.0f, juce::Font::bold));
            title->setColour (juce::Label::textColourId, theme::textMain);
            list.addAndMakeVisible (title);
            title->setBounds (0, y, width, 26);
            y += 28;
            // the top-level names of both views, as the slots below count them: without its tick a name is hidden in both;
            // alphabetical, so that one finds a folder; the menu keeps its own order
            const auto tree = library.folders (area);
            const auto names = library.topLevelFolders (area);
            std::vector<juce::String> shown (names.begin(), names.end());
            std::stable_sort (shown.begin(), shown.end(), [] (const juce::String& a, const juce::String& b) { return folders::sortsBefore (a, b); });
            for (const auto& name : shown)
            {
                const auto here = std::find_if (tree.folders.begin(), tree.folders.end(), [&] (const FolderNode& n) { return n.name.equalsIgnoreCase (name); });
                const auto count = here != tree.folders.end() ? juce::String (folders::countItems (*here))
                                 : library.view() == Library::View::Album ? juce::String ("nur eigene Ordner") : juce::String ("nur Album-Ansicht");
                auto* tick = new juce::ToggleButton (name + "   (" + count + ")");
                rows.add (tick);
                tick->setColour (juce::ToggleButton::textColourId, theme::textMain);
                tick->setColour (juce::ToggleButton::tickColourId, theme::textMain);
                tick->setToggleState (! library.folderHidden (area, name), juce::dontSendNotification);
                tick->onClick = [this, area, name, tick]
                {
                    if (const auto error = library.setFolderHidden (area, name, ! tick->getToggleState()); error.isNotEmpty())
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Nicht gespeichert", error);
                    slotsPanel.update();   // which slot matches the ticks now
                };
                list.addAndMakeVisible (tick);
                tick->setBounds (12, y, width - 12, 26);
                y += 28;
            }
            y += 12;
        }
        list.setSize (width + 12, y);
        slotsPanel.update();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        ghostToggle.setBounds (r.removeFromTop (28));
        auto duration = r.removeFromTop (30).withTrimmedLeft (28);
        ghostLabel.setBounds (duration.removeFromLeft (60));
        ghostSeconds.setBounds (duration.removeFromLeft (260));
        r.removeFromTop (12);
        slotsTitle.setBounds (r.removeFromTop (24));
        slotsPanel.setBounds (r.removeFromTop (5 * 32));
        r.removeFromTop (12);
        hint.setBounds (r.removeFromTop (44));
        r.removeFromTop (6);
        viewport.setBounds (r);
        refresh();   // the rows follow the width
    }

    juce::ToggleButton ghostToggle { "Grüne Einblend-Menüs aktiv"_u };
    juce::Label ghostLabel { {}, "Dauer" };
    juce::Slider ghostSeconds;
    std::function<void()> onGhostChanged;   // reads ghostToggle and ghostSeconds

private:
    Library& library;
    juce::Label slotsTitle;
    MenuFolderSlotsPanel slotsPanel { library };   // after library: it reads the folders when it is made
    juce::Label hint;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<juce::Component> rows;
};
