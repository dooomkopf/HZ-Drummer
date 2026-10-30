#include "app/Song.h"

#include "app/PatternIO.h"
#include "app/Text.h"
#include "app/BadgeRegistry.h"
#include "app/views/SourceBadge.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"

#include <algorithm>
#include <map>

namespace
{
    const hz::Pattern* findPattern (const std::vector<hz::Pattern>& library, const std::string& id)
    {
        for (auto& p : library)
            if (p.id == id)
                return &p;
        return nullptr;
    }

    juce::String kindName (PartKind k)   { return k == PartKind::Rest ? "rest" : k == PartKind::Click ? "click" : "pattern"; }



    //==========================================================================
    // JSON

    bool partFromJson (const juce::var& v, int version, SongPart& part, juce::String& error)
    {
        part = {};
        part.label = v["label"].toString().toStdString();
        part.repeats = juce::jlimit (1, 64, v.hasProperty ("repeats") ? (int) v["repeats"] : 1);
        part.edited = (bool) v["edited"];
        part.uid = v["uid"].toString().toStdString();
        if (part.uid.empty())
            part.uid = newBlockUid();   // older files: give every block an identity

        if (version == 1)
        {
            part.sourceId = v["pattern"].toString().toStdString();   // reference into the library
            if (part.sourceId.empty())
            {
                error = "Teil ohne Pattern";
                return false;
            }
            return true;
        }

        const auto type = v["type"].toString();
        if (type == "rest" || type == "click")
        {
            part.kind = type == "rest" ? PartKind::Rest : PartKind::Click;
            part.restBars = juce::jlimit (1, 64, v.hasProperty ("count") ? (int) v["count"] : 1);
            if (! hz::parseMeter (v["meter"].toString().toStdString(), part.meter))
            {
                error = "ungültige Taktart \"" + v["meter"].toString() + "\""_u;
                return false;
            }
            return true;
        }
        if (type != "pattern")
        {
            error = "unbekannter Teil-Typ \"" + type + "\""_u;
            return false;
        }

        part.sourceId = v["source"].toString().toStdString();
        if (v["copy"].isObject())
        {
            hz::Pattern copy;
            if (! patternFromJson (v["copy"], copy, error))
                return false;
            part.copy = std::make_shared<const hz::Pattern> (std::move (copy));
        }
        else if (part.sourceId.empty())
        {
            error = "Pattern-Teil ohne Kopie und ohne Quelle";
            return false;
        }

        // "bars": [from, to], 1-based and inclusive in the file
        if (auto* range = v["bars"].getArray(); range != nullptr)
        {
            const int from = range->size() > 0 ? (int) (*range)[0] : 0;
            const int to = range->size() > 1 ? (int) (*range)[1] : from;
            const int available = part.copy ? (int) part.copy->bars.size() : 1 << 20;
            if (from < 1 || to < from || to > available)
            {
                error = "ungültiger Taktbereich "_u + juce::String (from) + "–"_u + juce::String (to);
                return false;
            }
            part.firstBar = from - 1;
            part.numBars = to - from + 1;
        }
        return true;
    }

    juce::var partToJson (const SongPart& part)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("uid", juce::String (part.uid));
        o->setProperty ("type", kindName (part.kind));
        if (! part.label.empty())
            o->setProperty ("label", juce::String (part.label));
        o->setProperty ("repeats", part.repeats);

        if (part.kind != PartKind::Pattern)
        {
            o->setProperty ("meter", juce::String (hz::meterLabel (part.meter)));
            o->setProperty ("count", part.restBars);
            return juce::var (o);
        }

        o->setProperty ("source", juce::String (part.sourceId));
        if (part.edited)
            o->setProperty ("edited", true);
        if (part.numBars > 0)
            o->setProperty ("bars", juce::Array<juce::var> { part.firstBar + 1, part.firstBar + part.numBars });
        if (part.copy)
            o->setProperty ("copy", patternToJson (*part.copy));
        return juce::var (o);
    }

    // atomic: a crash while writing never leaves a half file behind
    bool writeJson (const juce::var& root, const juce::File& file, juce::String& error)
    {
        juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
        if (! temp.getFile().replaceWithText (juce::JSON::toString (root)) || ! temp.overwriteTargetFileWithTemporary())
        {
            error = "Kann nicht speichern: " + file.getFullPathName();
            return false;
        }
        return true;
    }
}

//==============================================================================
bool loadSong (const juce::File& file, Song& song, juce::String& error)
{
    const auto json = juce::JSON::parse (file);
    const int version = json.isObject() ? (int) json["formatVersion"] : 0;
    if (version != 1 && version != Song::formatVersion)
    {
        error = file.getFileName() + ": kein gültiger Song"_u;
        return false;
    }

    song = {};
    song.id = json["id"].toString().toStdString();
    song.name = json["name"].toString().toStdString();
    song.tempo = (double) json["tempo"];
    if (! juce::String (song.id).containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789-_."))
    {
        error = file.getFileName() + ": ungültige id \""_u + juce::String (song.id) + "\"";   // ids become file names
        return false;
    }

    auto readList = [&] (const juce::var& list, std::vector<SongPart>& out, const char* what)
    {
        if (auto* items = list.getArray())
            for (int i = 0; i < items->size(); ++i)
            {
                SongPart part;
                juce::String err;
                if (! partFromJson ((*items)[i], version, part, err))
                {
                    error = file.getFileName() + ": " + what + " " + juce::String (i + 1) + ": " + err;
                    return false;
                }
                out.push_back (std::move (part));
            }
        return true;
    };
    if (version == Song::formatVersion && ! json["parts"].isArray())
    {
        error = file.getFileName() + ": \"parts\" fehlt"_u;
        return false;
    }
    if (! readList (json["parts"], song.parts, "Teil"))
        return false;

    // v1 songs need parts; a v2 file may be an empty working state of the arranger
    if (song.name.empty() || (version == 1 && (song.id.empty() || song.parts.empty())))
    {
        error = file.getFileName() + ": id, name oder Teile fehlen"_u;
        return false;
    }
    return true;
}

juce::var songToJson (const Song& song)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("formatVersion", Song::formatVersion);
    root->setProperty ("id", juce::String (song.id));
    root->setProperty ("name", juce::String (song.name));
    root->setProperty ("tempo", song.tempo);

    juce::Array<juce::var> parts;
    for (auto& p : song.parts)
        parts.add (partToJson (p));
    root->setProperty ("parts", parts);
    return juce::var (root);
}

bool saveSong (const Song& song, const juce::File& file, juce::String& error)
{
    for (auto& p : song.parts)
        if (p.kind == PartKind::Pattern && ! p.copy)
        {
            error = "Song enthält noch Verweise ohne Kopie (embedCopies fehlt)"_u;
            return false;
        }
    return writeJson (songToJson (song), file, error);
}

bool loadPartList (const juce::File& file, std::vector<SongPart>& list, juce::String& error)
{
    list.clear();
    if (! file.existsAsFile())
        return true;
    const auto json = juce::JSON::parse (file);
    auto* items = json["parts"].getArray();
    if (! json.isObject() || (int) json["formatVersion"] != Song::formatVersion || items == nullptr)
    {
        error = file.getFileName() + ": keine gültige Liste"_u;
        return false;
    }
    for (int i = 0; i < items->size(); ++i)
    {
        SongPart part;
        juce::String err;
        if (! partFromJson ((*items)[i], Song::formatVersion, part, err))
        {
            error = file.getFileName() + ": Eintrag " + juce::String (i + 1) + ": " + err;
            list.clear();   // all or nothing: a half list would be saved back over the damaged one
            return false;
        }
        list.push_back (std::move (part));
    }
    return true;
}

std::string newBlockUid()
{
    return juce::Uuid().toString().substring (0, 12).toStdString();
}

bool sameSource (const SongPart& a, const SongPart& b)
{
    if (a.kind != b.kind)
        return false;
    if (a.kind != PartKind::Pattern)
        return a.restBars == b.restBars && a.meter.numerator == b.meter.numerator
            && a.meter.denominator == b.meter.denominator && a.meter.groups == b.meter.groups;
    return a.sourceId == b.sourceId && a.firstBar == b.firstBar && a.numBars == b.numBars;
}

bool sameBlock (const SongPart& a, const SongPart& b)
{
    if (! sameSource (a, b))
        return false;
    if (a.kind == PartKind::Pattern && a.copy && b.copy && a.copy != b.copy && ! samePatternContent (*a.copy, *b.copy))
        return false;   // a local variant is a block of its own
    return true;
}

bool savePartList (const std::vector<SongPart>& list, const juce::File& file, juce::String& error)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("formatVersion", Song::formatVersion);
    juce::Array<juce::var> parts;
    for (auto& p : list)
        parts.add (partToJson (p));
    root->setProperty ("parts", parts);
    return writeJson (juce::var (root), file, error);
}

bool embedCopies (Song& song, const std::vector<hz::Pattern>& library, juce::String& error)
{
    Song result = song;
    for (auto& p : result.parts)
        if (p.kind == PartKind::Pattern && ! p.copy)
        {
            const auto* src = findPattern (library, p.sourceId);
            if (src == nullptr)
            {
                error = "Pattern fehlt: "_u + juce::String (p.sourceId);
                return false;
            }
            p.copy = std::make_shared<const hz::Pattern> (*src);
        }
    song = std::move (result);
    return true;
}

//==============================================================================
int sourceColourIndex (const std::string& sourceId)
{
    if (auto* r = BadgeRegistry::global())
        return r->badgeFor (sourceId).colour;
    return BadgeRegistry::hashed (sourceId).colour;
}

int sourceShapeIndex (const std::string& sourceId)
{
    if (auto* r = BadgeRegistry::global())
        return r->badgeFor (sourceId).shape;
    return BadgeRegistry::hashed (sourceId).shape;
}

int partColourIndex (const SongPart& part)
{
    if (part.kind == PartKind::Rest)  return theme::restColourIndex;
    if (part.kind == PartKind::Click) return theme::clickColourIndex;
    return sourceColourIndex (part.sourceId);
}

int partShapeIndex (const SongPart& part)
{
    if (part.kind == PartKind::Rest)  return 7;    // bar = "pause"
    if (part.kind == PartKind::Click) return 9;    // dot = "click"
    return sourceShapeIndex (part.sourceId);
}

bool samePatternContent (const hz::Pattern& a, const hz::Pattern& b)
{
    auto key = [] (hz::Pattern p)
    {
        p.id.clear();
        p.revision = 0;
        p.origin = {};
        return juce::JSON::toString (patternToJson (p), true);
    };
    return key (a) == key (b);
}

bool sourceDiffers (const SongPart& part, const std::vector<hz::Pattern>& library)
{
    if (part.kind != PartKind::Pattern || ! part.copy)
        return false;
    const auto* src = findPattern (library, part.sourceId);
    return src != nullptr && ! samePatternContent (*src, *part.copy);
}

bool partPattern (const SongPart& part, const std::vector<hz::Pattern>& library, hz::Pattern& out, juce::String& error)
{
    if (part.kind != PartKind::Pattern)
    {
        out = {};
        out.id = kindName (part.kind).toStdString();
        out.name = part.kind == PartKind::Rest ? "Pause" : "Klick";
        out.tempo = 0.0;
        out.humanize = 0.0f;
        out.layers = { { 1, hz::LayerRole::Hands, {} }, { 2, hz::LayerRole::Feet, {} } };
        hz::PatternBar bar;
        bar.meter = part.meter;
        bar.grid = hz::Rational (1, std::max (16, part.meter.denominator));
        out.bars.assign ((size_t) part.restBars, bar);
        return true;
    }

    const hz::Pattern* src = part.copy ? part.copy.get() : findPattern (library, part.sourceId);
    if (src == nullptr)
    {
        error = "Pattern fehlt: "_u + juce::String (part.sourceId);
        return false;
    }
    if (part.numBars <= 0)
    {
        out = *src;
        return true;
    }
    if (part.firstBar < 0 || part.firstBar + part.numBars > (int) src->bars.size())
    {
        error = juce::String (src->name) + ": Taktbereich passt nicht (Pattern hat "_u + juce::String ((int) src->bars.size()) + " Takte)";
        return false;
    }
    out = hz::sliceBars (*src, part.firstBar, part.numBars);
    return true;
}

juce::String partTitle (const SongPart& part, const hz::Pattern& played)
{
    if (! part.label.empty())
        return juce::String (part.label);
    if (part.kind != PartKind::Pattern)
        return juce::String (part.kind == PartKind::Rest ? "Pause " : "Klick ") + juce::String (hz::meterLabel (part.meter));
    juce::String title (played.name);
    if (part.numBars > 0)
        title << " (T. " << (part.firstBar + 1)
              << (part.numBars > 1 ? "–"_u + juce::String (part.firstBar + part.numBars) : juce::String()) << ")";
    return title;
}

bool buildSongPattern (const Song& song, const std::vector<hz::Pattern>& library,
                       hz::Pattern& result, std::vector<SongSpan>& spans, juce::String& error)
{
    result = {};
    result.id = "song." + song.id;
    result.name = song.name;
    result.tags = { "song-struktur" };
    result.layers = { { 1, hz::LayerRole::Hands, {} }, { 2, hz::LayerRole::Feet, {} } };
    spans.clear();

    std::uint32_t nextId = 1;
    hz::Rational offset;
    bool haveSettings = false;

    for (int pi = 0; pi < (int) song.parts.size(); ++pi)
    {
        const auto& part = song.parts[(size_t) pi];
        hz::Pattern src;
        if (! partPattern (part, library, src, error))
            return false;

        if (! haveSettings && part.kind == PartKind::Pattern)
        {
            result.tempo = song.tempo > 0.0 ? song.tempo : src.tempo;
            result.humanize = src.humanize;
            result.humanizeSeed = src.humanizeSeed;
            result.swing = src.swing;
            haveSettings = true;
        }

        const int colour = partColourIndex (part);
        const auto title = partTitle (part, src);
        const bool differs = sourceDiffers (part, library);

        for (int r = 0; r < part.repeats; ++r)
        {
            spans.push_back ({ title, juce::String (src.name), pi, r, part.repeats, (int) result.bars.size(), (int) src.bars.size(),
                               colour, partShapeIndex (part), part.edited, differs });

            for (auto& b : src.bars)
                result.bars.push_back (b);

            for (auto& layer : src.layers)
            {
                auto& dest = result.layers[layer.role == hz::LayerRole::Feet ? 1 : 0];
                for (auto e : layer.events)
                {
                    e.id = nextId++;
                    e.pos = e.pos + offset;
                    dest.events.push_back (e);
                }
            }
            offset = offset + src.lengthInQuarters();
        }
    }

    if (! haveSettings)
        result.tempo = song.tempo > 0.0 ? song.tempo : 120.0;

    if (auto v = result.validate(); ! v.empty())
    {
        if (result.bars.size() > (size_t) hz::Pattern::maxBars)
            error = "Grenze erreicht: Ein Ablauf darf höchstens "_u + juce::String (hz::Pattern::maxBars) + " Takte enthalten.";
        else if (v == "zu viele Schläge")
            error = "Grenze erreicht: Ein Ablauf darf höchstens "_u + juce::String ((int) hz::Pattern::maxEvents) + " Schläge enthalten."_u;
        else
            error = juce::String (v);
        return false;
    }
    error.clear();
    return true;
}

juce::String songChangeError (const Song& before, const Song& after, const std::vector<hz::Pattern>& library)
{
    hz::Pattern candidate;
    std::vector<SongSpan> spans;
    juce::String error;
    if (buildSongPattern (after, library, candidate, spans, error))
        return {};
    if (! error.startsWith ("Grenze erreicht:"))
        return error;

    // A restored oversized song must be repairable in several small steps.
    hz::Pattern previous;
    juce::String previousError;
    if (buildSongPattern (before, library, previous, spans, previousError)
        || ! previousError.startsWith ("Grenze erreicht:"))
        return error;
    const auto events = [] (const hz::Pattern& pattern)
    {
        size_t count = 0;
        for (const auto& layer : pattern.layers)
            count += layer.events.size();
        return count;
    };
    const auto previousEvents = events (previous), candidateEvents = events (candidate);
    if (candidate.bars.size() <= previous.bars.size() && candidateEvents <= previousEvents
        && (candidate.bars.size() < previous.bars.size() || candidateEvents < previousEvents))
        return {};
    return error;
}
