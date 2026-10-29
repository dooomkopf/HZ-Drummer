#pragma once

#include "app/Song.h"

#include <functional>
#include <vector>

// The song being arranged and the "Ablage" (collected candidates), with undo/redo.
// Every edit of either is one undo step; selection and auditioning are view state and not
// part of the history. The Ablage is independent of the song: it persists across songs and
// sessions (the owner saves it on onScratchChange) and is only emptied by clearScratch().
//
// "Unsaved" compares the song's content with what was last saved (or loaded), so undoing
// back to the saved state counts as saved again. The song's id is its file identity and is
// not part of the history: saving under a new id renames every snapshot. Runs on the
// message thread.
class ArrangerModel
{
public:
    // Return an error before committing a song edit; rejection leaves history and state intact.
    std::function<juce::String (const Song&)> validateSong;
    std::function<void (const juce::String&)> onRejected;
    std::function<void()> onChange;          // after every edit, undo, redo and load
    std::function<void()> onScratchChange;   // the Ablage changed: save it

    void load (Song);                         // new song, fresh history; counts as saved
    void loadScratch (std::vector<SongPart>); // at startup, no undo step
    const Song& song() const                  { return current.song; }
    const std::vector<SongPart>& scratch() const { return current.scratch; }
    bool hasUnsavedChanges() const;
    static juce::String contentKey (const Song&);   // the song without its file id, as text: what "unsaved" compares
    void markSavedAs (const Song& saved);     // what is on disk now (compare base), no undo step
    void markUnsaved();                       // nothing on disk matches (e.g. the file is gone)
    void adoptSaved (const Song& saved);      // just written under saved.id: renames all snapshots, counts as saved

    // Sequence (Song::parts)
    void insertPart (int index, SongPart);              // index clamped; size() appends
    void insertParts (int index, std::vector<SongPart>); // one undo step
    void removePart (int index);
    void movePart (int from, int to);                   // 'to' = target index after removal
    void setRepeats (int index, int repeats);           // 1..64
    void setRange (int index, int firstBar, int numBars);   // numBars 0 = whole pattern
    int indexOfUid (const std::string& uid) const;      // -1 if the block is gone
    void setLabel (int index, const std::string&);
    bool setCopy (int index, const hz::Pattern&, bool edited);           // a local variant of the block
    int updateFromSource (const std::vector<int>& indices, const std::vector<hz::Pattern>& library);   // returns how many changed
    std::vector<int> partsWithSource (const std::string& sourceId) const;
    void setName (const std::string&);
    void setTempo (double bpm);                         // 0 = tempo of the first part

    // Ablage; the same building block is kept only once
    int addToScratch (std::vector<SongPart>);   // returns how many were new (one undo step)
    void removeFromScratch (int index);
    void clearScratch();

    bool canUndo() const                      { return ! undoStack.empty(); }
    bool canRedo() const                      { return ! redoStack.empty(); }
    void undo();
    void redo();

    static constexpr int maxUndo = 200;

private:
    struct State
    {
        Song song;
        std::vector<SongPart> scratch;
    };

    bool validPart (int index) const          { return index >= 0 && index < (int) current.song.parts.size(); }
    bool commit (State next, bool scratchChanged);
    void restore (State);

    State current;
    juce::String savedKey;                    // contentKey of the saved version
    std::vector<State> undoStack, redoStack;
};

// A part that plays the whole pattern, with its own copy (format v2)
SongPart makePatternPart (const hz::Pattern&);


