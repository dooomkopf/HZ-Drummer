#include "app/PatternIO.h"
#include "app/Text.h"

#include "engine/MeterCatalog.h"

namespace
{

juce::String rationalText (hz::Rational r)
{
    return r.den == 1 ? juce::String (r.num) : juce::String (r.num) + "/" + juce::String (r.den);
}

bool parseRational (const juce::String& s, hz::Rational& out)
{
    const auto t = s.trim();
    if (t.isEmpty() || ! t.containsOnly ("-0123456789/"))
        return false;

    const auto num = t.upToFirstOccurrenceOf ("/", false, false).getLargeIntValue();
    const auto den = t.contains ("/") ? t.fromFirstOccurrenceOf ("/", false, false).getLargeIntValue() : 1;
    if (den <= 0 || den > (1 << 16) || std::abs (num) > (1ll << 40))
        return false;
    out = hz::Rational (num, den);
    return true;
}

const char* limbName (hz::Limb l)
{
    switch (l)
    {
        case hz::Limb::Right: return "right";
        case hz::Limb::Left:  return "left";
        case hz::Limb::Auto:  break;
    }
    return "auto";
}

const char* roleName (hz::LayerRole r)
{
    switch (r)
    {
        case hz::LayerRole::Feet:  return "feet";
        case hz::LayerRole::Other: return "other";
        case hz::LayerRole::Hands: break;
    }
    return "hands";
}

juce::var stringArray (const std::vector<std::string>& v)
{
    juce::Array<juce::var> a;
    for (auto& s : v)
        a.add (juce::String (s));
    return a;
}

} // namespace

juce::var patternToJson (const hz::Pattern& p)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("formatVersion", hz::Pattern::formatVersion);
    root->setProperty ("id", juce::String (p.id));
    root->setProperty ("name", juce::String (p.name));
    root->setProperty ("revision", p.revision);
    root->setProperty ("tags", stringArray (p.tags));

    auto* tempo = new juce::DynamicObject();
    tempo->setProperty ("default", p.tempo);
    tempo->setProperty ("min", p.tempoMin);
    tempo->setProperty ("max", p.tempoMax);
    root->setProperty ("tempo", tempo);

    juce::Array<juce::var> bars;
    for (auto& b : p.bars)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("meter", juce::String (hz::meterLabel (b.meter)));
        o->setProperty ("grid", rationalText (b.grid));
        bars.add (o);
    }
    root->setProperty ("bars", bars);

    auto* swing = new juce::DynamicObject();
    swing->setProperty ("subdivision", rationalText (p.swing.subdivision));
    swing->setProperty ("ratio", p.swing.ratio);
    root->setProperty ("swing", swing);

    auto* human = new juce::DynamicObject();
    human->setProperty ("amount", p.humanize);
    human->setProperty ("seed", (juce::int64) p.humanizeSeed);
    root->setProperty ("humanize", human);

    juce::Array<juce::var> layers;
    for (auto& l : p.layers)
    {
        auto* lo = new juce::DynamicObject();
        lo->setProperty ("id", (juce::int64) l.id);
        lo->setProperty ("role", roleName (l.role));

        juce::Array<juce::var> events;
        for (auto& e : l.events)
        {
            auto* eo = new juce::DynamicObject();
            eo->setProperty ("id", (juce::int64) e.id);
            eo->setProperty ("voice", juce::String (hz::voiceKey (e.voice)));
            eo->setProperty ("pos", rationalText (e.pos));
            eo->setProperty ("vel", std::round (e.velocity * 1000.0f) / 1000.0f);
            if (e.limb != hz::Limb::Auto) eo->setProperty ("limb", limbName (e.limb));
            if (e.flam) eo->setProperty ("flam", true);
            if (std::abs (e.lagMs) > 0.05f) eo->setProperty ("lag", std::round (e.lagMs * 10.0f) / 10.0f);   // ms later (-: earlier), only when set
            events.add (eo);
        }
        lo->setProperty ("events", events);
        layers.add (lo);
    }
    root->setProperty ("layers", layers);

    auto* origin = new juce::DynamicObject();
    origin->setProperty ("parentId", juce::String (p.origin.parentId));
    origin->setProperty ("parentRevision", p.origin.parentRevision);
    origin->setProperty ("operators", stringArray (p.origin.operators));
    origin->setProperty ("seed", (juce::int64) p.origin.seed);
    root->setProperty ("origin", origin);

    return root;
}

bool patternFromJson (const juce::var& json, hz::Pattern& p, juce::String& error)
{
    if (! json.isObject())
    {
        error = "kein JSON-Objekt";
        return false;
    }

    const int version = json["formatVersion"];
    if (version < 1 || version > hz::Pattern::formatVersion)
    {
        error = "unbekannte formatVersion " + juce::String (version);
        return false;
    }

    p = {};
    p.id = json["id"].toString().toStdString();
    p.name = json["name"].toString().toStdString();
    p.revision = (int) json["revision"];
    if (p.id.empty() || p.name.empty())
    {
        error = "id oder name fehlt";
        return false;
    }

    if (auto* tags = json["tags"].getArray())
        for (auto& t : *tags)
            p.tags.push_back (t.toString().toStdString());

    p.tempo = (double) json["tempo"]["default"];
    p.tempoMin = (double) json["tempo"]["min"];
    p.tempoMax = (double) json["tempo"]["max"];
    if (p.tempo < 20.0 || p.tempo > 400.0)
        p.tempo = 120.0;

    if (auto* bars = json["bars"].getArray())
        for (auto& b : *bars)
        {
            hz::PatternBar bar;
            if (! hz::parseMeter (b["meter"].toString().toStdString(), bar.meter) || ! parseRational (b["grid"].toString(), bar.grid))
            {
                error = "Takt ungültig: "_u + b["meter"].toString();
                return false;
            }
            p.bars.push_back (bar);
        }

    if (json["swing"].isObject())
    {
        parseRational (json["swing"]["subdivision"].toString(), p.swing.subdivision);
        p.swing.ratio = (float) (double) json["swing"]["ratio"];
    }

    p.humanize = (float) (double) json["humanize"]["amount"];
    p.humanizeSeed = (std::uint32_t) (juce::int64) json["humanize"]["seed"];

    if (auto* layers = json["layers"].getArray())
        for (auto& l : *layers)
        {
            hz::PatternLayer layer;
            layer.id = (std::uint32_t) (juce::int64) l["id"];
            const auto role = l["role"].toString();
            layer.role = role == "feet" ? hz::LayerRole::Feet : role == "other" ? hz::LayerRole::Other : hz::LayerRole::Hands;

            if (auto* events = l["events"].getArray())
                for (auto& e : *events)
                {
                    hz::PatternEvent ev;
                    ev.id = (std::uint32_t) (juce::int64) e["id"];
                    if (! hz::parseVoiceKey (e["voice"].toString().toStdString(), ev.voice))
                    {
                        error = "unbekannte Stimme " + e["voice"].toString();
                        return false;
                    }
                    if (! parseRational (e["pos"].toString(), ev.pos))
                    {
                        error = "Position ungültig: "_u + e["pos"].toString();
                        return false;
                    }
                    ev.velocity = (float) (double) e["vel"];
                    const auto limb = e["limb"].toString();
                    ev.limb = limb == "left" ? hz::Limb::Left : limb == "right" ? hz::Limb::Right : hz::Limb::Auto;
                    ev.flam = (bool) e["flam"];
                    if (const auto lag = e["lag"]; ! lag.isVoid() && ! (lag.isDouble() || lag.isInt() || lag.isInt64()))
                    {
                        error = "Versatz ungültig: "_u + lag.toString();   // a damaged value, not silently 0
                        return false;
                    }
                    ev.lagMs = (float) (double) e["lag"];   // missing = 0 (on the grid)
                    layer.events.push_back (ev);
                }
            p.layers.push_back (std::move (layer));
        }

    const auto& o = json["origin"];
    p.origin.parentId = o["parentId"].toString().toStdString();
    p.origin.parentRevision = (int) o["parentRevision"];
    p.origin.seed = (std::uint32_t) (juce::int64) o["seed"];
    if (auto* ops = o["operators"].getArray())
        for (auto& op : *ops)
            p.origin.operators.push_back (op.toString().toStdString());

    if (auto err = p.validate(); ! err.empty())
    {
        error = juce::String (err);
        return false;
    }
    return true;
}

bool savePattern (const hz::Pattern& p, const juce::File& file, juce::String& error)
{
    // atomic: write a temporary file first, then replace
    juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
    if (! temp.getFile().replaceWithText (juce::JSON::toString (patternToJson (p))) || ! temp.overwriteTargetFileWithTemporary())
    {
        error = "Kann nicht speichern: " + file.getFullPathName();
        return false;
    }
    return true;
}

bool loadPattern (const juce::File& file, hz::Pattern& p, juce::String& error)
{
    if (file.getSize() > 4 * 1024 * 1024)
    {
        error = "Datei zu groß"_u;
        return false;
    }
    return patternFromJson (juce::JSON::parse (file), p, error);
}

std::vector<hz::Pattern> loadPatternFolder (const juce::File& folder, juce::StringArray& errors)
{
    std::vector<hz::Pattern> result;
    auto files = folder.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json");
    files.sort();

    for (auto& f : files)
    {
        hz::Pattern p;
        juce::String err;
        if (loadPattern (f, p, err))
            result.push_back (std::move (p));
        else
            errors.add (f.getFileName() + ": " + err);
    }
    return result;
}
