#include "engine/PatternEdit.h"

#include <algorithm>

namespace hz
{

namespace
{

PatternLayer& layerFor (Pattern& p, LayerRole role)
{
    for (auto& l : p.layers)
        if (l.role == role)
            return l;

    std::uint32_t id = 1;
    for (auto& l : p.layers)
        id = std::max (id, l.id + 1);
    p.layers.push_back ({ id, role, {} });
    return p.layers.back();
}

void sortEvents (Pattern& p)
{
    for (auto& l : p.layers)
        std::stable_sort (l.events.begin(), l.events.end(), [] (const PatternEvent& a, const PatternEvent& b) { return a.pos < b.pos; });
}

// Shifts all hits from 'from' by 'delta' quarters; removes those in the range [cutFrom, cutTo)
void shiftAndCut (Pattern& p, Rational from, Rational delta, Rational cutFrom, Rational cutTo)
{
    for (auto& l : p.layers)
    {
        auto& ev = l.events;
        ev.erase (std::remove_if (ev.begin(), ev.end(), [&] (const PatternEvent& e) { return e.pos >= cutFrom && e.pos < cutTo; }),
                  ev.end());
        for (auto& e : ev)
            if (e.pos >= from)
                e.pos = e.pos + delta;
    }
}

} // namespace

LayerRole roleFor (Voice v)
{
    if (v.instrument == Instrument::Kick || (v.instrument == Instrument::HiHat && v.articulation == Articulation::Pedal))
        return LayerRole::Feet;
    return LayerRole::Hands;
}

const PatternEvent* findEvent (const Pattern& p, Voice v, Rational pos)
{
    for (const auto& l : p.layers)
        for (const auto& e : l.events)
            if (e.voice == v && e.pos == pos)
                return &e;
    return nullptr;
}

bool removeEvent (Pattern& p, Voice v, Rational pos)
{
    for (auto& l : p.layers)
    {
        auto it = std::find_if (l.events.begin(), l.events.end(), [&] (const PatternEvent& e) { return e.voice == v && e.pos == pos; });
        if (it != l.events.end())
        {
            l.events.erase (it);
            return true;
        }
    }
    return false;
}

bool toggleEvent (Pattern& p, Voice v, Rational pos, float velocity)
{
    if (removeEvent (p, v, pos))
        return false;

    if (pos < Rational (0) || pos >= p.lengthInQuarters())
        return false;

    PatternEvent e;
    e.id = p.nextEventId();
    e.voice = v;
    e.pos = pos;
    e.velocity = std::clamp (velocity, 0.05f, 1.0f);
    layerFor (p, roleFor (v)).events.push_back (e);
    sortEvents (p);
    return true;
}

bool setEventVelocity (Pattern& p, Voice v, Rational pos, float velocity)
{
    for (auto& l : p.layers)
        for (auto& e : l.events)
            if (e.voice == v && e.pos == pos)
            {
                e.velocity = std::clamp (velocity, 0.05f, 1.0f);
                return true;
            }
    return false;
}

bool setEventLag (Pattern& p, Voice v, Rational pos, float lagMs)
{
    for (auto& l : p.layers)
        for (auto& e : l.events)
            if (e.voice == v && e.pos == pos)
            {
                e.lagMs = std::clamp (lagMs, -100.0f, 100.0f);
                return true;
            }
    return false;
}

bool setEventFlam (Pattern& p, Voice v, Rational pos, bool flam)
{
    for (auto& l : p.layers)
        for (auto& e : l.events)
            if (e.voice == v && e.pos == pos)
            {
                e.flam = flam;
                return true;
            }
    return false;
}

void insertBar (Pattern& p, int afterBar, bool copyContent)
{
    if (p.bars.empty())
        return;
    afterBar = std::clamp (afterBar, 0, (int) p.bars.size() - 1);
    const auto source = p.bars[(size_t) afterBar];
    const Rational srcStart = p.barStart (afterBar);
    const Rational len = source.lengthInQuarters();
    const Rational insertAt = srcStart + len;

    // Copies remember their layer (there can be several layers of the same role)
    std::vector<std::pair<std::uint32_t, PatternEvent>> copies;
    if (copyContent)
        for (const auto& l : p.layers)
            for (const auto& e : l.events)
                if (e.pos >= srcStart && e.pos < insertAt)
                    copies.push_back ({ l.id, e });

    shiftAndCut (p, insertAt, len, Rational (1), Rational (0));   // empty cut range
    p.bars.insert (p.bars.begin() + afterBar + 1, source);

    std::uint32_t id = p.nextEventId();
    for (auto [layerId, e] : copies)
    {
        e.id = id++;
        e.pos = e.pos + len;
        for (auto& l : p.layers)
            if (l.id == layerId)
                l.events.push_back (e);
    }
    sortEvents (p);
}

bool removeBar (Pattern& p, int bar)
{
    if (p.bars.size() <= 1 || bar < 0 || bar >= (int) p.bars.size())
        return false;

    const Rational start = p.barStart (bar);
    const Rational len = p.bars[(size_t) bar].lengthInQuarters();
    shiftAndCut (p, start + len, Rational (0) - len, start, start + len);
    p.bars.erase (p.bars.begin() + bar);
    return true;
}

void setBarMeter (Pattern& p, int bar, const Meter& m)
{
    if (bar < 0 || bar >= (int) p.bars.size() || ! m.isValid())
        return;

    const Rational start = p.barStart (bar);
    const Rational oldLen = p.bars[(size_t) bar].lengthInQuarters();
    p.bars[(size_t) bar].meter = m;
    const Rational newLen = p.bars[(size_t) bar].lengthInQuarters();

    // Hits beyond the new bar end are dropped, following bars shift by the difference
    shiftAndCut (p, start + oldLen, newLen - oldLen, start + newLen, start + oldLen);

    // Grid must still divide evenly, otherwise pick a suitable one
    auto& b = p.bars[(size_t) bar];
    const auto steps = newLen / (b.grid * Rational (4));
    if (steps.den != 1)
        b.grid = Rational (1, m.denominator * (m.denominator >= 8 ? 2 : 4));
}

void setBarGrid (Pattern& p, int bar, Rational grid)
{
    if (bar < 0 || bar >= (int) p.bars.size() || grid.num <= 0)
        return;
    const auto steps = p.bars[(size_t) bar].lengthInQuarters() / (grid * Rational (4));
    if (steps.den == 1)
        p.bars[(size_t) bar].grid = grid;
}

void clearLane (Pattern& p, int bar, Voice v)
{
    if (bar < 0 || bar >= (int) p.bars.size())
        return;
    const Rational start = p.barStart (bar);
    const Rational end = start + p.bars[(size_t) bar].lengthInQuarters();
    for (auto& l : p.layers)
        l.events.erase (std::remove_if (l.events.begin(), l.events.end(),
                                        [&] (const PatternEvent& e) { return e.voice == v && e.pos >= start && e.pos < end; }),
                        l.events.end());
}

Pattern sliceBars (const Pattern& p, int first, int count)
{
    const int n = (int) p.bars.size();
    first = std::clamp (first, 0, std::max (0, n - 1));
    count = std::clamp (count, 1, std::max (1, n - first));

    Pattern out = p;
    out.bars.assign (p.bars.begin() + first, p.bars.begin() + first + count);
    const Rational start = p.barStart (first);
    const Rational end = p.barStart (first + count);
    for (auto& l : out.layers)
    {
        l.events.clear();
        for (const auto& layer : p.layers)
            if (layer.id == l.id)
                for (auto e : layer.events)
                    if (e.pos >= start && e.pos < end)
                    {
                        e.pos = e.pos - start;
                        l.events.push_back (e);
                    }
    }
    return out;
}

} // namespace hz
