#include "app/AlbumIndex.h"

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
    if (const auto parsed = juce::JSON::parse (f.loadFileAsString(), root); parsed.failed())
        return juce::String::fromUTF8 ("kein gültiges JSON (") + parsed.getErrorMessage() + ")";
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
                    if (const auto k = key (titles->getReference (i).toString()); k.isNotEmpty())   // "???" names nothing
                        songs.emplace (k, place);
                }
            if (const auto* numbers = a["nummern"].getDynamicObject())   // single songs with their number
                for (const auto& n : numbers->getProperties())
                {
                    place.number = (int) n.value;
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
    return {};
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
    added = 0;
    if (! staging.existsAsFile())
        return {};
    const auto incoming = juce::JSON::parse (staging);
    const auto* bandsIn = incoming["bands"].getDynamicObject();
    if (bandsIn == nullptr)
        return staging.getFileName() + ": kein Objekt \"bands\"";
    juce::var root = own.existsAsFile() ? juce::JSON::parse (own) : juce::var();
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
    for (const auto& band : bandsIn->getProperties())
    {
        const auto name = band.name.toString();
        bool known = hasAlbums (name);   // the shipped list, or the own file as it was loaded ...
        for (const auto& o : ownBands->getProperties())
            known = known || key (o.name.toString()) == key (name);   // ... or as it is now
        if (known || key (name).isEmpty() || band.value.getArray() == nullptr)
            continue;
        ownBands->setProperty (band.name, band.value);
        ++added;
    }
    if (added == 0)
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
