#pragma once

#include "app/ArrangerModel.h"
#include "app/SearchIndex.h"
#include "app/arranger/ArrangerBars.h"
#include "app/arranger/ArrangerHost.h"
#include "app/arranger/FindView.h"
#include "app/arranger/ScratchView.h"
#include "app/arranger/ArrangerStyle.h"
#include "app/arranger/SequenceView.h"
#include "app/views/HelpLine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <string>
#include <vector>

// The "Arranger" tab: Finden (left), Ablage (top right), Ablauf (bottom right), toolbar and
// block detail row. Owns the model and the search index, wires the views, handles drag and
// drop between them and the keyboard. One working state: the song being arranged and the
// Ablage are written to ~/.config/HZ-Drummer (arranger.json, ablage.json) on every change and
// restored when the arranger opens again, so unsaved work survives closing the program.
class ArrangerPanel : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    // Set by the workshop window: shows a block's copy in the raster tab; 'apply' takes the edit back
    std::function<void (const hz::Pattern&, const juce::String& title, std::function<bool (const hz::Pattern&)> apply)> onEditInRaster;
    std::function<void (const hz::Pattern&)> onBlockApplied;   // 'apply' said "not yet" and asked; the answer was yes
    // a card of the Ablage into the raster: like onEditInRaster; savedAsNew gets an own pattern saved from it
    std::function<void (const hz::Pattern&, const juce::String& title, std::function<bool (const hz::Pattern&)> apply,
                        std::function<void (const hz::Pattern&)> savedAsNew)> onEditCardInRaster;

    explicit ArrangerPanel (ArrangerHost&);
    ~ArrangerPanel() override;

    void loadSong (const Song&);      // e.g. "Bearbeiten" on a song in the main window
    bool editSelectedBlock();         // opens the selected block in the raster; false if there is none
    juce::String songName() const     { return juce::String (model.song().name); }
    void libraryChanged();            // patterns/songs changed: rebuild the search index
    bool hasUnsavedChanges() const    { return model.hasUnsavedChanges(); }
    bool usesPattern (const std::string& id) const;
    void songDeleted (const std::string& libraryId);
    void patternRenamed (const std::string& sourceId, const std::string& oldName, const std::string& newName);
    void songRenamed (const std::string& libraryId, const std::string& newName);
    void focusSearch()                { find.focusField(); }

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void modelChanged();
    void compareWithSavedFile();   // after a rename on disk: what counts as saved
    void rebuildSequence();
    void updateDetail();

    // building blocks from the views' identifiers
    std::vector<SongPart> partsForIds (const std::vector<std::string>& ids) const;
    std::vector<SongPart> partsForDrag (const juce::var& description) const;

    void toggleAudition (const std::string& key, const SongPart&);
    void stopAudition();
    void playFrom (int part);
    int insertIndex() const;                  // after the selected block, else at the end
    bool transportHoldsSequence() const;      // the main window plays/pauses this sequence (or the same saved song)
    void addToSequence (std::vector<SongPart>, int index);
    void selectPart (int part);               // selects and scrolls it into view
    void layoutSequence();
    void askRange();
    void askRename (int part);
    void editBlock();
    void editScratchCard (int index);   // "Bearbeiten (Kopie im Editor)" on a card of the Ablage
    void showCardMenu (int index);      // right click on a card of the Ablage
    void updateFromSource();
    void showBlockMenu (int part);
    void save (bool asNew);
    void newSong();
    void showLoadMenu();
    void confirmDiscard (std::function<void()> then);   // asks when the song is unsaved

    void saveState();
    void loadState();
    void saveScratch();

    ArrangerHost& host;
    ArrangerModel model;
    SearchIndex index;
    arrangerstyle::Look look;

    ArrangerToolbar toolbar;
    FindView find;
    ScratchView scratch;
    juce::Viewport sequenceViewport;   // long songs scroll instead of shrinking
    SequenceView sequence;
    BlockDetailRow detail;
    HelpLine hint;                     // own text: hilfezeile.txt in the data folder

    hz::Pattern built;                 // the sequence as one pattern (length, spans)
    std::vector<SongSpan> spans;
    juce::String buildError;
    std::string auditionKey;           // "find:<id>" or "scratch:<index>" while auditioning
    juce::String lastSentKey;          // content of the sequence last handed to the transport
    std::optional<SongPart> copiedBlock;   // Ctrl+C/X in the sequence; kept across songs, not saved
    int pendingSeekPart = -1;          // a click on a block seeks after a short delay (a double-click must not)
    double seekDueMs = 0.0;
    bool stateDirty = false;
    double stateDueMs = 0.0;
};
