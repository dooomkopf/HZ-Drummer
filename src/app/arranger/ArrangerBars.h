#pragma once

#include "app/Song.h"
#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// Two rows of the arranger: the toolbar at the top (song name, undo/redo, new/load/save)
// and the detail row below the sequence (what the selected block is and what to do with it).
// Visible buttons for everything, so no action depends on a hidden gesture.

class ArrangerToolbar : public juce::Component
{
public:
    std::function<void()> onNew, onLoad, onSave, onSaveAs, onClearScratch;
    std::function<void (const juce::String&)> onRename;

    ArrangerToolbar()
    {
        addAndMakeVisible (name);
        name.setFont (juce::FontOptions (arrangerstyle::fontTitle));
        name.getProperties().set ("pill", true);        // drawn by HZLook as a pill
        name.setIndents (16, 0);      // text clear of the round ends; no top indent, so centred exactly
        name.setJustification (juce::Justification::centredLeft);   // the text in the middle, as in the buttons
        name.setTextToShowWhenEmpty ("Song-Name", arrangerstyle::textDim);
        name.onReturnKey = [this] { if (onRename) onRename (name.getText()); };
        name.onFocusLost = [this] { if (onRename) onRename (name.getText()); };
        for (auto* b : { &newButton, &loadButton, &clearScratchButton, &saveButton, &saveAsButton })
            addAndMakeVisible (*b);
        newButton.onClick          = [this] { if (onNew) onNew(); };
        loadButton.onClick         = [this] { if (onLoad) onLoad(); };
        clearScratchButton.onClick = [this] { if (onClearScratch) onClearScratch(); };
        saveButton.onClick         = [this] { if (onSave) onSave(); };
        saveAsButton.onClick       = [this] { if (onSaveAs) onSaveAs(); };
        saveButton.setTooltip ("Song speichern (Strg+S)");
        clearScratchButton.setColour (juce::TextButton::buttonColourId, arrangerstyle::danger);
        clearScratchButton.setTooltip ("Alle gesammelten Bausteine aus der Ablage entfernen (Strg+Z holt sie zurück)"_u);
        addAndMakeVisible (status);
        status.setFont (juce::FontOptions (arrangerstyle::fontSmall));
        status.setJustificationType (juce::Justification::centredRight);
    }

    void update (const juce::String& songName, bool unsaved, bool hasParts, bool hasScratch)
    {
        if (! name.hasKeyboardFocus (false) && name.getText() != songName)
            name.setText (songName, juce::dontSendNotification);
        clearScratchButton.setEnabled (hasScratch);
        saveButton.setEnabled (hasParts && unsaved);
        saveAsButton.setEnabled (hasParts);
        this->unsaved = unsaved;
        showStatus();
    }

    // a problem that must not go unnoticed (e.g. the working state could not be written); "" clears it
    void setWarning (const juce::String& text)
    {
        warning = text;
        showStatus();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        newButton.setBounds (r.removeFromLeft (70));       r.removeFromLeft (8);
        loadButton.setBounds (r.removeFromLeft (100));     r.removeFromLeft (8);
        saveAsButton.setBounds (r.removeFromRight (150));  r.removeFromRight (8);
        saveButton.setBounds (r.removeFromRight (110));    r.removeFromRight (8);
        clearScratchButton.setBounds (r.removeFromRight (130));   r.removeFromRight (8);
        status.setBounds (r.removeFromRight (juce::jmax (120, r.getWidth() / 3)));
        r.removeFromRight (8);
        name.setBounds (r);
    }

private:
    void showStatus()
    {
        if (warning.isNotEmpty())
        {
            status.setText (warning, juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, juce::Colour (0xffe0443a));
            return;
        }
        status.setText (unsaved ? "ungespeichert" : "gespeichert", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, unsaved ? juce::Colour (0xffe89a2c) : arrangerstyle::textDim);
    }

    juce::TextEditor name;
    juce::TextButton newButton { "Neu" }, loadButton { "Laden" }, clearScratchButton { "Ablage leeren" },
                     saveButton { "Speichern" }, saveAsButton { "Speichern unter" };
    juce::Label status;
    juce::String warning;
    bool unsaved = false;
};

//==============================================================================
class BlockDetailRow : public juce::Component
{
public:
    std::function<void (int delta)> onRepeats;
    std::function<void()> onRange, onDuplicate, onMoveLeft, onMoveRight, onRemove, onPlayFrom, onStop, onEdit, onUpdateFromSource;

    BlockDetailRow()
    {
        addAndMakeVisible (title);
        title.setFont (juce::FontOptions (arrangerstyle::fontBlockSmall));
        title.setColour (juce::Label::textColourId, arrangerstyle::textDim);
        for (auto* b : { &minus, &plus, &range, &duplicate, &left, &right, &remove, &play, &edit, &fromSource })
            addAndMakeVisible (*b);
        edit.onClick       = [this] { if (onEdit) onEdit(); };
        fromSource.onClick = [this] { if (onUpdateFromSource) onUpdateFromSource(); };
        edit.setTooltip ("Die Kopie dieses Blocks im Raster bearbeiten; die Änderung gilt nur in diesem Song");
        fromSource.setTooltip ("Holt die aktuelle Fassung des Patterns aus der Bibliothek in diesen Block. Nur aktiv, wenn das "
                               "Original seit dem Einfügen geändert wurde (\xe2\x86\xbb am Block). Änderungen, die du am Block in "
                               "diesem Song gemacht hast, gehen dabei verloren; vorher kommt eine Rückfrage, Strg+Z macht es rückgängig."_u);
        minus.onClick     = [this] { if (onRepeats) onRepeats (-1); };
        plus.onClick      = [this] { if (onRepeats) onRepeats (1); };
        range.onClick     = [this] { if (onRange) onRange(); };
        duplicate.onClick = [this] { if (onDuplicate) onDuplicate(); };
        left.onClick      = [this] { if (onMoveLeft) onMoveLeft(); };
        right.onClick     = [this] { if (onMoveRight) onMoveRight(); };
        remove.onClick    = [this] { if (onRemove) onRemove(); };
        play.onClick      = [this] { if (anyPlaying) { if (onStop) onStop(); } else if (onPlayFrom) onPlayFrom(); };
        minus.setTooltip ("eine Wiederholung weniger");
        plus.setTooltip ("eine Wiederholung mehr");
        range.setTooltip ("nur bestimmte Takte dieses Bausteins spielen");
        play.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e7a4a));   // lit while the sequence plays
        play.getProperties().set ("transportIcon", true);   // drawn by HZLook as > or ||
        addAndMakeVisible (repeats);
        repeats.setFont (juce::FontOptions (arrangerstyle::fontText));
        repeats.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (length);
        length.setFont (juce::FontOptions (arrangerstyle::fontBlockSmall));
        length.setColour (juce::Label::textColourId, arrangerstyle::textDim);
        length.setJustificationType (juce::Justification::centredRight);
        update (nullptr, {}, {}, {});
    }

    // part == nullptr: nothing selected; sourceName: the pattern the block comes from;
    // canUpdate: the library's source differs from the copy
    void update (const SongPart* part, const juce::String& partName, const juce::String& sourceName,
                 const juce::String& lengthText, bool canUpdate = false)
    {
        const bool has = part != nullptr;
        lastTitle = has ? "Block: " + partName
                          + (! part->label.empty() && sourceName.isNotEmpty() ? "   (Pattern: " + sourceName + ")" : juce::String())
                          + (part->edited ? juce::String ("   (im Song geändert)"_u) : juce::String())
                        : juce::String ("Kein Block gewählt (Klick im Ablauf)"_u);
        edit.setEnabled (has && part->kind == PartKind::Pattern && part->copy);
        fromSource.setEnabled (has && canUpdate);
        if (playingText.isEmpty())
            title.setText (lastTitle, juce::dontSendNotification);
        repeats.setText (has ? juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (part->repeats) : juce::String(), juce::dontSendNotification);
        for (auto* c : { (juce::Component*) &minus, (juce::Component*) &plus, (juce::Component*) &duplicate,
                         (juce::Component*) &left, (juce::Component*) &right, (juce::Component*) &remove })
            c->setEnabled (has);
        updatePlayButton();
        range.setEnabled (has && part->kind == PartKind::Pattern && part->copy && part->copy->bars.size() > 1);
        length.setText (lengthText, juce::dontSendNotification);
    }

    // sequencePlaying: the sequence runs (Play lights up); anything: also an audition (Stopp works);
    // where: "Spielt: Strophe · Takt 3/16" or ""
    void setPlaying (bool sequencePlaying, bool anything, const juce::String& where)
    {
        anyPlaying = anything;
        updatePlayButton();
        play.setToggleState (sequencePlaying, juce::dontSendNotification);
        if (where != playingText)
        {
            playingText = where;
            title.setText (playingText.isNotEmpty() ? playingText : lastTitle, juce::dontSendNotification);
            title.setColour (juce::Label::textColourId, playingText.isNotEmpty() ? arrangerstyle::playing : arrangerstyle::textDim);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (26);
        length.setBounds (top.removeFromRight (300));
        title.setBounds (top);
        r.removeFromTop (4);
        // 8 px between the buttons, 4 px inside the pair < >; Play (only the symbol) right, at least 8 px away;
        // the text buttons get narrower when the row is shorter than at full width (702 px)
        play.setBounds (r.removeFromRight (48));
        r.removeFromRight (8);
        const bool compact = r.getWidth() < 702;
        minus.setBounds (r.removeFromLeft (34));
        repeats.setBounds (r.removeFromLeft (44));
        plus.setBounds (r.removeFromLeft (34));                       r.removeFromLeft (8);
        range.setBounds (r.removeFromLeft (compact ? 60 : 70));       r.removeFromLeft (8);
        duplicate.setBounds (r.removeFromLeft (compact ? 88 : 100));  r.removeFromLeft (8);
        left.setBounds (r.removeFromLeft (36));                       r.removeFromLeft (4);
        right.setBounds (r.removeFromLeft (36));                      r.removeFromLeft (8);
        remove.setBounds (r.removeFromLeft (compact ? 84 : 96));      r.removeFromLeft (8);
        edit.setBounds (r.removeFromLeft (compact ? 88 : 100));       r.removeFromLeft (8);
        fromSource.setBounds (r.removeFromLeft (compact ? 80 : 100));
    }

private:
    juce::Label title, repeats, length;
    juce::String lastTitle, playingText;
    bool anyPlaying = false;

    // one button for both: Play while nothing sounds, Stopp while the sequence or an audition plays
    void updatePlayButton()
    {
        play.getProperties().set ("playing", anyPlaying);
        play.setButtonText (anyPlaying ? "Stopp" : "Play");   // not drawn, the name for accessibility
        play.setTooltip (anyPlaying ? "Wiedergabe und Vorhören anhalten (Esc)"_u
                                    : "Ablauf ab dem gewählten Block abspielen, ohne Auswahl ab dem ersten (Enter)"_u);
        // always active: without a selected block ArrangerPanel::playFrom starts at the first one
    }
    juce::TextButton minus { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) }, plus { "+" }, range { "Takte" },
                     duplicate { "Duplizieren" },
                     left { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")) }, right { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")) },
                     remove { "Entfernen" }, play { "Play" }, edit { "Raster" }, fromSource { "Original" };
};
