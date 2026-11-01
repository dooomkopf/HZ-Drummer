#include "app/AlbumIndex.h"

namespace
{
    // JUCE reads the first value of a text and ignores what follows; saving would drop that rest, so it must be blank
    bool endsCleanly (const juce::String& text)
    {
        int depth = 0;
        bool inString = false, escaped = false;
        auto p = text.getCharPointer();
        for (; ! p.isEmpty(); ++p)
        {
            const auto c = *p;
            if (inString)
            {
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    inString = false;
            }
            else if (c == '"')
                inString = true;
            else if (c == '{' || c == '[')
                ++depth;
            else if ((c == '}' || c == ']') && --depth == 0)
            {
                ++p;
                break;
            }
        }
        for (; ! p.isEmpty(); ++p)
            if (! juce::CharacterFunctions::isWhitespace (*p))
                return false;
        return true;
    }
}

juce::String AlbumIndex::key (const juce::String& text)
{
    // per code point, without String::replace (which compares UTF-8 by its first byte here): the common letters
    // with signs as their base letter, & as "and", then letters and digits only, lower case
    static const std::map<juce::juce_wchar, const char*> folded {
        { 0xe4, "a" }, { 0xc4, "a" }, { 0xf6, "o" }, { 0xd6, "o" }, { 0xfc, "u" }, { 0xdc, "u" }, { 0xdf, "ss" },
        { 0xe9, "e" }, { 0xc9, "e" }, { 0xe8, "e" }, { 0xc8, "e" }, { 0xea, "e" }, { 0xca, "e" }, { 0xeb, "e" }, { 0xcb, "e" },
        { 0xe1, "a" }, { 0xc1, "a" }, { 0xe0, "a" }, { 0xc0, "a" }, { 0xe2, "a" }, { 0xc2, "a" }, { 0xe5, "a" }, { 0xc5, "a" },
        { 0xe6, "ae" }, { 0xc6, "ae" }, { 0xf3, "o" }, { 0xd3, "o" }, { 0xf2, "o" }, { 0xd2, "o" }, { 0xf4, "o" }, { 0xd4, "o" },
        { 0xf8, "o" }, { 0xd8, "o" }, { 0xfa, "u" }, { 0xda, "u" }, { 0xf9, "u" }, { 0xd9, "u" }, { 0xfb, "u" }, { 0xdb, "u" },
        { 0xf1, "n" }, { 0xd1, "n" }, { 0xe7, "c" }, { 0xc7, "c" }, { 0xed, "i" }, { 0xcd, "i" }, { 0xec, "i" }, { 0xcc, "i" },
        { 0xee, "i" }, { 0xce, "i" }, { 0xef, "i" }, { 0xcf, "i" }, { 0xe3, "a" }, { 0xc3, "a" }, { 0xf5, "o" }, { 0xd5, "o" },
        { 0xfd, "y" }, { 0xdd, "y" }, { 0xff, "y" } };   // the same table as norm() in tools/import/alben_musicbrainz.py
    juce::String out;
    for (auto c : text)
    {
        if (c == '&')
        {
            out << "and";
            continue;
        }
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out << juce::String::charToString (c);
        else if (const auto it = folded.find (c); it != folded.end())
            out << it->second;
    }
    return out;
}

juce::String AlbumIndex::load (const juce::File& shipped, const juce::File& own)
{
    bands.clear();
    juce::StringArray errors;
    for (const auto* f : { &shipped, &own })
        if (f->existsAsFile())
            if (const auto error = read (*f, f == &own); error.isNotEmpty())
                errors.add ((f == &own ? "eigene " : "mitgelieferte ") + f->getFileName() + ": " + error);
    return errors.joinIntoString ("; ");
}

juce::String AlbumIndex::read (const juce::File& f, bool overrides)
{
    juce::var root;
    const auto text = f.loadFileAsString();
    if (const auto parsed = juce::JSON::parse (text, root); parsed.failed())
        return juce::String::fromUTF8 ("kein gültiges JSON (") + parsed.getErrorMessage() + ")";
    const auto rest = endsCleanly (text) ? juce::String()
                                         : juce::String::fromUTF8 ("Text nach der letzten Klammer wird nicht gelesen");
    const auto* list = root["bands"].getDynamicObject();
    if (list == nullptr)
        return "kein Objekt \"bands\"";
    for (const auto& band : list->getProperties())
    {
        const auto* albums = band.value.getArray();
        if (albums == nullptr)
            continue;
        std::map<juce::String, AlbumPlace> songs;   // of this file: the first album that names a song keeps it
        for (const auto& a : *albums)
        {
            AlbumPlace place;
            place.album = a["album"].toString().trim();
            place.year = (int) a["jahr"];
            place.live = (bool) a["live"];
            if (place.album.isEmpty())
                continue;
            if (const auto* titles = a["titel"].getArray())   // the whole track list: the position is the number
                for (int i = 0; i < titles->size(); ++i)
                {
                    place.number = i + 1;
                    place.listed = true;
                    if (const auto k = key (titles->getReference (i).toString()); k.isNotEmpty())   // "???" names nothing
                        songs.emplace (k, place);
                }
            if (const auto* numbers = a["nummern"].getDynamicObject())   // single songs with their number
                for (const auto& n : numbers->getProperties())
                {
                    place.number = (int) n.value;
                    place.listed = false;
                    if (const auto k = key (n.name.toString()); k.isNotEmpty())
                        songs.emplace (k, place);
                }
        }
        const auto bandKey = key (band.name.toString());
        if (bandKey.isEmpty())   // "!!!": no letter, no digit, it would match every such name
            continue;
        auto& known = bands[bandKey];
        for (const auto& [k, place] : songs)
            if (overrides)
                known[k] = place;      // the own file: its word counts
            else
                known.emplace (k, place);
    }
    return rest;
}

bool AlbumIndex::hasAlbums (const juce::String& band) const
{
    if (key (band).isEmpty())
        return false;
    const auto it = bands.find (key (band));
    return it != bands.end() && ! it->second.empty();
}

std::optional<AlbumPlace> AlbumIndex::find (const juce::String& band, const juce::String& song) const
{
    if (key (band).isEmpty())
        return {};
    const auto it = bands.find (key (band));
    if (it == bands.end())
        return {};
    const auto s = it->second.find (key (song));
    if (s == it->second.end() || key (song).isEmpty())
        return {};
    return s->second;
}

juce::String AlbumIndex::mergeNewBands (const juce::File& staging, const juce::File& own, int& added) const
{
    int songs = 0;
    return mergeAlbums (staging, own, {}, added, songs);
}

AlbumIndex::Songs AlbumIndex::importedSongs (const juce::File& patternFolder)
{
    Songs found;
    std::set<std::pair<juce::String, juce::String>> keys;
    for (const auto& f : patternFolder.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        juce::String band, song;
        const auto json = juce::JSON::parse (f);   // kept: the tags below live inside it
        if (const auto* tags = json["tags"].getArray())
            for (const auto& t : *tags)
            {
                const auto s = t.toString();
                if (s.startsWithIgnoreCase ("band:") && band.isEmpty()) band = s.substring (5).trim();
                if (s.startsWithIgnoreCase ("song:") && song.isEmpty()) song = s.substring (5).trim();
            }
        if (key (band).isNotEmpty() && key (song).isNotEmpty() && keys.insert ({ key (band), key (song) }).second)
            found.push_back ({ band, song });
    }
    return found;
}

juce::String AlbumIndex::mergeAlbums (const juce::File& staging, const juce::File& own, const Songs& imported,
                                      int& addedBands, int& addedSongs) const
{
    auto& added = addedBands;
    added = addedSongs = 0;
    if (! staging.existsAsFile())
        return {};
    const auto incoming = juce::JSON::parse (staging);
    const auto* bandsIn = incoming["bands"].getDynamicObject();
    if (bandsIn == nullptr)
        return staging.getFileName() + ": kein Objekt \"bands\"";
    juce::var root = own.existsAsFile() ? juce::JSON::parse (own) : juce::var();
    if (own.existsAsFile() && ! endsCleanly (own.loadFileAsString()))
        return own.getFileName() + juce::String::fromUTF8 (": Text nach der letzten Klammer, nichts ergänzt");   // stays as it is
    if (own.existsAsFile() && root.getDynamicObject() == nullptr)
        return own.getFileName() + juce::String::fromUTF8 (" ist kein JSON-Objekt, nichts ergänzt");
    if (own.existsAsFile() && root.hasProperty ("bands") && root["bands"].getDynamicObject() == nullptr)
        return own.getFileName() + juce::String::fromUTF8 (": \"bands\" ist kein Objekt, nichts ergänzt");   // a damaged file stays
    if (root.getDynamicObject() == nullptr)
    {
        root = juce::var (new juce::DynamicObject());
        root.getDynamicObject()->setProperty ("formatVersion", 1);
    }
    if (root.getDynamicObject()->getProperty ("bands").getDynamicObject() == nullptr)
        root.getDynamicObject()->setProperty ("bands", juce::var (new juce::DynamicObject()));
    auto* ownBands = root.getDynamicObject()->getProperty ("bands").getDynamicObject();
    const auto ownSpelling = [ownBands] (const juce::String& bandKey) -> juce::String   // the band as the own file writes it
    {
        for (const auto& o : ownBands->getProperties())
            if (key (o.name.toString()) == bandKey)
                return o.name.toString();
        return {};
    };
    std::set<juce::String> knownBefore, addedNow;   // keys: bands the lists knew before this merge, bands it brought
    for (const auto& band : bandsIn->getProperties())
    {
        const auto name = band.name.toString();
        const bool known = hasAlbums (name) || (ownSpelling (key (name)).isNotEmpty() && addedNow.count (key (name)) == 0);
        if (known)
            knownBefore.insert (key (name));
        if (known || addedNow.count (key (name)) > 0 || key (name).isEmpty() || band.value.getArray() == nullptr)
            continue;   // a second spelling of a band this merge brought: the first one stays
        ownBands->setProperty (band.name, band.value);
        addedNow.insert (key (name));
        ++added;
    }
    addedSongs = mergeSongs (*bandsIn, *ownBands, imported, knownBefore);
    if (added == 0 && addedSongs == 0)
        return {};
    juce::TemporaryFile temp (own, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk() || ! out.writeText (juce::JSON::toString (root), false, false, "\n"))
            return "kann nicht schreiben: " + own.getFullPathName();
        out.flush();
        if (out.getStatus().failed())
            return "kann nicht schreiben: " + own.getFullPathName();
    }
    return temp.overwriteTargetFileWithTemporary() ? juce::String() : "kann nicht schreiben: " + own.getFullPathName();
}

int AlbumIndex::mergeSongs (const juce::DynamicObject& bandsIn, juce::DynamicObject& ownBands, const Songs& imported,
                            const std::set<juce::String>& knownBefore) const
{
    const auto names = [] (const juce::var& album, const juce::String& songKey)   // the song's number there, -1: not there
    {
        if (const auto* titles = album["titel"].getArray())
            for (int i = 0; i < titles->size(); ++i)
                if (key (titles->getReference (i).toString()) == songKey)
                    return i + 1;
        if (const auto* numbers = album["nummern"].getDynamicObject())
            for (const auto& n : numbers->getProperties())
                if (key (n.name.toString()) == songKey)
                    return juce::jmax (0, (int) n.value);
        return -1;
    };
    int count = 0;
    std::set<std::pair<juce::String, juce::String>> done;
    for (const auto& [band, song] : imported)
    {
        const auto bandKey = key (band), songKey = key (song);
        if (knownBefore.count (bandKey) == 0 || songKey.isEmpty() || find (band, song) || ! done.insert ({ bandKey, songKey }).second)
            continue;
        juce::String spelling = band;
        const juce::Array<juce::var>* albumsIn = nullptr;
        for (const auto& b : bandsIn.getProperties())
            if (key (b.name.toString()) == bandKey)
                albumsIn = b.value.getArray();
        bool placed = false;   // the own file as it is now may hold the song already
        for (const auto& o : ownBands.getProperties())
            if (key (o.name.toString()) == bandKey)
            {
                spelling = o.name.toString();
                for (const auto& a : o.value.getArray() != nullptr ? *o.value.getArray() : juce::Array<juce::var>())
                    placed = placed || names (a, songKey) >= 0;
            }
        if (albumsIn == nullptr || placed)
            continue;
        juce::String album;
        int year = 0, number = -1;
        bool live = false;
        for (const auto& a : *albumsIn)   // earliest first, as the tool writes them: the first album that has it
            if (number < 0 && a["album"].toString().trim().isNotEmpty() && (number = names (a, songKey)) >= 0)
            {
                album = a["album"].toString().trim();
                year = (int) a["jahr"];
                live = (bool) a["live"];
            }
        if (number < 0)
            continue;   // on no album: it stays under "Sonstige"
        if (const auto it = bands.find (bandKey); it != bands.end())   // an album the lists know: its name and year
            for (const auto& [k, place] : it->second)
                if (key (place.album) == key (album))
                {
                    album = place.album;
                    year = place.year;
                    if (place.listed)
                        number = 0;   // not on that edition's track list: its number there is unknown
                }
        auto list = ownBands.getProperty (spelling);
        if (list.isVoid())
            list = juce::var (juce::Array<juce::var>());
        auto* entries = list.getArray();
        if (entries == nullptr)
            continue;   // a damaged entry of the band stays as it is
        juce::DynamicObject* entry = nullptr;
        for (auto& e : *entries)
            if (auto* o = e.getDynamicObject(); o != nullptr && entry == nullptr && o->getProperty ("album").toString() == album
                && (int) o->getProperty ("jahr") == year && (! o->hasProperty ("nummern") || o->getProperty ("nummern").getDynamicObject() != nullptr))
                entry = o;
        if (entry == nullptr)
        {
            entry = new juce::DynamicObject();
            entry->setProperty ("album", album);
            entry->setProperty ("jahr", year);
            if (live)
                entry->setProperty ("live", true);
            entries->add (juce::var (entry));
        }
        if (entry->getProperty ("nummern").getDynamicObject() == nullptr)
            entry->setProperty ("nummern", juce::var (new juce::DynamicObject()));
        entry->getProperty ("nummern").getDynamicObject()->setProperty (song, number);
        ownBands.setProperty (spelling, list);
        ++count;
    }
    return count;
}
