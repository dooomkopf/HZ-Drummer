#include "app/Library.h"
#include "app/MenuFolderSlots.h"
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
    songFilesById.clear();
    auto songFiles = songFolder().findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*.json");
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
            // a duplicate id keeps the first (as the entries do below)
            spansById.emplace (pattern.id, std::move (spans));
            songsById.emplace (pattern.id, s);
            songFilesById.emplace (pattern.id, f);
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
    if (const auto error = albums.load (findAppFolder ("alben").getChildFile ("alben.json"),
                                        userPatternFolder().getParentDirectory().getChildFile ("alben.json")); error.isNotEmpty())
        loadErrors.add (error);   // names the file itself
    placeSongs();

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

juce::File Library::songFile (const std::string& songId) const
{
    const auto it = songFilesById.find ("song." + songId);
    return it != songFilesById.end() ? it->second : songFolder().getChildFile (juce::String (songId) + ".json");
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
    if (isSong (i))
    {
        // only the name changes in the song's own file: an old reference song (v1) stays one
        const auto file = songFilesById[id];
        auto json = juce::JSON::parse (file);
        auto* object = json.getDynamicObject();
        if (object == nullptr)
            return file.getFileName() + " ist nicht lesbar."_u;
        object->setProperty ("name", juce::String (name));
        juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
        if (! temp.getFile().replaceWithText (juce::JSON::toString (json)) || ! temp.overwriteTargetFileWithTemporary())
            return file.getFileName() + " ist nicht speicherbar."_u;
        reload();
        return {};
    }
    if (! isUserPattern (i))
        return "Bibliothek-Bausteine lassen sich nicht umbenennen.";

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

void Library::placeSongs()
{
    songPlaces.clear();
    std::map<std::string, const hz::Pattern*> byId;
    for (const auto& p : entries)
        byId.emplace (p.id, &p);
    for (const auto& [id, s] : songsById)
    {
        // the pattern parts vote with their (band, song); a part's own copy counts, else its source pattern
        std::vector<albumview::Vote> votes;
        for (const auto& part : s.parts)
        {
            if (part.kind != PartKind::Pattern)
                continue;
            const hz::Pattern* p = part.copy.get();
            if (p == nullptr)
                if (const auto it = byId.find (part.sourceId); it != byId.end())
                    p = it->second;
            if (p != nullptr && bandOf (*p) != otherBand)
                votes.push_back ({ bandOf (*p), groupOf (*p) });
        }
        if (const auto place = albumview::vote (votes))
            songPlaces[id] = *place;
    }
}

std::vector<albumview::Entry> Library::albumEntries (FolderTree::Area area) const
{
    std::vector<albumview::Entry> list;
    for (const auto& p : entries)
    {
        if (areaOf (p.id) != area)
            continue;
        albumview::Entry e;
        e.id = p.id;
        e.name = juce::String (p.name);
        if (area == FolderTree::Songs)
        {
            e.arrangement = true;
            if (const auto it = songPlaces.find (p.id); it != songPlaces.end())
                std::tie (e.band, e.song) = it->second;
        }
        else if (const auto band = bandOf (p); band != otherBand)
        {
            e.band = band;
            e.song = groupOf (p);
        }
        list.push_back (std::move (e));
    }
    return list;
}

FolderNode Library::folders (FolderTree::Area area) const
{
    if (viewMode == View::Album && area != FolderTree::Bundled)
        return albumview::build (albumEntries (area), albums);
    return ownFolders (area);
}

juce::StringArray Library::topLevelFolders (FolderTree::Area area) const
{
    juce::StringArray names;
    for (const auto& f : ownFolders (area).folders)
        names.add (f.name);
    if (area != FolderTree::Bundled)
        for (const auto& band : albumview::topLevel (albumEntries (area)))
            names.addIfNotAlreadyThere (band, true);
    return names;
}

juce::String Library::entryLabel (int index, const FolderPath& folder) const
{
    const auto& p = entries[(size_t) index];
    if (folder.isEmpty())
        return juce::String (p.name);
    auto label = folders::shortName (juce::String (p.name), folder);
    if (viewMode == View::Album && areaOf (p.id) == FolderTree::Songs && folder.size() == 2)   // in its album: with the number
        if (const auto it = songPlaces.find (p.id); it != songPlaces.end())
            if (const auto place = albums.find (it->second.first, it->second.second); place && place->number > 0)
                label = juce::String (place->number).paddedLeft ('0', 2) + ". " + label;
    return label;
}

FolderNode Library::ownFolders (FolderTree::Area area) const
{
    std::vector<FolderTree::Entry> list;
    for (int i = 0; i < size(); ++i)
    {
        const auto& p = entries[(size_t) i];
        if (areaOf (p.id) != area)
            continue;
        // songs: top level; own patterns: band, then song if they belong to one; bundled: style
        FolderPath place;
        if (area == FolderTree::Patterns && bandOf (p) != otherBand)   // without a band: at the top, easy to find again
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

juce::String Library::editFolders (FolderTree::Area area, const std::function<juce::String (FolderNode&)>& edit,
                                   const std::optional<std::pair<juce::String, juce::String>>& renamedTop)
{
    auto topLevel = [] (const FolderNode& node)
    {
        juce::StringArray names;
        for (const auto& f : node.folders)
            names.add (f.name);
        return names;
    };
    auto root = ownFolders (area);
    const auto before = topLevel (root);
    if (const auto error = edit (root); error.isNotEmpty())
        return error;
    const auto after = topLevel (root);
    // the bands on top of the album view: hidden ones stay hidden whatever happened to the own folders
    const auto albumTops = area == FolderTree::Bundled ? juce::StringArray() : albumview::topLevel (albumEntries (area));
    auto changed = folderTree;
    changed.followTopLevel (area, before, after, albumTops);   // System > Menü-Ordner: hidden folders keep up
    changed.set (area, std::move (root));
    if (const auto error = changed.save (folderFile()); error.isNotEmpty())
        return error;
    folderTree = std::move (changed);
    // a top-level folder renamed: the stored sets of Menü-Ordner follow. Said outright by the caller (then also
    // when it merged into an existing name), otherwise read off the names: one went, one came
    juce::String from, to;
    if (renamedTop)
        std::tie (from, to) = *renamedTop;
    else
    {
        juce::StringArray gone, came;
        for (const auto& name : before)
            if (! after.contains (name, true))
                gone.add (name);
        for (const auto& name : after)
            if (! before.contains (name, true))
                came.add (name);
        if (gone.size() == 1 && came.size() == 1)
            from = gone[0], to = came[0];
    }
    if (from.isNotEmpty() && to.isNotEmpty() && from != to)
        menufolderslots::followRename (area, from, to, albumTops.contains (from, true));   // a band keeps its name there
    return {};
}

juce::String Library::setHiddenFolders (const std::array<juce::StringArray, FolderTree::NumAreas>& hide)
{
    auto changed = folderTree;
    for (int a = 0; a < FolderTree::NumAreas; ++a)   // exactly these: names of folders gone just now do not stay hidden
        changed.setHiddenFolders ((FolderTree::Area) a, hide[(size_t) a]);
    if (const auto error = changed.save (folderFile()); error.isNotEmpty())
        return error;
    folderTree = std::move (changed);
    return {};
}

juce::String Library::setFolderHidden (FolderTree::Area area, const juce::String& name, bool hidden)
{
    auto changed = folderTree;
    changed.setHidden (area, name, hidden);
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

std::map<std::string, juce::String> Library::removeMany (const std::vector<std::string>& ids)
{
    auto result = removeOwnLibraryFiles (ids, userPatternFolder().getParentDirectory());
    bool any = false;
    for (const auto& [id, error] : result)
        if (error.isEmpty())
        {
            favorites.erase (id);
            any = true;
        }
    if (any)
        reload();
    return result;
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
        { "lernen", "Lernen / Beispiele" }, { "hardcore", "Hardcore / Crossover" }, { "groove", "Groove" },
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

int Library::installStartPackage()
{
    const auto patterns = userPatternFolder(), songs = songFolder();
    if (patterns.exists() || songs.exists())
        return 0;   // not the first start: the user's own data stays as it is
    const auto community = findAppFolder ("community"), start = findAppFolder ("startpaket");
    int copied = 0;
    auto copyAll = [&copied] (const juce::File& from, const juce::File& to)
    {
        if (! from.isDirectory())
            return;
        to.createDirectory();
        for (const auto& f : from.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*.json"))
            if (const auto target = to.getChildFile (f.getFileName()); ! target.exists() && f.copyFileTo (target))
                ++copied;
    };
    if (community.isDirectory())
        for (const auto& set : community.findChildFiles (juce::File::findDirectories, false))
        {
            copyAll (set, patterns);
            copyAll (set.getChildFile ("songs"), songs);
        }
    copyAll (start.getChildFile ("songs"), songs);
    if (const auto tree = start.getChildFile ("ordner.json"), target = patterns.getSiblingFile ("ordner.json");
        tree.existsAsFile() && ! target.exists() && tree.copyFileTo (target))
        ++copied;
    patterns.createDirectory();   // also without a package: from now on it is no first start any more
    songs.createDirectory();
    juce::Logger::writeToLog ("Erster Start: Startpaket mit " + juce::String (copied) + " Dateien in den Datenordner gelegt");
    return copied;
}
