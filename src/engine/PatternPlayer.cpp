#include "engine/PatternPlayer.h"

#include <algorithm>
#include <cmath>

namespace hz
{

float humanizeNoise (std::uint32_t seed, std::uint32_t eventId, int loop, int channel)
{
    // Integer hash (lowbias32 variant), realtime-safe
    std::uint32_t h = seed * 0x9e3779b9u ^ eventId * 0x85ebca6bu ^ (std::uint32_t) loop * 0xc2b2ae35u ^ (std::uint32_t) channel * 0x27d4eb2fu;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return (float) h / 2147483647.5f - 1.0f;
}

void PatternPlayer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
}

void PatternPlayer::setTempo (double quarterBpm)
{
    tempo = std::clamp (quarterBpm, 20.0, 400.0);
}

void PatternPlayer::retire (const PatternPlan* plan)
{
    if (plan == nullptr)
        return;
    if (numRetired < (int) retired.size())
        retired[(size_t) numRetired++] = plan;
    // Otherwise: return queue full (UI isn't collecting) -> plan stays in memory, but no crash
}

const PatternPlan* PatternPlayer::popRetired()
{
    if (numRetired == 0)
        return nullptr;
    return retired[(size_t) --numRetired];
}

void PatternPlayer::setNextPlan (const PatternPlan* plan)
{
    if (plan == current || plan == pending)
        return;

    retire (pending);
    pending = plan;

    if (! playing || current == nullptr)
        applyPending();
    else
        // the same pattern or song again (e.g. the editor's audition copy): take it over right
        // away and keep the position instead of starting over at the next bar end
        seamless = plan->patternId == current->patternId && std::abs (plan->length - current->length) < 1.0e-9;
}

void PatternPlayer::applyPending()
{
    if (pending == nullptr)
        return;
    retire (current);
    current = pending;
    pending = nullptr;
    resuming = false;
    // a new plan starts from its beginning, unless a seek already named a position in it
    startAt = pendingStart >= 0.0 ? std::clamp (pendingStart, 0.0, std::max (0.0, current->length - 1.0e-9)) : 0.0;
    pendingStart = -1.0;
}

void PatternPlayer::seek (double quarters)
{
    if (pending != nullptr)
    {
        pendingStart = quarters;   // the switch takes it over (applyPending)
        return;
    }
    if (current == nullptr || current->length <= 0.0)
        return;

    const double q = std::clamp (quarters, 0.0, std::nextafter (current->length, 0.0));
    if (! playing)
    {
        startAt = q;
        resuming = false;
        return;
    }
    anchorQ = q;
    anchorSample = clock;
    anchorSpq = sampleRate * 60.0 / tempo;
    loopSpq = anchorSpq;
    rangePass = false;
    floorSample = clock;
    floorQ = q;
    floorKeepsWritten = true;
}

void PatternPlayer::start()
{
    applyPending();
    playing = true;
    clock = 0;
    anchorSample = 0;
    anchorQ = current != nullptr ? std::clamp (startAt, 0.0, std::max (0.0, current->length - 1.0e-9)) : 0.0;
    floorSample = 0;
    if (resuming)
    {
        // the paused pass goes on: same pass count and humanize tempo (the same offsets as before the pause),
        // same range state; nothing handed out before the pause comes again
        floorQ = pauseFloor;
        floorKeepsWritten = pauseKeepsWritten;
    }
    else
    {
        floorQ = anchorQ;   // a fresh start or a seek: its early hits are played on it
        floorKeepsWritten = true;
        loopSpq = sampleRate * 60.0 / tempo;
        loop = 0;
        rangePass = false;
    }
    resuming = false;
    startAt = 0.0;
    anchorSpq = 0.0;
}

void PatternPlayer::stop()
{
    // pause: remember the position so that the next start() continues here
    if (playing && current != nullptr && anchorSpq > 0.0)
    {
        startAt = std::fmod (anchorQ + (double) (clock - anchorSample) / anchorSpq, current->length);
        if (clock > floorSample)
        {
            pauseFloor = startAt - 0.5 / anchorSpq;   // hits up to half a sample before the pause are out
            pauseKeepsWritten = false;
        }
        else
        {
            pauseFloor = floorQ;   // nothing handed out since the floor was set (a pass start or seek just before)
            pauseKeepsWritten = floorKeepsWritten;
        }
        resuming = true;
    }
    playing = false;
}

void PatternPlayer::updateLoopRange()
{
    const double length = current != nullptr ? current->length : 0.0;
    loopFrom = std::clamp (rangeStart, 0.0, length);
    loopTo = std::clamp (rangeEnd, loopFrom, length);
    if (rangeStart < 0.0 || loopTo <= loopFrom + 1.0e-9)
        loopFrom = loopTo = 0.0;
}

double PatternPlayer::humanizedPos (const PlanEvent& e) const
{
    const double amount = (double) (e.humanize * humanizeScale);
    double ms = e.extraDelayMs;
    if (amount > 0.0)
        ms += maxHumanizeMs * amount * humanizeNoise (current->seed, e.eventId, loop, 2 * e.part);

    const double q = e.pos + ms * 0.001 * sampleRate / loopSpq;

    // never push past the pattern boundary: switch and count-in stay unambiguous; a hit of the loop range
    // stays inside the range, so its first beat is heard after every jump back
    // (a late hit never leaves the range at its end; an early one on its first beat only after a jump back,
    // on the way in it is heard early as written)
    if (loopTo > loopFrom && e.pos >= loopFrom && e.pos < loopTo)
        return std::clamp (q, rangePass ? loopFrom : 0.0, std::nextafter (loopTo, loopFrom));
    return std::clamp (q, 0.0, std::nextafter (current->length, 0.0));
}

float PatternPlayer::humanizedVelocity (const PlanEvent& e) const
{
    const float amount = e.humanize * humanizeScale;
    if (amount <= 0.0f)
        return e.velocity;
    const float v = e.velocity * (1.0f + maxHumanizeVelocity * amount * humanizeNoise (current->seed, e.eventId, loop, 2 * e.part + 1));
    return std::clamp (v, 0.0f, 1.0f);
}

std::int64_t PatternPlayer::sampleAt (double q, double samplesPerQuarter) const
{
    return anchorSample + (std::int64_t) std::llround ((q - anchorQ) * samplesPerQuarter);
}

void PatternPlayer::collect (std::int64_t from, std::int64_t to, std::int64_t loopEnd, double endQ,
                             std::int64_t blockStart, double samplesPerQuarter, PlayEvent* events, int maxEvents, int& numEvents,
                             PulseEvent* pulses, int maxPulses, int& numPulses)
{
    // Which hits belong to this pass is decided by their written place: none written at or after where it ends
    // (the plan end, the end of the loop range, the bar end where a waiting plan takes over), so neither an early
    // hit nor a flam's grace note of the bar after it; after a jump back to the range start none of the bars
    // before it (not even a late hit that reaches into the range), also when the loop is switched off meanwhile.
    auto belongs = [&] (double pos) { return pos < endQ && ! (rangePass && pos < rangePassFrom); };

    // Where it sounds: not before the floor (see floorQ), and a late hit at the latest on the last sample of
    // the pass (rounding or its delay would push it past the end).
    auto place = [&] (double pos, double q, std::int64_t& s)
    {
        if (q < floorQ && ! (floorKeepsWritten && pos >= floorQ))
            return false;
        s = std::min (std::max (sampleAt (q, samplesPerQuarter), floorSample), loopEnd - 1);
        return true;
    };

    for (const auto& e : current->events)
    {
        if (! belongs (e.pos))
            continue;
        const double q = humanizedPos (e);
        std::int64_t s = 0;
        if (place (e.pos, q, s) && s >= from && s < to && numEvents < maxEvents)
            events[numEvents++] = { (int) (s - blockStart), e.voice, humanizedVelocity (e), e.limb };
    }

    // Beats (without humanization)
    for (int b = 0; b < (int) current->bars.size(); ++b)
    {
        const auto& bar = current->bars[(size_t) b];
        const double step = bar.meter.quartersPerPulse();
        for (int k = 0; k < bar.meter.numerator; ++k)
        {
            const double q = bar.start + k * step;
            std::int64_t s = 0;
            if (! belongs (q) || ! place (q, q, s))
                continue;
            if (s >= from && s < to && numPulses < maxPulses)
                pulses[numPulses++] = { (int) (s - blockStart), b, k, loop, bar.meterTag, bar.meter.accentAt (k),
                                         (int) current->bars.size(), current->tag, bar.click };
        }
    }
}

void PatternPlayer::process (int numSamples, PlayEvent* events, int maxEvents, int& numEvents,
                             PulseEvent* pulses, int maxPulses, int& numPulses)
{
    numEvents = 0;
    numPulses = 0;
    if (! playing || current == nullptr || current->length <= 0.0)
        return;

    const double samplesPerQuarter = sampleRate * 60.0 / tempo;

    if (pending != nullptr && seamless)
    {
        const bool jump = pendingStart >= 0.0;
        applyPending();   // anchors stay: playback continues at the same position in the new plan
        seamless = false;
        if (jump)
            seek (startAt);
        startAt = 0.0;
    }

    // Tempo change: set anchor to the current position, the new tempo applies from here
    if (samplesPerQuarter != anchorSpq)
    {
        if (anchorSpq > 0.0)
        {
            anchorQ += (double) (clock - anchorSample) / anchorSpq;
            if (clock > anchorSample)
            {
                // the old tempo has handed out every hit that rounded to a sample before this block: those are
                // done, everything after comes from here on (the new sample grid would lose or repeat one)
                floorQ = anchorQ - 0.5 / anchorSpq;
                floorSample = clock;
                floorKeepsWritten = false;
            }
        }
        anchorSample = clock;
        anchorSpq = samplesPerQuarter;
    }

    const std::int64_t blockStart = clock;
    const std::int64_t blockEnd = clock + numSamples;
    std::int64_t from = blockStart;
    updateLoopRange();

    for (;;)
    {
        // The pass ends at the plan end; a pending plan takes over already at the next bar end,
        // so that even a long song reacts to a new selection right away.
        double endQ = current->length;   // where the pass ends, as written
        double exactEnd = (double) anchorSample + (endQ - anchorQ) * samplesPerQuarter;
        bool planEnd = true, toRangeEnd = false;
        if (pending == nullptr && loopTo > loopFrom)
        {
            // a loop range: the pass ends at its end, unless playback is already past it (then at the plan end)
            const double qFrom = anchorQ + (double) (from - anchorSample) / samplesPerQuarter;
            if (qFrom < loopTo - 1.0e-9)
            {
                endQ = loopTo;
                exactEnd = (double) anchorSample + (loopTo - anchorQ) * samplesPerQuarter;
                planEnd = false;
                toRangeEnd = true;
            }
        }
        if (pending != nullptr)
        {
            const double qFrom = anchorQ + (double) (from - anchorSample) / samplesPerQuarter;
            for (const auto& bar : current->bars)
            {
                const double barEnd = bar.start + bar.length;
                if (barEnd > qFrom + 1.0e-9 && barEnd < current->length - 1.0e-9)
                {
                    endQ = barEnd;
                    exactEnd = (double) anchorSample + (barEnd - anchorQ) * samplesPerQuarter;
                    planEnd = false;
                    break;
                }
            }
        }

        const std::int64_t loopEnd = std::max (from + 1, (std::int64_t) std::llround (exactEnd));
        const std::int64_t to = std::min (blockEnd, loopEnd);
        collect (from, to, loopEnd, endQ, blockStart, samplesPerQuarter, events, maxEvents, numEvents,
                 pulses, maxPulses, numPulses);

        if (loopEnd > blockEnd)
            break;

        // Boundary reached: take over the switch. The new pass begins exactly at exactEnd;
        // the rounding remainder is kept in the anchor so nothing accumulates over many passes.
        const bool switched = pending != nullptr;
        if (planEnd || toRangeEnd)
            ++loop;
        else
            loop = 0;   // the new plan starts with its first pass
        applyPending();
        updateLoopRange();
        anchorSample = loopEnd;
        // where the next pass begins: a loop range sends it back to its start (also after running to the plan end)
        const double restart = ! switched && loopTo > loopFrom ? loopFrom : startAt;   // startAt: a seek into the new plan
        rangePass = ! switched && loopTo > loopFrom;
        rangePassFrom = restart;
        anchorQ = restart + ((double) loopEnd - exactEnd) / samplesPerQuarter;
        floorSample = loopEnd;
        floorQ = restart;
        floorKeepsWritten = true;
        startAt = 0.0;
        loopSpq = samplesPerQuarter;
        from = loopEnd;
    }

    clock = blockEnd;
}

} // namespace hz
