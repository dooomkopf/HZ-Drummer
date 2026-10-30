#pragma once
#include "app/Library.h"
#include "app/views/Theme.h"
#include "app/Text.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

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
            .getChildFile (choice.getSelectedId() == 1 ? ".cache/HZ-Drummer/app.log" : ".cache/HZ-Drummer/run.log");
    }
    juce::ComboBox choice;
    juce::TextEditor output;
    juce::TextButton reload { "Aktualisieren" }, clear { "Log leeren" };
};
