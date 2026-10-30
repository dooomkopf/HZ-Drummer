#pragma once

#include "app/FolderTree.h"
#include "app/Song.h"
#include "engine/Pattern.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <map>
#include <set>
#include <vector>

// All playable content: bundled patterns, the user's own patterns and song structures.
// Songs appear as entries too (their parts concatenated), so that every entry can be played,
// shown and edited the same way. Identity is the pattern id; indices change on reload.
//
// Id conventions: "lib.*" bundled (read-only), "user.*" own/imported patterns, "song.*" songs.
class Library
{
public:
    // Loads everything from disk. Entries with duplicate ids are rejected (reported in errors()).
    void reload();
    std::function<void (const std::vector<hz::Pattern>&)> onReloaded;   // after every reload (also after save/remove)

    int size() const                               { return (int) entries.size(); }
    const hz::Pattern& get (int index) const       { return entries[(size_t) index]; }
    const std::vector<hz::Pattern>& all() const    { return entries; }
    int indexOf (const std::string& id) const;     // -1 if unknown

    bool isSong (int index) const;
    bool isUserPattern (int index) const;
    const std::vector<SongSpan>* songSpans (int index) const;   // parts/repeats of a song, else nullptr
    const Song* song (int index) const;                         // the song structure, else nullptr
    juce::File songFile (const std::string& songId) const;      // the file it was loaded from ("" if none: <id>.json)
    const juce::StringArray& errors() const        { return loadErrors; }

    // Own patterns: save (new id or next revision) and delete. Return "" on success.
    juce::String save (hz::Pattern&, bool asNew);
    juce::String remove (const hz::Pattern&);
    // Own pattern or song: a new name. A pattern's copies in songs, the arranger's working state
    // and the Ablage follow while they still carry the old name (see SongRename). "" on success.
    juce::String rename (const std::string& id, const juce::String& newName);

    // Favorites (by id), persisted by the caller
    bool isFavorite (const std::string& id) const  { return favorites.count (id) > 0; }
    void toggleFavorite (const std::string& id);
    juce::String favoritesToString() const;
    void favoritesFromString (const juce::String&);

    // Folders of songs, own patterns and the bundled library (System → Ordnerstruktur); edits are saved at once
    static FolderTree::Area areaOf (const std::string& id)
    {
        const juce::String s (id);
        return s.startsWith ("song.") ? FolderTree::Songs : s.startsWith ("user.") ? FolderTree::Patterns : FolderTree::Bundled;
    }
    FolderNode folders (FolderTree::Area) const;             // every entry of the area in its place
    juce::String editFolders (FolderTree::Area, const std::function<juce::String (FolderNode&)>& edit);   // "" on success

    static juce::File userPatternFolder();   // ~/.config/HZ-Drummer/patterns
    static juce::File songFolder();          // ~/.config/HZ-Drummer/songs
    static juce::File folderFile();          // ~/.config/HZ-Drummer/ordner.json

    // Display helpers
    static juce::String styleTitle (const std::string& tag);   // "thrash" -> "Thrash"
    static juce::String meterText (const hz::Pattern&);         // "4/4 + 12/8 (3+3+3+3)"
    static constexpr const char* otherBand = "Sonstige";
    static juce::String bandOf (const hz::Pattern&);            // band: metadata, else otherBand
    static juce::String groupOf (const hz::Pattern&);           // "Carcass CJQ – 01 Intro" -> "Carcass CJQ", "" if none

private:
    std::vector<hz::Pattern> entries;
    std::map<std::string, std::vector<SongSpan>> spansById;
    std::map<std::string, Song> songsById;
    std::map<std::string, juce::File> songFilesById;   // where each song was loaded from
    std::set<std::string> favorites;
    FolderTree folderTree;
    juce::StringArray loadErrors;
};

// Remove a user pattern or saved song from a data folder; protects unresolved references.
juce::String removeOwnLibraryFile (const std::string& libraryId, const juce::File& dataFolder);

struct LibraryTrashEntry
{
    juce::String key, libraryId, name, deletedAt;
    bool song = false;
};
std::vector<LibraryTrashEntry> listOwnTrash (const juce::File& dataFolder);
juce::String restoreOwnTrash (const juce::String& key, const juce::File& dataFolder);
