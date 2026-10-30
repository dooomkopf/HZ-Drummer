#include "app/Library.h"
#include "app/SongRename.h"

#include "app/KitLoader.h"
#include "app/PatternIO.h"
#include "app/Text.h"
#include "engine/MeterCatalog.h"

#include <algorithm>

namespace
{
    bool hasPrefix (const std::string& id, const char* prefix) { return juce::String (id).startsWith (prefix); }

    // order: bundled (by style), own patterns, songs
    int rank (const hz::Pattern& p) { return hasPrefix (p.id, "song.") ? 2 : hasPrefix (p.id, "user.") ? 1 : 0; }

    const juce::String enDash (juce::CharPointer_UTF8 (" \xe2\x80\x93 "));
}

juce::File Library::userPatternFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/HZ-Drummer/patterns");
}

juce::File Library::songFolder()
{
    return userPatternFolder().getSiblingFile ("songs");
}

juce::File Library::folderFile()
{
    return userPatternFolder().getSiblingFile ("ordner.json");
}

void Library::reload()
{
    loadErrors.clear();
    entries = loadPatternFolder (findAppFolder ("library"), loadErrors);
    for (auto& p : loadPatternFolder (userPatternFolder(), loadErrors))
        entries.push_back (std::move (p));

    // Songs are built from their parts; they reference patterns by id
    spansById.clear();
    songsById.clear();
    auto songFiles = songFolder().findChildFiles (juce::File::findFiles, false, "*.json");
    songFiles.sort();
    std::vector<hz::Pattern> built;
    for (auto& f : songFiles)
    {
        Song s;
        juce::String err;
        hz::Pattern pattern;
        std::vector<SongSpan> spans;
        if (loadSong (f, s, err) && buildSongPattern (s, entries, pattern, spans, err))
        {
            spansById[pattern.id] = std::move (spans);
            songsById[pattern.id] = s;
            built.push_back (std::move (pattern));
        }
        else
        {
            loadErrors.add (f.getFileName() + ": " + err);
        }
    }
    for (auto& p : built)
        entries.push_back (std::move (p));

    // duplicate ids would make selection, favorites and song references ambiguous: keep the first
    std::set<std::string> seen;
    entries.erase (std::remove_if (entries.begin(), entries.end(), [&] (const hz::Pattern& p)
    {
        if (seen.insert (p.id).second)
            return false;
        loadErrors.add ("Doppelte ID ignoriert: " + juce::String (p.id) + " (" + juce::String (p.name) + ")");
        return true;
    }), entries.end());

    std::stable_sort (entries.begin(), entries.end(), [] (const hz::Pattern& a, const hz::Pattern& b)
    {
        if (rank (a) != rank (b))
            return rank (a) < rank (b);
        const auto sa = a.tags.empty() ? std::string() : a.tags.front();
        const auto sb = b.tags.empty() ? std::string() : b.tags.front();
        return sa < sb;
    });

    if (const auto error = folderTree.load (folderFile()); error.isNotEmpty())
        loadErrors.add (error);

    if (onReloaded)
        onReloaded (entries);
}

int Library::indexOf (const std::string& id) const
{
    for (int i = 0; i < size(); ++i)
        if (entries[(size_t) i].id == id)
            return i;
    return -1;
}

bool Library::isSong (int index) const        { return index >= 0 && index < size() && hasPrefix (entries[(size_t) index].id, "song."); }
bool Library::isUserPattern (int index) const { return index >= 0 && index < size() && hasPrefix (entries[(size_t) index].id, "user."); }

const std::vector<SongSpan>* Library::songSpans (int index) const
{
    if (! isSong (index))
        return nullptr;
    auto it = spansById.find (entries[(size_t) index].id);
    return it != spansById.end() ? &it->second : nullptr;
}

const Song* Library::song (int index) const
{
    if (! isSong (index))
        return nullptr;
    auto it = songsById.find (entries[(size_t) index].id);
    return it != songsById.end() ? &it->second : nullptr;
}

juce::String Library::save (hz::Pattern& p, bool asNew)
{
    auto folder = userPatternFolder();
    if (! folder.createDirectory())
        return "Ordner nicht anlegbar: "_u + folder.getFullPathName();

    // work on a copy: p only changes on success
    auto candidate = p;
    if (asNew || ! hasPrefix (candidate.id, "user."))
    {
        // remember where it came from (decision E3: copy with origin reference)
        if (! candidate.id.empty())
        {
            candidate.origin.parentId = candidate.id;
            candidate.origin.parentRevision = candidate.revision;
        }

        juce::String slug;
        for (auto c : juce::String (candidate.name).toLowerCase())
            slug << (c < 128 && juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String ("-"));
        slug = slug.trimCharactersAtStart ("-").trimCharactersAtEnd ("-").substring (0, 40);

        do
            candidate.id = ("user." + slug + "-" + juce::Uuid().toString().substring (0, 12)).toStdString();
        while (folder.getChildFile (juce::String (candidate.id) + ".json").exists());
        candidate.revision = 1;
    }
    else
    {
        ++candidate.revision;
    }

    if (auto v = candidate.validate(); ! v.empty())
        return juce::String (v);

    juce::String err;
    if (! savePattern (candidate, folder.getChildFile (juce::String (candidate.id) + ".json"), err))
        return err;

    p = candidate;
    reload();
    return {};
}

juce::String Library::rename (const std::string& id, const juce::String& newName)
{
    const int i = indexOf (id);
    const auto name = newName.trim().toStdString();
    if (i < 0)
        return "Den Eintrag gibt es nicht mehr.";
    if (name.empty())
        return "Der Name darf nicht leer sein.";
    if (name == entries[(size_t) i].name)
        return {};

    juce::String error;
    if (const auto* s = song (i))
    {
        auto renamed = *s;
        renamed.name = name;
        if (! saveSong (renamed, songFolder().getChildFile (juce::String (renamed.id) + ".json"), error))
            return error;
        reload();
        return {};
    }
    if (! isUserPattern (i))
        return "Mitgelieferte Patterns lassen sich nicht umbenennen.";

    auto p = entries[(size_t) i];
    const auto oldName = p.name;
    p.name = name;
    if (error = save (p, false); error.isNotEmpty())
        return error;
    error = renameCopiesInFiles (songFolder(), songFolder().getSiblingFile ("arranger.json"), songFolder().getSiblingFile ("ablage.json"),
                                 id, oldName, name);
    reload();
    return error;
}

FolderNode Library::folders (FolderTree::Area area) const
{
    std::vector<FolderTree::Entry> list;
    for (int i = 0; i < size(); ++i)
    {
        const auto& p = entries[(size_t) i];
        if (areaOf (p.id) != area)
            continue;
        // songs: top level; own patterns: band, then song if they belong to one; bundled: style
        FolderPath place;
        if (area == FolderTree::Patterns)
        {
            place.add (bandOf (p));
            if (const auto group = groupOf (p); group.isNotEmpty())
                place.add (group);
        }
        else if (area == FolderTree::Bundled)
        {
            place.add (styleTitle (p.tags.empty() ? std::string ("sonstige") : p.tags.front()));
        }
        list.push_back ({ p.id, place });
    }
    return folderTree.resolve (area, list);
}

juce::String Library::editFolders (FolderTree::Area area, const std::function<juce::String (FolderNode&)>& edit)
{
    auto root = folders (area);
    if (const auto error = edit (root); error.isNotEmpty())
        return error;
    auto changed = folderTree;
    changed.set (area, std::move (root));
    if (const auto error = changed.save (folderFile()); error.isNotEmpty())
        return error;
    folderTree = std::move (changed);
    return {};
}

juce::String Library::remove (const hz::Pattern& p)
{
    if (const auto error = removeOwnLibraryFile (p.id, userPatternFolder().getParentDirectory()); error.isNotEmpty())
        return error;

    favorites.erase (p.id);
    reload();
    return {};
}

void Library::toggleFavorite (const std::string& id)
{
    if (! favorites.erase (id))
        favorites.insert (id);
}

juce::String Library::favoritesToString() const
{
    juce::StringArray a;
    for (auto& f : favorites)
        a.add (f);
    return a.joinIntoString (";");
}

void Library::favoritesFromString (const juce::String& s)
{
    favorites.clear();
    for (auto& id : juce::StringArray::fromTokens (s, ";", ""))
        if (id.isNotEmpty())
            favorites.insert (id.toStdString());
}

juce::String Library::styleTitle (const std::string& tag)
{
    static const std::map<std::string, const char*> titles {
        { "d-beat", "D-Beat" }, { "thrash", "Thrash" }, { "death", "Death Metal" }, { "grind", "Grind / Blast" },
        { "tech-death", "Tech-Death" }, { "doom", "Doom / Sludge" }, { "prog", "Prog / ungerade" }, { "jazz", "Jazz / Fusion" },
        { "lernen", "Lernen / Beispiele" }, { "hardcore", "Hardcore / Crossover" },
    };
    auto it = titles.find (tag);
    return it != titles.end() ? juce::String (juce::CharPointer_UTF8 (it->second)) : juce::String (tag);
}

juce::String Library::meterText (const hz::Pattern& p)
{
    juce::StringArray m;
    for (auto& b : p.bars)
        if (m.isEmpty() || m[m.size() - 1] != juce::String (hz::meterLabel (b.meter)))
            m.add (hz::meterLabel (b.meter));
    if (m.size() > 3)
    {
        m.removeDuplicates (false);   // long songs: list each meter once
        m.removeRange (4, m.size());
    }
    return m.joinIntoString (" + ");
}

juce::String Library::bandOf (const hz::Pattern& p)
{
    for (const auto& tag : p.tags)
    {
        const juce::String value (tag);
        if (value.startsWithIgnoreCase ("band:") && value.substring (5).trim().isNotEmpty())
            return value.substring (5).trim();
    }
    return otherBand;
}

juce::String Library::groupOf (const hz::Pattern& p)
{
    for (const auto& tag : p.tags)
    {
        const juce::String value (tag);
        if (value.startsWithIgnoreCase ("song:") && value.substring (5).trim().isNotEmpty())
            return value.substring (5).trim();
    }
    const juce::String name (p.name);
    const auto dash = name.indexOf (enDash);
    return dash > 0 ? name.substring (0, dash) : juce::String();
}
