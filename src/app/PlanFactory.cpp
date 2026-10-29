#include "app/PlanFactory.h"

#include "app/Library.h"
#include "app/MeterLibrary.h"

namespace
{
    float velocityFor (hz::Meter::Accent a)
    {
        switch (a)
        {
            case hz::Meter::Accent::Downbeat: return 1.0f;
            case hz::Meter::Accent::Group:    return 0.72f;
            case hz::Meter::Accent::Weak:     return 0.45f;
        }
        return 0.5f;
    }
}

std::unique_ptr<hz::PatternPlan> makeMetronomePlan (const hz::Meter& m, MetronomeSound sound)
{
    auto plan = std::make_unique<hz::PatternPlan>();
    plan->patternId = "metronome";
    plan->length = m.numerator * m.quartersPerPulse();
    plan->bars.push_back ({ 0.0, plan->length, m, 0 });

    if (sound == MetronomeSound::Click)
        return plan;   // pulses only; the AudioEngine's click synth plays them

    for (int k = 0; k < m.numerator; ++k)
    {
        const auto accent = m.accentAt (k);
        hz::Voice v { hz::Instrument::Kick, hz::Articulation::Normal };
        float vel = velocityFor (accent);

        if (sound == MetronomeSound::Snare)
            v.instrument = hz::Instrument::Snare;
        else if (sound == MetronomeSound::KickSnare)
        {
            // downbeat and group starts: kick, others: snare; in x/4: kick on 1+3, snare on 2+4
            const bool kick = m.denominator <= 4 ? (k % 2 == 0) : accent != hz::Meter::Accent::Weak;
            v.instrument = kick ? hz::Instrument::Kick : hz::Instrument::Snare;
            if (m.denominator <= 4)
                vel = 0.85f;
        }

        plan->events.push_back ({ k * m.quartersPerPulse(), v, vel, hz::Limb::Right, (std::uint32_t) k + 1, 0.0f, 0, 0.0f });
    }
    return plan;
}

std::unique_ptr<hz::PatternPlan> makePatternPlan (const hz::Pattern& p)
{
    return std::make_unique<hz::PatternPlan> (hz::buildPlan (p));
}

std::unique_ptr<hz::PatternPlan> makeSongPlan (const Song& song, const Library& library)
{
    auto plan = std::make_unique<hz::PatternPlan>();
    plan->patternId = "song." + song.id;
    double offset = 0.0;
    std::uint32_t occurrence = 0;

    for (const auto& part : song.parts)
    {
        hz::Pattern played;
        juce::String error;
        if (! partPattern (part, library.all(), played, error))
            return nullptr;

        const auto partPlan = hz::buildPlan (played);   // own swing/humanize/flams
        if (plan->seed == 1 && part.kind == PartKind::Pattern)
            plan->seed = partPlan.seed;

        for (int r = 0; r < part.repeats; ++r, ++occurrence)
        {
            for (auto b : partPlan.bars)
            {
                b.start += offset;
                b.click = part.kind == PartKind::Click;
                plan->bars.push_back (b);
            }
            for (auto e : partPlan.events)
            {
                e.pos += offset;
                e.eventId += occurrence << 20;   // repeats get their own humanizing
                plan->events.push_back (e);
            }
            offset += partPlan.length;
        }
    }

    plan->length = offset;
    return plan;
}

//==============================================================================
void PlanRegistry::registerPlan (hz::PatternPlan& plan, MeterLibrary& meters, PlanInfo info)
{
    for (auto& b : plan.bars)
        b.meterTag = juce::jmax (0, meters.findOrAdd (b.meter));

    plan.tag = nextTag++;
    lastTag = plan.tag;
    info.patternId = plan.patternId;
    info.bars = plan.bars;
    info.events = plan.events;
    infos[plan.tag] = std::move (info);

    while (infos.size() > maxKept)
        infos.erase (infos.begin());
}

const PlanInfo* PlanRegistry::find (int tag) const
{
    auto it = infos.find (tag);
    return it != infos.end() ? &it->second : nullptr;
}
