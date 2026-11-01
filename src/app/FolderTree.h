#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <string>
#include <vector>

// The user's own folders for songs, own patterns and the bundled library (System → Ordnerstruktur),
// one tree per area, entries referred to by id. Stored in ordner.json next to the patterns; the
// pattern and song files themselves stay untouched. An entry that sits in no folder yet goes to
// its default place (own patterns: band, then song; bundled ones: their style), and folders of the
// same name merge, so the tree starts out like the fixed menu it replaces. The first edit stores
// the whole tree of that area.
using FolderPath = juce::StringArray;   // folder names from the top down; empty = top level

struct FolderNode
{
    juce::String name;
    std::vector<FolderNode> folders;   // in display order (shown before the entries)
    std::vector<std::string> items;    // entry ids in display order
};

class FolderTree
{
public:
    enum Area { Songs, Patterns, Bundled, NumAreas };
    struct Entry { std::string id; FolderPath defaultPath; };

    // The area's tree with exactly the given entries: stored ones in their place, the rest at
    // their default path (new folders in alphabetical order), ids of entries that are gone left out
    FolderNode resolve (Area, const std::vector<Entry>&) const;
    void set (Area area, FolderNode root)   { roots[(size_t) area] = std::move (root); }

    // Top-level folders the main menu leaves out (System → Menü-Ordner); stored with the tree. Names compare
    // without case. The search and the arranger still know every entry.
    bool isHidden (Area area, const juce::String& folder) const   { return hidden[(size_t) area].contains (folder, true); }
    void setHiddenFolders (Area area, const juce::StringArray& folders)   { hidden[(size_t) area] = folders; }   // exactly these
    void setHidden (Area, const juce::String& folder, bool hide);
    // after one edit of the tree, with its top-level folder names before and after: a renamed folder stays
    // hidden under its new name (one name went, one came), names no longer at the top are forgotten;
    // keep: names on top in the other view (AlbumView), they stay whatever the edit did
    void followTopLevel (Area, const juce::StringArray& before, const juce::StringArray& after, const juce::StringArray& keep = {});

    // A missing file means no folders yet. A broken one is left as it is: saving is refused then.
    juce::String load (const juce::File&);
    juce::String save (const juce::File&) const;

private:
    std::array<FolderNode, NumAreas> roots;
    std::array<juce::StringArray, NumAreas> hidden;
    bool unreadable = false;
};

// Edits of a resolved tree. They return "" on success, otherwise why not (the tree is unchanged).
// Names compare without case. An index of -1 appends; otherwise it counts as in the list before the move.
namespace folders
{
    const FolderNode* find (const FolderNode& root, const FolderPath&);
    bool locate (const FolderNode& root, const std::string& id, FolderPath& folder);   // where the entry is
    int countItems (const FolderNode&);                                                // entries in it and below
    bool startsWith (const FolderPath& path, const FolderPath& prefix);

    juce::String addFolder (FolderNode& root, const FolderPath& parent, const juce::String& name);
    juce::String renameFolder (FolderNode& root, const FolderPath&, const juce::String& name);   // onto an existing name: merged
    juce::String removeFolder (FolderNode& root, const FolderPath&);   // only one without entries
    juce::String moveFolder (FolderNode& root, const FolderPath&, const FolderPath& newParent, int index = -1);
    juce::String moveItem (FolderNode& root, const std::string& id, const FolderPath& target, int index = -1);
    bool shiftFolder (FolderNode& root, const FolderPath&, int delta);   // up/down among its sibling folders
    bool shiftItem (FolderNode& root, const std::string& id, int delta);

    // "Carcass – CJQ – 01 Intro" in Carcass/CJQ -> "01 Intro": what the folders already say is left out
    juce::String shortName (const juce::String& name, const FolderPath& folder);
}
