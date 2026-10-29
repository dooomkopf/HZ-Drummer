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
        return;
    }
    anchorQ = q;
    anchorSample = clock;
    anchorSpq = sampleRate * 60.0 / tempo;
    loopSpq = anchorSpq;
}

void PatternPlayer::start()
{
    applyPending();
    playing = true;
    clock = 0;
    anchorSample = 0;
    anchorQ = current != nullptr ? std::clamp (startAt, 0.0, std::max (0.0, current->length - 1.0e-9)) : 0.0;
    startAt = 0.0;
    anchorSpq = 0.0;
    loopSpq = sampleRate * 60.0 / tempo;
    loop = 0;
}

void PatternPlayer::stop()
{
    // pause: remember the position so that the next start() continues here
    if (playing && current != nullptr && anchorSpq > 0.0)
        startAt = std::fmod (anchorQ + (double) (clock - anchorSample) / anchorSpq, current->length);
    playing = false;
}

double PatternPlayer::humanizedPos (const PlanEvent& e) const
{
    const double amount = (double) (e.humanize * humanizeScale);
    double ms = e.extraDelayMs;
    if (amount > 0.0)
        ms += maxHumanizeMs * amount * humanizeNoise (current->seed, e.eventId, loop, 2 * e.part);

    const double q = e.pos + ms * 0.001 * sampleRate / loopSpq;

    // never push past the pattern boundary: switch and count-in stay unambiguous
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

void PatternPlayer::collect (std::int64_t from, std::int64_t to, std::int64_t loopEnd, bool planEnd, std::int64_t blockStart,
                             double samplesPerQuarter, PlayEvent* events, int maxEvents, int& numEvents,
                             PulseEvent* pulses, int maxPulses, int& numPulses)
{
    // At the plan end, rounding may push a hit just past the boundary: clamp it. At a mid-plan
    // switch (bar end) everything beyond the boundary belongs to the abandoned rest: drop it.
    auto limit = [&] (std::int64_t s) { return planEnd ? std::min (s, loopEnd - 1) : s; };

    for (const auto& e : current->events)
    {
        const auto s = limit (sampleAt (humanizedPos (e), samplesPerQuarter));
        if (s >= from && s < to && numEvents < maxEvents)
            events[numEvents++] = { (int) (s - blockStart), e.voice, humanizedVelocity (e), e.limb };
    }

    // Beats (without humanization)
    for (int b = 0; b < (int) current->bars.size(); ++b)
    {
        const auto& bar = current->bars[(size_t) b];
        const double step = bar.meter.quartersPerPulse();
        for (int k = 0; k < bar.meter.numerator; ++k)
        {
            const auto s = limit (sampleAt (bar.start + k * step, samplesPerQuarter));
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
            anchorQ += (double) (clock - anchorSample) / anchorSpq;
        anchorSample = clock;
        anchorSpq = samplesPerQuarter;
    }

    const std::int64_t blockStart = clock;
    const std::int64_t blockEnd = clock + numSamples;
    std::int64_t from = blockStart;

    for (;;)
    {
        // The pass ends at the plan end; a pending plan takes over already at the next bar end,
        // so that even a long song reacts to a new selection right away.
        double exactEnd = (double) anchorSample + (current->length - anchorQ) * samplesPerQuarter;
        bool planEnd = true;
        if (pending != nullptr)
        {
            const double qFrom = anchorQ + (double) (from - anchorSample) / samplesPerQuarter;
            for (const auto& bar : current->bars)
            {
                const double barEnd = bar.start + bar.length;
                if (barEnd > qFrom + 1.0e-9 && barEnd < current->length - 1.0e-9)
                {
                    exactEnd = (double) anchorSample + (barEnd - anchorQ) * samplesPerQuarter;
                    planEnd = false;
                    break;
                }
            }
        }

        const std::int64_t loopEnd = std::max (from + 1, (std::int64_t) std::llround (exactEnd));
        const std::int64_t to = std::min (blockEnd, loopEnd);
        collect (from, to, loopEnd, planEnd, blockStart, samplesPerQuarter, events, maxEvents, numEvents, pulses, maxPulses, numPulses);

        if (loopEnd > blockEnd)
            break;

        // Boundary reached: take over the switch. The new pass begins exactly at exactEnd;
        // the rounding remainder is kept in the anchor so nothing accumulates over many passes.
        if (planEnd)
            ++loop;
        else
            loop = 0;   // the new plan starts with its first pass
        applyPending();
        anchorSample = loopEnd;
        anchorQ = startAt + ((double) loopEnd - exactEnd) / samplesPerQuarter;   // startAt: a seek into the new plan
        startAt = 0.0;
        loopSpq = samplesPerQuarter;
        from = loopEnd;
    }

    clock = blockEnd;
}

} // namespace hz
