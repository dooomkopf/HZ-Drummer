// ArrangerPanel, part 3: saving and loading songs, the working state (arranger.json, ablage.json), keys.

#include "app/arranger/ArrangerPanel.h"

#include "app/Library.h"
#include "app/NavigationMenus.h"
#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"
#include "app/views/InWindowDialog.h"
#include "app/views/PlaybackView.h"

#include <algorithm>

using namespace arrangerstyle;

namespace
{
    juce::File stateFile()   { return Library::songFolder().getSiblingFile ("arranger.json"); }
    juce::File scratchFile() { return Library::songFolder().getSiblingFile ("ablage.json"); }
    juce::String songFileName (const std::string& id) { return juce::String (id) + ".json"; }
}

void ArrangerPanel::save (bool asNew)
{
    if (model.song().parts.empty())
        return;
    Song copy = model.song();
    if (asNew || copy.id.empty())
    {
        auto* w = new InWindowDialog ("Arrangement speichern", "Name des Arrangements:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", juce::String (copy.name), "Name:");
        w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), w, copy] (int r) mutable
        {
            if (safe == nullptr || r != 1)
                return;
            copy.name = w->getTextEditorContents ("name").trim().toStdString();
            if (copy.name.empty())
                return;
            copy.id.clear();   // a new file
            if (auto err = safe->host.arrangerSaveSong (copy); err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Speichern", err);
            else
            {
                safe->model.adoptSaved (copy);
                safe->libraryChanged();
            }
        }), true);
        return;
    }
    if (auto err = host.arrangerSaveSong (copy); err.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Speichern", err);
    else
    {
        model.adoptSaved (copy);
        libraryChanged();
    }
}

void ArrangerPanel::newSong()
{
    confirmDiscard ([this]
    {
        Song s;
        s.name = "Neues Arrangement";
        model.load (s);
        selectPart (-1);
    });
}

void ArrangerPanel::showLoadMenu()
{
    juce::PopupMenu menu;
    const auto& lib = host.getLibrary();
    std::vector<std::string> ids;   // by id: the library may reload before the choice arrives
    addFolderMenu (menu, lib.folders (FolderTree::Songs), [&] (juce::PopupMenu& m, const std::string& id, const FolderPath& folder)
    {
        if (const int i = lib.indexOf (id); i >= 0)
        {
            ids.push_back (id);
            m.addItem ((int) ids.size(), folders::shortName (juce::String (lib.get (i).name), folder));
        }
        return false;
    });
    if (ids.empty())
        menu.addItem (1, "(keine gespeicherten Arrangements)", false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolbar),
                        [safe = juce::Component::SafePointer<ArrangerPanel> (this), ids] (int r)
    {
        if (safe == nullptr || r <= 0 || r > (int) ids.size())
            return;
        const auto& lib = safe->host.getLibrary();
        if (const auto* song = lib.song (lib.indexOf (ids[(size_t) r - 1])); song != nullptr)
            safe->loadSong (*song);
    });
}

void ArrangerPanel::confirmDiscard (std::function<void()> then)
{
    if (! model.hasUnsavedChanges())
    {
        then();
        return;
    }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
        "Der Ablauf \""_u + juce::String (model.song().name) + "\" ist nicht gespeichert. Verwerfen?",
        "Verwerfen", "Behalten", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), then] (int r)
        {
            if (safe != nullptr && r == 1)
                then();
        }));
}

void ArrangerPanel::saveState()
{
    juce::String err;
    Song s = model.song();
    if (s.name.empty())
        s.name = "Neues Arrangement";
    if (saveSong (s, stateFile(), err))
    {
        stateDirty = false;
        toolbar.setWarning ({});
        return;
    }
    // keep trying, and say so: this is the safety net for unsaved work
    stateDueMs = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    toolbar.setWarning ("Arbeitsstand nicht gesichert: " + err);
}

void ArrangerPanel::loadState()
{
    juce::String err;
    std::vector<SongPart> list;
    if (loadPartList (scratchFile(), list, err))
        model.loadScratch (std::move (list));
    else
    {
        const auto keep = scratchFile().getSiblingFile ("ablage.defekt-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S") + ".json");
        toolbar.setWarning ("Ablage nicht lesbar (" + err + "), Kopie: " + keep.getFileName()
                            + (scratchFile().copyFileTo (keep) ? "" : " (Kopie fehlgeschlagen!)"));
    }

    Song s;
    if (stateFile().existsAsFile())
    {
        if (::loadSong (stateFile(), s, err))
        {
            juce::String ignored;
            embedCopies (s, host.getLibrary().all(), ignored);   // an old reference song: copies, so it can be saved again
            model.load (s);
            // "unsaved" compares with the song file of the same id, if there is one. That file may still
            // hold references only (v1), so it gets its copies the same way before the comparison.
            Song onDisk;
            juce::String err2;
            const auto saved = Library::songFolder().getChildFile (songFileName (s.id));
            if (! s.id.empty() && saved.existsAsFile() && ::loadSong (saved, onDisk, err2))
            {
                embedCopies (onDisk, host.getLibrary().all(), ignored);
                model.markSavedAs (onDisk);
                if (model.hasUnsavedChanges())
                    juce::Logger::writeToLog ("Arranger: Arbeitsstand \"" + juce::String (s.name) + "\" weicht von "
                                              + saved.getFileName() + " ab, gilt als ungespeichert");
            }
            else if (! s.id.empty() || ! s.parts.empty())
            {
                model.markUnsaved();
                juce::Logger::writeToLog ("Arranger: Arbeitsstand \"" + juce::String (s.name) + "\" hat keine gespeicherte Datei"
                                          + (err2.isNotEmpty() ? " (" + err2 + ")" : juce::String()) + ", gilt als ungespeichert");
            }
        }
        else
        {
            // never overwrite a damaged state silently: keep it aside and say so
            const auto keep = stateFile().getSiblingFile ("arranger.defekt-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S") + ".json");
            toolbar.setWarning ("Arbeitsstand defekt, Kopie: " + keep.getFileName()
                                + (stateFile().copyFileTo (keep) ? "" : " (Kopie fehlgeschlagen!)"));
            s = {};
            s.name = "Neues Arrangement";
            model.load (s);
        }
    }
    else
    {
        s.name = "Neues Arrangement";
        model.load (s);
    }
    modelChanged();
    stateDirty = false;
}

void ArrangerPanel::saveScratch()
{
    juce::String err;
    if (! savePartList (model.scratch(), scratchFile(), err))
        toolbar.setWarning ("Ablage nicht gesichert: " + err);
}

bool ArrangerPanel::keyPressed (const juce::KeyPress& key)
{
    // Linux sends lowercase letter key codes for Ctrl shortcuts (as in the raster editor).
    const int c = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());
    const auto mods = key.getModifiers();
    const int p = sequence.getSelected();
    const int n = (int) model.song().parts.size();

    if (mods.isCommandDown())
    {
        if (c == 'Z' && mods.isShiftDown()) { model.redo(); return true; }
        if (c == 'Z') { model.undo(); return true; }
        if (c == 'Y') { model.redo(); return true; }
        if (c == 'S') { save (false); return true; }
        if (c == 'K') { find.focusField(); return true; }
        // only in the sequence (not in the Ablage or the search list); text fields keep their own Ctrl+C/X/V
        if (sequence.hasKeyboardFocus (true) && (c == 'C' || c == 'X' || c == 'V'))
        {
            if (c == 'C' && p >= 0)
                copiedBlock = model.song().parts[(size_t) p];
            else if (c == 'X' && p >= 0)
            {
                copiedBlock = model.song().parts[(size_t) p];   // as usual, Ctrl+V afterwards puts it elsewhere
                model.moveToScratch (p);
                selectPart (juce::jmin (p, n - 2));
            }
            else if (c == 'V' && copiedBlock.has_value())
            {
                // always a new identity: an open raster edit of the copied block must not land in the pasted one
                auto pasted = *copiedBlock;
                pasted.uid.clear();
                addToSequence ({ pasted }, insertIndex());
            }
            return true;
        }
        return false;
    }
    if (c == juce::KeyPress::escapeKey) { stopAudition(); return true; }
    if (c == juce::KeyPress::F2Key && p >= 0) { askRename (p); return true; }
    if (c == juce::KeyPress::spaceKey && ! mods.isAnyModifierKeyDown())
    {
        // Space here means the sequence: start it if the transport holds something else, otherwise pause/resume
        if (! host.arrangerPlanLoaded() && n > 0 && buildError.isEmpty())
        {
            playFrom (juce::jmax (0, p));
            return true;
        }
        return host.handleTransportKey (key);
    }

    if (sequence.hasKeyboardFocus (true) && n > 0)
    {
        if (mods.isAltDown() && c == juce::KeyPress::leftKey)  { if (p > 0) { model.movePart (p, p - 1); selectPart (p - 1); } return true; }
        if (mods.isAltDown() && c == juce::KeyPress::rightKey) { if (p >= 0 && p + 1 < n) { model.movePart (p, p + 1); selectPart (p + 1); } return true; }
        if (c == juce::KeyPress::leftKey)  { selectPart (p < 0 ? 0 : juce::jmax (0, p - 1)); return true; }
        if (c == juce::KeyPress::rightKey) { selectPart (p < 0 ? 0 : juce::jmin (n - 1, p + 1)); return true; }
        if (c == juce::KeyPress::deleteKey || c == juce::KeyPress::backspaceKey)
        {
            if (p >= 0) { model.removePart (p); selectPart (juce::jmin (p, n - 2)); }
            return true;
        }
        const auto ch = key.getTextCharacter();
        if ((ch == '+' || ch == '-') && p >= 0) { model.setRepeats (p, model.song().parts[(size_t) p].repeats + (ch == '+' ? 1 : -1)); return true; }
        if (c == juce::KeyPress::returnKey) { playFrom (juce::jmax (0, p)); return true; }
    }
    return host.handleTransportKey (key);
}
