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

ArrangerPanel::ArrangerPanel (ArrangerHost& h) : host (h)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);   // holds the focus when no child does, so the shortcuts keep working
    for (auto* c : { (juce::Component*) &toolbar, (juce::Component*) &find, (juce::Component*) &scratch,
                     (juce::Component*) &sequenceViewport, (juce::Component*) &detail, (juce::Component*) &hint })
        addAndMakeVisible (*c);
    sequenceViewport.setViewedComponent (&sequence, false);
    sequenceViewport.setScrollBarsShown (true, false);
    sequence.setWantsKeyboardFocus (true);

    hint.setFont (juce::FontOptions (fontHint));
    hint.setColour (juce::Label::textColourId, textDim);
    hint.setText ("Klick: anhören (nochmal: Stopp)   ·   Kästchen: markieren   ·   Ziehen in Ablage/Ablauf   ·   Rechtsklick/Doppelklick auf Block: Menü/Umbenennen   ·   "
                  "Strg+K Suche   ·   Strg+S Speichern   ·   Strg+Z/Y Rückgängig/Wiederholen   ·   Entf   ·   Alt+Pfeile   ·   +/-   ·   Esc Stopp"_u,
                  juce::dontSendNotification);

    // model -> views
    model.validateSong = [this] (const Song& song)
    {
        return songChangeError (model.song(), song, host.getLibrary().all());
    };
    model.onRejected = [this] (const juce::String& error)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            "Ablauf unverändert"_u, error + "\n\nDie Änderung wurde nicht übernommen. Der bisherige Ablauf bleibt erhalten."_u,
            "OK", this);
    };
    model.onChange = [this] { modelChanged(); };
    model.onScratchChange = [this] { saveScratch(); };

    // toolbar
    toolbar.onNew    = [this] { newSong(); };
    toolbar.onLoad   = [this] { showLoadMenu(); };
    toolbar.onClearScratch = [this] { model.clearScratch(); };
    toolbar.onSave   = [this] { save (false); };
    toolbar.onSaveAs = [this] { save (true); };
    toolbar.onRename = [this] (const juce::String& n) { model.setName (n.trim().toStdString()); };

    // Finden
    find.onAudition = [this] (const std::string& id)
    {
        if (auto parts = partsForIds ({ id }); ! parts.empty())
            toggleAudition ("find:" + id, parts.front());
    };
    find.isAuditioning   = [this] (const std::string& id) { return auditionKey == "find:" + id && host.arrangerAuditionActive(); };
    find.isUsed          = [this] (const std::string& id)
    {
        return std::any_of (model.song().parts.begin(), model.song().parts.end(),
                            [&] (const SongPart& p) { return p.kind == PartKind::Pattern && p.sourceId == id; });
    };
    find.onAddToScratch  = [this] (const std::vector<std::string>& ids) { model.addToScratch (partsForIds (ids)); };
    find.onAddToSequence = [this] (const std::vector<std::string>& ids) { addToSequence (partsForIds (ids), insertIndex()); };
    find.onEscape        = [this] { stopAudition(); };

    // Ablage
    scratch.onAudition   = [this] (int i) { toggleAudition ("scratch:" + std::to_string (i), model.scratch()[(size_t) i]); };
    scratch.isAuditioning = [this] (int i) { return auditionKey == "scratch:" + std::to_string (i) && host.arrangerAuditionActive(); };
    scratch.onToSequence = [this] (int i) { addToSequence ({ model.scratch()[(size_t) i] }, insertIndex()); };
    scratch.onRemove     = [this] (int i) { model.removeFromScratch (i); };
    scratch.onDrop       = [this] (const juce::var& d) { model.addToScratch (partsForDrag (d)); };
    scratch.onContextMenu = [this] (int i)
    {
        juce::PopupMenu menu;
        menu.addItem (1, juce::String (juce::CharPointer_UTF8 ("Anh\xc3\xb6ren / Stopp")));
        menu.addItem (2, "+ in Ablauf (nach der Auswahl)");
        menu.addSeparator();
        menu.addItem (3, "Aus Ablage entfernen");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scratch).withMousePosition(),
                            [safe = juce::Component::SafePointer<ArrangerPanel> (this), i] (int r)
        {
            if (safe == nullptr || r == 0 || i >= (int) safe->model.scratch().size())
                return;
            if (r == 1) safe->toggleAudition ("scratch:" + std::to_string (i), safe->model.scratch()[(size_t) i]);
            if (r == 2) safe->addToSequence ({ safe->model.scratch()[(size_t) i] }, safe->insertIndex());
            if (r == 3) safe->model.removeFromScratch (i);
        });
    };

    // Ablauf
    sequence.onSelect   = [this] (int part) { selectPart (part); };
    sequence.onMove     = [this] (int from, int to) { model.movePart (from, to); selectPart (to); };
    sequence.onDrop     = [this] (const juce::var& d, int index) { addToSequence (partsForDrag (d), index); };
    sequence.onRepeats  = [this] (int part, int delta) { model.setRepeats (part, model.song().parts[(size_t) part].repeats + delta); };
    sequence.onDoubleClick = [this] (int part) { pendingSeekPart = -1; askRename (part); };
    sequence.onClick    = [this] (int part)
    {
        // like the song strip, but a moment later: the first click of a double-click must not jump
        if (transportHoldsSequence())
        {
            pendingSeekPart = part;
            seekDueMs = juce::Time::getMillisecondCounterHiRes() + 280.0;
        }
    };
    sequence.onContextMenu = [this] (int part) { showBlockMenu (part); };

    // detail row
    detail.onRepeats   = [this] (int delta) { if (int p = sequence.getSelected(); p >= 0) model.setRepeats (p, model.song().parts[(size_t) p].repeats + delta); };
    detail.onRange     = [this] { askRange(); };
    detail.onEdit      = [this] { editBlock(); };
    detail.onUpdateFromSource = [this] { updateFromSource(); };
    detail.onDuplicate = [this] { if (int p = sequence.getSelected(); p >= 0) { addToSequence ({ model.song().parts[(size_t) p] }, p + 1); } };
    detail.onMoveLeft  = [this] { if (int p = sequence.getSelected(); p > 0) { model.movePart (p, p - 1); selectPart (p - 1); } };
    detail.onMoveRight = [this] { if (int p = sequence.getSelected(); p >= 0 && p + 1 < (int) model.song().parts.size()) { model.movePart (p, p + 1); selectPart (p + 1); } };
    detail.onRemove    = [this] { if (int p = sequence.getSelected(); p >= 0) { model.removePart (p); selectPart (juce::jmin (p, (int) model.song().parts.size() - 1)); } };
    detail.onPlayFrom  = [this] { playFrom (juce::jmax (0, sequence.getSelected())); };
    detail.onStop      = [this] { stopAudition(); };

    libraryChanged();
    loadState();
    startTimerHz (10);
}

ArrangerPanel::~ArrangerPanel()
{
    if (stateDirty)
        saveState();
    setLookAndFeel (nullptr);
}

//==============================================================================
void ArrangerPanel::songDeleted (const std::string& libraryId)
{
    if (libraryId == "song." + model.song().id)
    {
        model.markUnsaved();
        toolbar.update (juce::String (model.song().name), true, ! model.song().parts.empty(), ! model.scratch().empty());
    }
}

void ArrangerPanel::patternRenamed (const std::string& sourceId, const std::string& oldName, const std::string& newName)
{
    // the song files were renamed the same way: "unsaved" compares with the file again
    if (model.renameCopies (sourceId, oldName, newName) == 0)
        return;
    Song onDisk;
    juce::String err;
    const auto saved = Library::songFolder().getChildFile (songFileName (model.song().id));
    if (! model.song().id.empty() && saved.existsAsFile() && ::loadSong (saved, onDisk, err))
    {
        embedCopies (onDisk, host.getLibrary().all(), err);
        model.markSavedAs (onDisk);
    }
    toolbar.update (juce::String (model.song().name), model.hasUnsavedChanges(), ! model.song().parts.empty(), ! model.scratch().empty());
}

void ArrangerPanel::songRenamed (const std::string& libraryId, const std::string& newName)
{
    if (libraryId != "song." + model.song().id)
        return;
    const bool wasSaved = ! model.hasUnsavedChanges();
    model.setName (newName);
    if (wasSaved)
        model.markSavedAs (model.song());   // the file has the new name too
    toolbar.update (juce::String (model.song().name), model.hasUnsavedChanges(), ! model.song().parts.empty(), ! model.scratch().empty());
}

bool ArrangerPanel::usesPattern (const std::string& id) const
{
    const auto uses = [&] (const SongPart& part) { return part.kind == PartKind::Pattern && part.sourceId == id; };
    return std::any_of (model.song().parts.begin(), model.song().parts.end(), uses)
        || std::any_of (model.scratch().begin(), model.scratch().end(), uses);
}

void ArrangerPanel::libraryChanged()
{
    index.build (host.getLibrary());
    find.setIndex (&index);
    rebuildSequence();   // "source differs" markers follow the library
    updateDetail();
}

void ArrangerPanel::loadSong (const Song& s)
{
    confirmDiscard ([this, s]
    {
        Song copy = s;
        juce::String err;
        if (! embedCopies (copy, host.getLibrary().all(), err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Song laden", err);
            return;
        }
        model.load (copy);
        selectPart (-1);
    });
}

void ArrangerPanel::modelChanged()
{
    rebuildSequence();
    std::vector<bool> used;   // which Ablage blocks the sequence contains
    for (auto& s : model.scratch())
        used.push_back (std::any_of (model.song().parts.begin(), model.song().parts.end(),
                                     [&] (const SongPart& p) { return sameSource (p, s); }));
    scratch.setParts (model.scratch(), used);
    find.stopAllAuditionMarks();   // also refreshes the "in sequence" rings
    toolbar.update (juce::String (model.song().name), model.hasUnsavedChanges(), ! model.song().parts.empty(),
                    ! model.scratch().empty());
    if (sequence.getSelected() >= (int) model.song().parts.size())
        sequence.setSelected ((int) model.song().parts.size() - 1);
    updateDetail();
    stateDirty = true;
    stateDueMs = juce::Time::getMillisecondCounterHiRes() + 500.0;
    resized();   // the sequence grows with its rows (up to three) before it scrolls
    // only changes that alter what is played reach the transport (not a rename or an Ablage edit)
    Song playedOnly = model.song();
    playedOnly.name.clear();
    for (auto& p : playedOnly.parts)
        p.label.clear();
    const auto key = juce::JSON::toString (songToJson (playedOnly), true);
    if (key != lastSentKey)
    {
        lastSentKey = key;
        if (host.arrangerPlanLoaded() && buildError.isEmpty())
            host.arrangerUpdate (model.song());   // edits reach the running (or paused) playback
    }
}

void ArrangerPanel::rebuildSequence()
{
    spans.clear();
    built = {};
    buildError.clear();
    // Keep the blocks available for editing even if a restored song exceeds playback limits.
    if (! model.song().parts.empty())
        buildSongPattern (model.song(), host.getLibrary().all(), built, spans, buildError);
    sequence.setSpans (&spans);
    sequence.setErrorText (buildError);
    repaint();   // capacity indicator follows edits, undo/redo and loaded songs
}

void ArrangerPanel::updateDetail()
{
    const int p = sequence.getSelected();
    const auto& parts = model.song().parts;
    const SongPart* part = p >= 0 && p < (int) parts.size() ? &parts[(size_t) p] : nullptr;

    juce::String partName, sourceName;
    bool canUpdate = false;
    if (part != nullptr)
    {
        for (auto& sp : spans)
            if (sp.part == p) { partName = sp.label; canUpdate = sp.sourceDiffers; break; }
        if (part->copy)
            sourceName = juce::String (part->copy->name);
    }

    juce::String lengthText;
    if (buildError.isNotEmpty())
        lengthText = "Fehler: " + buildError;
    else if (! built.bars.empty())
    {
        const double seconds = built.lengthInQuarters().toDouble() * 60.0 / juce::jmax (20.0, built.tempo);
        lengthText << (int) built.bars.size() << " Takte  "_u << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "  ca. "
                   << (int) seconds / 60 << ":" << juce::String ((int) seconds % 60).paddedLeft ('0', 2) << " min bei " << (int) built.tempo << " BPM";
    }
    detail.update (part, partName, sourceName, lengthText, canUpdate);
}

void ArrangerPanel::timerCallback()
{
    // audition markers go out when the main window stops; the playhead follows the sequence
    if (! auditionKey.empty() && ! host.arrangerAuditionActive())
    {
        auditionKey.clear();
        find.stopAllAuditionMarks();
        scratch.repaint();
    }
    // the transport holds the sequence (or the saved song with the same content): show the position,
    // playing or paused, just like the player's strip keeps its marker while paused
    const auto& view = host.getPlaybackView();
    const bool ours = transportHoldsSequence();
    const bool hasPosition = ours && view.hasPosition();
    const bool playing = ours && view.live;
    const double pos = hasPosition ? view.position() : -1.0;
    sequence.setPlayPosition (pos);
    juce::String where;
    if (hasPosition)
        for (auto& sp : spans)
            if (pos >= sp.firstBar && pos < sp.firstBar + sp.numBars)
            {
                where << (playing ? "Spielt: " : "Pause: ") << sp.label << "   " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "   Takt "
                      << (int) (pos - sp.firstBar) + 1 << "/" << sp.numBars;
                if (sp.repeats > 1)
                    where << "   (" << sp.repeat + 1 << ". von " << sp.repeats << ")";
                const auto r = sequence.boundsOfPart (sp.part);   // keep the sounding block in view
                if (! r.isEmpty() && ! sequenceViewport.getViewArea().contains (r))
                    sequenceViewport.setViewPosition (0, juce::jmax (0, r.getY() - 4));
                break;
            }
    if (! playing && host.arrangerAuditionActive())
        where = "Spielt: Vorschau (Esc beendet)";
    detail.setPlaying (playing, playing || host.arrangerAuditionActive(), where);
    if (stateDirty && juce::Time::getMillisecondCounterHiRes() >= stateDueMs)
        saveState();
    if (pendingSeekPart >= 0 && juce::Time::getMillisecondCounterHiRes() >= seekDueMs)
    {
        const int part = pendingSeekPart;
        pendingSeekPart = -1;
        if (transportHoldsSequence())
            host.arrangerSeekToPart (part);
    }
}

//==============================================================================
std::vector<SongPart> ArrangerPanel::partsForIds (const std::vector<std::string>& ids) const
{
    std::vector<SongPart> parts;
    const auto& lib = host.getLibrary();
    for (auto& id : ids)
    {
        const int i = lib.indexOf (id);
        if (i < 0)
            continue;
        if (const auto* song = lib.song (i); song != nullptr)
        {
            // a whole song: its parts as copies (a template to start from)
            Song copy = *song;
            juce::String err;
            if (embedCopies (copy, lib.all(), err))
                parts.insert (parts.end(), copy.parts.begin(), copy.parts.end());
        }
        else
            parts.push_back (makePatternPart (lib.get (i)));
    }
    return parts;
}

std::vector<SongPart> ArrangerPanel::partsForDrag (const juce::var& description) const
{
    const auto s = description.toString();
    const auto payload = s.fromFirstOccurrenceOf (":", false, false);
    if (s.startsWith ("find:"))
    {
        juce::StringArray ids;
        ids.addTokens (payload, ",", "");
        std::vector<std::string> list;
        for (auto& id : ids)
            if (id.isNotEmpty())
                list.push_back (id.toStdString());
        return partsForIds (list);
    }
    if (s.startsWith ("scratch:") && payload.isNotEmpty() && payload.containsOnly ("0123456789"))
    {
        const int i = payload.getIntValue();
        if (i >= 0 && i < (int) model.scratch().size())
            return { model.scratch()[(size_t) i] };
    }
    return {};
}

bool ArrangerPanel::transportHoldsSequence() const
{
    if (host.arrangerPlanLoaded())
        return true;
    const auto& view = host.getPlaybackView();
    return ! model.song().id.empty() && host.currentPlanId() == "song." + model.song().id
        && view.spans != nullptr && view.spans->size() == spans.size()
        && view.plan != nullptr && view.plan->bars.size() == built.bars.size();
}

int ArrangerPanel::insertIndex() const
{
    const int p = sequence.getSelected();
    return p >= 0 && p < (int) model.song().parts.size() ? p + 1 : (int) model.song().parts.size();
}

void ArrangerPanel::addToSequence (std::vector<SongPart> parts, int index)
{
    if (parts.empty())
        return;
    const int n = (int) parts.size();
    index = juce::jlimit (0, (int) model.song().parts.size(), index);
    const auto previousSize = model.song().parts.size();
    model.insertParts (index, std::move (parts));
    if (model.song().parts.size() == previousSize)
        return;   // rejected: keep selection, viewport and keyboard focus unchanged
    selectPart (index + n - 1);
    sequence.grabKeyboardFocus();   // Ctrl+Z now undoes the insertion, not the search field's text
}

void ArrangerPanel::selectPart (int part)
{
    sequence.setSelected (part);
    updateDetail();
    const auto r = sequence.boundsOfPart (part);
    if (! r.isEmpty() && ! sequenceViewport.getViewArea().contains (r))
        sequenceViewport.setViewPosition (0, juce::jmax (0, r.getY() - 4));
}

void ArrangerPanel::layoutSequence()
{
    const int w = juce::jmax (10, sequenceViewport.getMaximumVisibleWidth());
    sequence.setSize (w, juce::jmax (sequenceViewport.getHeight(), sequence.preferredHeight (w)));
}

void ArrangerPanel::toggleAudition (const std::string& key, const SongPart& part)
{
    if (auditionKey == key && host.arrangerAuditionActive())
    {
        stopAudition();
        return;
    }
    auditionKey = key;
    host.arrangerAudition (part);
    find.stopAllAuditionMarks();
    scratch.repaint();
}

void ArrangerPanel::stopAudition()
{
    auditionKey.clear();
    host.arrangerStop();
    find.stopAllAuditionMarks();
    scratch.repaint();
    updateDetail();
}

void ArrangerPanel::playFrom (int part)
{
    if (model.song().parts.empty() || buildError.isNotEmpty())
        return;
    auditionKey.clear();
    host.arrangerPlay (model.song(), juce::jlimit (0, (int) model.song().parts.size() - 1, part));
    updateDetail();
}

//==============================================================================
void ArrangerPanel::askRange()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! part.copy)
        return;
    const int total = (int) part.copy->bars.size();
    const int from = part.numBars > 0 ? part.firstBar + 1 : 1;
    const int to = part.numBars > 0 ? part.firstBar + part.numBars : total;

    auto* w = new InWindowDialog ("Takte eingrenzen", "Welche Takte von \""_u + juce::String (part.copy->name) + "\" sollen gespielt werden? (1 bis "
                                     + juce::String (total) + ")", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("from", juce::String (from), "von Takt:");
    w->addTextEditor ("to", juce::String (to), "bis Takt:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Ganzes Pattern", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), w, uid, total] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        // the block is found by its identity, wherever it moved meanwhile; gone = nothing happens
        const int p = safe->model.indexOfUid (uid);
        if (p < 0 || ! safe->model.song().parts[(size_t) p].copy || (int) safe->model.song().parts[(size_t) p].copy->bars.size() != total)
            return;
        if (r == 2)
            safe->model.setRange (p, 0, 0);
        else
        {
            const int a = juce::jlimit (1, total, w->getTextEditorContents ("from").getIntValue());
            const int b = juce::jlimit (a, total, w->getTextEditorContents ("to").getIntValue());
            safe->model.setRange (p, a - 1, b - a + 1);
        }
    }), true);
}

void ArrangerPanel::editBlock()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size() || ! onEditInRaster)
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! part.copy)
        return;

    juce::String title;
    for (auto& sp : spans)
        if (sp.part == p) { title = sp.label; break; }

    // The edit comes back later. The block is found by its identity; if its copy changed in the
    // meantime (undo, "Aus Quelle"), the user decides instead of losing that silently.
    const auto uid = part.uid;
    const auto opened = part.copy;
    onEditInRaster (*part.copy, title, [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid, opened] (const hz::Pattern& edited)
    {
        if (safe == nullptr)
            return false;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "In Ablauf übernehmen"_u,
                                                    "Der Block ist nicht mehr im Ablauf. Nichts geändert."_u);
            return false;
        }
        if (safe->model.song().parts[(size_t) p].copy != opened)
        {
            // asked, not decided here: the raster keeps its edit until the answer arrives
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "In Ablauf übernehmen"_u,
                "Der Block wurde inzwischen im Arranger geändert (Rückgängig oder \"Aus Quelle\"). Trotzdem mit der Raster-Fassung ersetzen?"_u,
                "Ersetzen", "Abbrechen", safe,
                juce::ModalCallbackFunction::create ([safe, uid, edited] (int r)
                {
                    if (safe == nullptr || r != 1)
                        return;
                    if (const int q = safe->model.indexOfUid (uid); q >= 0)
                        safe->model.setCopy (q, edited, true);
                }));
            return false;
        }
        return safe->model.setCopy (p, edited, true);
    });
}

bool ArrangerPanel::editSelectedBlock()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size() || model.song().parts[(size_t) p].kind != PartKind::Pattern)
        return false;
    editBlock();
    return true;
}

void ArrangerPanel::showBlockMenu (int part)
{
    if (part < 0 || part >= (int) model.song().parts.size())
        return;
    const auto& p = model.song().parts[(size_t) part];
    bool canUpdate = false;
    for (auto& sp : spans)
        if (sp.part == part) { canUpdate = sp.sourceDiffers; break; }

    juce::PopupMenu menu;
    menu.addItem (7, "Umbenennen");
    menu.addItem (1, "Bearbeiten (Kopie im Raster)", p.kind == PartKind::Pattern && p.copy != nullptr);
    menu.addItem (2, "Duplizieren");
    menu.addItem (3, "Takte eingrenzen", p.kind == PartKind::Pattern && p.copy && p.copy->bars.size() > 1);
    menu.addItem (4, "Aus Quelle aktualisieren", canUpdate);
    menu.addItem (8, "In die Ablage legen");
    menu.addSeparator();
    menu.addItem (5, "Play ab hier");
    menu.addSeparator();
    menu.addItem (6, "Entfernen");
    const auto uid = p.uid;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sequence).withMousePosition(),
                        [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int part = safe->model.indexOfUid (uid);
        if (part < 0)
            return;
        safe->sequence.setSelected (part);
        switch (r)
        {
            case 1: safe->editBlock(); break;
            case 2: safe->addToSequence ({ safe->model.song().parts[(size_t) part] }, part + 1); break;
            case 3: safe->askRange(); break;
            case 4: safe->updateFromSource(); break;
            case 5: safe->playFrom (part); break;
            case 6: safe->model.removePart (part); safe->selectPart (juce::jmin (part, (int) safe->model.song().parts.size() - 1)); break;
            case 7: safe->askRename (part); break;
            case 8: safe->model.addToScratch ({ safe->model.song().parts[(size_t) part] }); break;
            default: break;
        }
    });
}

void ArrangerPanel::updateFromSource()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! sourceDiffers (part, host.getLibrary().all()))
        return;
    const auto all = model.partsWithSource (part.sourceId);
    juce::String sourceName (part.copy ? part.copy->name : part.sourceId);
    if (const int i = host.getLibrary().indexOf (part.sourceId); i >= 0)
        sourceName = juce::String (host.getLibrary().get (i).name);

    // the warning the user asked for: this replaces the copy, local edits included
    juce::String text = "Der Block wird durch die aktuelle Fassung von \""_u + sourceName + "\" aus der Bibliothek ersetzt.";
    if (part.edited)
        text << "\n\nAchtung: Die Änderungen, die du an diesem Block im Song gemacht hast, gehen dabei verloren."_u;
    text << "\n\n(Strg+Z macht es rückgängig.)"_u;

    auto* w = new InWindowDialog ("Aus Quelle aktualisieren", text, juce::MessageBoxIconType::WarningIcon, this);
    w->addButton ("Nur diesen Block", 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (all.size() > 1)
        w->addButton ("Alle " + juce::String ((int) all.size()) + " Bl" + juce::String (juce::CharPointer_UTF8 ("\xc3\xb6")) + "cke dieser Quelle", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto sourceId = part.sourceId;
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), uid, sourceId] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
            return;
        safe->model.updateFromSource (r == 2 ? safe->model.partsWithSource (sourceId) : std::vector<int> { p }, safe->host.getLibrary().all());
    }), true);
}

void ArrangerPanel::askRename (int p)
{
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    juce::String origin;
    for (auto& sp : spans)
        if (sp.part == p) { origin = sp.label; break; }
    const juce::String sourceName = part.copy ? juce::String (part.copy->name) : juce::String (part.sourceId);

    auto* w = new InWindowDialog ("Block umbenennen",
                                     "Name dieses Blocks im Ablauf. Das Pattern selbst (\"" + sourceName + "\") wird nicht umbenannt.\n"
                                     "Leer lassen = Pattern-Name anzeigen. Strg+Z macht es rückgängig."_u,
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", juce::String (part.label), "Name im Ablauf:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Pattern-Name verwenden", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), w, uid] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
            return;
        safe->model.setLabel (p, r == 2 ? std::string() : w->getTextEditorContents ("name").trim().toStdString());
    }), true);
}

void ArrangerPanel::save (bool asNew)
{
    if (model.song().parts.empty())
        return;
    Song copy = model.song();
    if (asNew || copy.id.empty())
    {
        auto* w = new InWindowDialog ("Song speichern", "Name des Songs:", juce::MessageBoxIconType::NoIcon, this);
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
        s.name = "Neuer Song";
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
        menu.addItem (1, "(keine gespeicherten Songs)", false);
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

//==============================================================================
void ArrangerPanel::saveState()
{
    juce::String err;
    Song s = model.song();
    if (s.name.empty())
        s.name = "Neuer Song";
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
            s.name = "Neuer Song";
            model.load (s);
        }
    }
    else
    {
        s.name = "Neuer Song";
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

//==============================================================================
void ArrangerPanel::resized()
{
    auto area = getLocalBounds().reduced (12);
    toolbar.setBounds (area.removeFromTop (buttonH));
    area.removeFromTop (10);
    hint.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (4);

    auto left = area.removeFromLeft (juce::jlimit (320, 460, area.getWidth() * 34 / 100));
    find.setBounds (left);
    area.removeFromLeft (14);

    detail.setBounds (area.removeFromBottom (26 + 4 + buttonH + 4));
    area.removeFromBottom (8);
    // the sequence grows with its rows, up to three; longer songs scroll inside
    const int seqH = juce::jlimit (SequenceView::heightForRows (1) + 2, SequenceView::heightForRows (3) + 2,
                                   sequence.preferredHeight (area.getWidth()) + 2);
    sequenceViewport.setBounds (area.removeFromBottom (seqH));
    layoutSequence();
    area.removeFromBottom (26);   // room for the "ABLAUF" heading
    scratch.setBounds (area);
}

void ArrangerPanel::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (fontSmall, juce::Font::bold));
    auto heading = juce::Rectangle<int> (sequenceViewport.getX(), sequenceViewport.getY() - 24,
                                         sequenceViewport.getWidth(), 20);
    g.drawText ("ABLAUF", heading.removeFromLeft (76), juce::Justification::centredLeft);
    const auto barArea = heading.removeFromLeft (100).toFloat().withSizeKeepingCentre (100.0f, 7.0f);
    size_t events = 0;
    for (const auto& layer : built.layers)
        events += layer.events.size();
    const double barUse = (double) built.bars.size() / hz::Pattern::maxBars;
    const double eventUse = (double) events / (double) hz::Pattern::maxEvents;
    const double used = juce::jmax (barUse, eventUse);
    const auto colour = used >= 1.0 ? juce::Colour (0xffe0443a)
                      : used >= 0.8 ? juce::Colour (0xffe89a2c) : playing;
    g.setColour (juce::Colour (0xff34343d));
    g.fillRoundedRectangle (barArea, 3.5f);
    if (used > 0.0)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (barArea.withWidth (barArea.getWidth() * (float) juce::jmin (1.0, used)), 3.5f);
    }
    heading.removeFromLeft (8);
    const auto remaining = barUse >= eventUse
        ? juce::String (juce::jmax (0, hz::Pattern::maxBars - (int) built.bars.size())) + " Takte frei"
        : juce::String (juce::jmax (0, (int) hz::Pattern::maxEvents - (int) events)) + " Schläge frei"_u;
    g.setColour (used >= 0.8 ? colour : textDim);
    g.setFont (juce::FontOptions (fontSmall));
    g.drawFittedText (juce::String ((int) (used * 100.0 + 0.5)) + " % belegt  ·  "_u + remaining,
                     heading, juce::Justification::centredLeft, 1);
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
