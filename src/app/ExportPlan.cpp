#include "app/ExportPlan.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double eps = 1.0e-9;

    double barLength (const hz::Meter& m)   { return m.numerator * m.quartersPerPulse(); }
}

ExportTimeline makeExportTimeline (const hz::PatternPlan& source, const ExportSpan& span, double tempo, double tailSeconds)
{
    ExportTimeline t;
    const bool range = span.to > span.from;
    const double from = range ? std::clamp (span.from, 0.0, source.length) : 0.0;
    const double to = range ? std::clamp (span.to, from, source.length) : source.length;

    std::vector<hz::PlanBar> bars;   // the bars of the span (a range lies on bar lines)
    for (const auto& b : source.bars)
        if (b.start >= from - eps && b.start < to - eps)
            bars.push_back (b);
    if (bars.empty() || to - from <= eps)
        return t;

    t.plan = std::make_unique<hz::PatternPlan>();
    auto& plan = *t.plan;
    plan.patternId = "export";
    plan.humanize = source.humanize;
    plan.seed = source.seed;
    double at = 0.0;

    if (span.countIn)
    {
        hz::PlanBar count = bars.front();
        count.start = 0.0;
        count.length = barLength (count.meter);
        count.click = true;
        plan.bars.push_back (count);
        at = count.length;
    }
    t.musicStart = at;

    for (int pass = 0; pass < std::max (1, span.repeats); ++pass)
    {
        const double shift = at - from;
        for (auto b : bars)
        {
            b.start += shift;
            b.click = b.click || span.click;
            plan.bars.push_back (b);
        }
        for (const auto& e : source.events)
            if (e.pos >= from - eps && e.pos < to - eps)
            {
                auto hit = e;
                hit.pos += shift;
                hit.eventId = e.eventId + (std::uint32_t) pass * 0x9e3779b9u;   // every pass humanized anew, as when it loops
                plan.events.push_back (hit);
            }
        at += to - from;
    }
    t.musicEnd = at;

    // silent bars in the meter of the last one, no click: the hits ring out, the player does not wrap around
    auto tail = bars.back();
    tail.length = barLength (tail.meter);
    tail.click = false;
    const double tailQuarters = std::max (0.0, tailSeconds) * std::max (1.0, tempo) / 60.0;
    for (double rest = tailQuarters; rest > eps; rest -= tail.length)
    {
        tail.start = at;
        plan.bars.push_back (tail);
        at += tail.length;
    }
    plan.length = at;
    return t;
}
