#include "engine/Pattern.h"

#include <algorithm>

namespace hz
{

namespace
{
    const char* const instrumentNames[] = {
        "kick", "snare", "hihat", "ride", "crash1", "crash2", "china", "splash", "tom1", "tom2", "tom3", "tom4", "cowbell"
    };
    const char* const articulationNames[] = { "", "open", "half", "pedal", "bell", "rim", "side" };

    static_assert (std::size (instrumentNames) == (size_t) Instrument::Count);
    static_assert (std::size (articulationNames) == (size_t) Articulation::Count);

    constexpr float flamGraceVelocity = 0.45f;   // relative to the main hit
    constexpr float flamDelayMs = 25.0f;          // main hit comes this long after the grace note
}

std::string voiceKey (Voice v)
{
    std::string k = instrumentNames[(size_t) v.instrument];
    if (v.articulation != Articulation::Normal)
        k += std::string (".") + articulationNames[(size_t) v.articulation];
    return k;
}

bool parseVoiceKey (const std::string& key, Voice& out)
{
    const auto dot = key.find ('.');
    const auto inst = key.substr (0, dot);
    const auto art = dot == std::string::npos ? std::string() : key.substr (dot + 1);

    for (size_t i = 0; i < (size_t) Instrument::Count; ++i)
        if (inst == instrumentNames[i])
            for (size_t a = 0; a < (size_t) Articulation::Count; ++a)
                if (art == articulationNames[a])
                {
                    out = { (Instrument) i, (Articulation) a };
                    return true;
                }
    return false;
}

Rational Pattern::lengthInQuarters() const
{
    Rational len;
    for (const auto& b : bars)
        len = len + b.lengthInQuarters();
    return len;
}

Rational Pattern::barStart (int bar) const
{
    Rational start;
    for (int i = 0; i < bar && i < (int) bars.size(); ++i)
        start = start + bars[(size_t) i].lengthInQuarters();
    return start;
}

int Pattern::barAt (Rational pos) const
{
    Rational start;
    for (int i = 0; i < (int) bars.size(); ++i)
    {
        const auto end = start + bars[(size_t) i].lengthInQuarters();
        if (pos < end)
            return i;
        start = end;
    }
    return (int) bars.size() - 1;
}

std::uint32_t Pattern::nextEventId() const
{
    std::uint32_t maxId = 0;
    for (const auto& l : layers)
        for (const auto& e : l.events)
            maxId = std::max (maxId, e.id);
    return maxId + 1;
}

std::string Pattern::validate() const
{
    constexpr std::int64_t maxDenominator = 1 << 16;

    if (bars.empty() || (int) bars.size() > maxBars)
        return "Pattern braucht 1 bis 512 Takte";

    for (const auto& b : bars)
    {
        if (! b.meter.isValid())
            return "ungültige Taktart";
        if (b.grid.num <= 0 || b.grid.den > 256)
            return "ungültiges Raster";
    }

    const auto len = lengthInQuarters();
    size_t count = 0;
    std::vector<std::uint32_t> ids;

    for (const auto& l : layers)
        for (const auto& e : l.events)
        {
            if (++count > maxEvents)
                return "zu viele Schläge";
            if (e.pos < Rational (0) || e.pos >= len || e.pos.den > maxDenominator)
                return "Schlag außerhalb des Patterns";
            if (! (e.velocity >= 0.0f && e.velocity <= 1.0f))
                return "Velocity außerhalb 0..1";
            if (! (e.lagMs >= -100.0f && e.lagMs <= 100.0f))
                return "Versatz außerhalb -100..100 ms";
            ids.push_back (e.id);
        }

    std::sort (ids.begin(), ids.end());
    if (std::adjacent_find (ids.begin(), ids.end()) != ids.end())
        return "doppelte Event-ID";

    if (! (swing.ratio >= 0.5f && swing.ratio <= 0.75f))
        return "Swing-Verhältnis außerhalb 0,5..0,75";
    if (swing.subdivision != Rational (1, 8) && swing.subdivision != Rational (1, 16))
        return "Swing nur auf Achtel (1/8) oder Sechzehntel (1/16)";
    if (! (humanize >= 0.0f && humanize <= 1.0f))
        return "Humanisierung außerhalb 0..1";
    if (! (tempo >= 20.0 && tempo <= 400.0))
        return "Tempo außerhalb 20..400";

    return {};
}

PatternPlan buildPlan (const Pattern& p)
{
    PatternPlan plan;
    plan.patternId = p.id;
    plan.length = p.lengthInQuarters().toDouble();
    plan.humanize = p.humanize;
    plan.seed = p.humanizeSeed;

    for (int i = 0; i < (int) p.bars.size(); ++i)
        plan.bars.push_back ({ p.barStart (i).toDouble(), p.bars[(size_t) i].lengthInQuarters().toDouble(), p.bars[(size_t) i].meter, 0 });

    const Rational sub = p.swing.subdivision * Rational (4);   // in quarters
    const bool swinging = p.swing.ratio > 0.5f && sub.num > 0;

    for (const auto& layer : p.layers)
        for (const auto& e : layer.events)
        {
            double pos = e.pos.toDouble();

            // Swing: only exact offbeats of the subdivision (triplets stay untouched)
            if (swinging)
            {
                const auto rel = e.pos - p.barStart (p.barAt (e.pos));
                const auto pairs = rel / (sub * Rational (2));
                const auto frac = pairs - Rational (pairs.num / pairs.den);
                if (frac == Rational (1, 2))
                    pos += (sub.toDouble() * 2.0) * (p.swing.ratio - 0.5);
            }

            if (e.flam)
            {
                plan.events.push_back ({ pos, e.voice, e.velocity * flamGraceVelocity, e.limb, e.id, e.lagMs, 0, p.humanize });
                plan.events.push_back ({ pos, e.voice, e.velocity, e.limb, e.id, flamDelayMs + e.lagMs, 1, p.humanize });
            }
            else
            {
                plan.events.push_back ({ pos, e.voice, e.velocity, e.limb, e.id, e.lagMs, 0, p.humanize });
            }
        }

    std::stable_sort (plan.events.begin(), plan.events.end(), [] (const PlanEvent& a, const PlanEvent& b) { return a.pos < b.pos; });
    return plan;
}

} // namespace hz
