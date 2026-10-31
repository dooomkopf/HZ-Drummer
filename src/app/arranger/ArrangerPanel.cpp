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

    juce::String defaultHint()
    {
        return "Klick in Liste: Play/Pause   ·   Drag 'n Drop in Ablage/Ablauf   ·   Rechtsklick im Ablauf: Lokale Umbenennung   ·   "
               "Strg+K Suche   ·   Strg+S Speichern   ·   Strg+Z/Y Rückgängig/Wiederholen   ·   Alt+←/→ Block verschieben   ·   +/-   ·   M Mixer   ·   E schließen   ·   Esc Stopp"_u;
    }

}

ArrangerPanel::ArrangerPanel (ArrangerHost& h) : host (h), hint ("hilfezeile.txt", defaultHint, fontHint, 1)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);   // holds the focus when no child does, so the shortcuts keep working
    for (auto* c : { (juce::Component*) &toolbar, (juce::Component*) &find, (juce::Component*) &scratch,
                     (juce::Component*) &sequenceViewport, (juce::Component*) &detail, (juce::Component*) &hint })
        addAndMakeVisible (*c);
    sequenceViewport.setViewedComponent (&sequence, false);
    sequenceViewport.setScrollBarsShown (true, false);
    sequence.setWantsKeyboardFocus (true);


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
    scratch.onContextMenu = [this] (int i) { showCardMenu (i); };

    // Ablauf
    sequence.onSelect   = [this] (int part) { selectPart (part); };
    sequence.onMove     = [this] (int from, int to) { model.movePart (from, to); selectPart (to); };
    sequence.onDrop     = [this] (const juce::var& d, int index) { addToSequence (partsForDrag (d), index); };
    sequence.onRepeats  = [this] (int part, int delta) { model.setRepeats (part, model.song().parts[(size_t) part].repeats + delta); };
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
    sequence.onSeek     = [this] (int bar) { if (transportHoldsSequence()) host.arrangerSeekToBar (bar); };

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
    // the files were renamed the same way, also where only an undo step holds the pattern now
    model.renameCopies (sourceId, oldName, newName);
    compareWithSavedFile();
}

void ArrangerPanel::songRenamed (const std::string& libraryId, const std::string& newName)
{
    if (libraryId != "song." + model.song().id)
        return;
    model.setName (newName);
    compareWithSavedFile();
}

void ArrangerPanel::compareWithSavedFile()
{
    // "unsaved" compares with what the song file holds now (an old reference file gets its copies first)
    Song onDisk;
    juce::String err;
    const auto saved = host.getLibrary().songFile (model.song().id);
    if (! model.song().id.empty() && saved.existsAsFile() && ::loadSong (saved, onDisk, err))
    {
        embedCopies (onDisk, host.getLibrary().all(), err);
        model.markSavedAs (onDisk);
    }
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

void ArrangerPanel::badgesChanged()
{
    libraryChanged();   // search index (Finden) and sequence
    modelChanged();     // Ablage cards
}

void ArrangerPanel::loadSong (const Song& s)
{
    confirmDiscard ([this, s]
    {
        Song copy = s;
        juce::String err;
        if (! embedCopies (copy, host.getLibrary().all(), err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Arrangement laden", err);
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
    g.setFont (juce::FontOptions (fontSmall));
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
