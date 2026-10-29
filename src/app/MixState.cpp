#include "app/MixState.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace mix
{

namespace
{
    const char* const names[] = { "Kick", "Snare", "HiHat", "Toms", "Becken", "Ride", "Klick", "Gesamt" };
    const char* const keys[]  = { "kick", "snare", "hihat", "toms", "cymbals", "ride", "click", "master" };
    constexpr float minDb = -48.0f;
}

int channelFor (hz::Voice v)
{
    using I = hz::Instrument;
    switch (v.instrument)
    {
        case I::Kick:  return Kick;
        case I::Snare: return Snare;
        case I::HiHat: return HiHat;
        case I::Ride:  return Ride;
        case I::Tom1: case I::Tom2: case I::Tom3: case I::Tom4: return Toms;
        case I::Crash1: case I::Crash2: case I::China: case I::Splash: case I::Cowbell: case I::Count: break;
    }
    return Cymbals;
}

std::array<bool, NumChannels> channelsUsedBy (const hz::PatternPlan& plan)
{
    std::array<bool, NumChannels> used {};
    for (const auto& e : plan.events)
        used[(size_t) channelFor (e.voice)] = true;
    for (const auto& b : plan.bars)
        used[Click] = used[Click] || b.click;
    used[Master] = true;
    return used;
}

std::uint64_t pack (const Settings& m)
{
    // solo: as soon as one channel is soloed, all others (except master) are silent
    bool anySolo = false;
    for (int c = 0; c < NumChannels; ++c)
        anySolo = anySolo || (c != Master && m[(size_t) c].solo);

    std::uint64_t packed = 0;
    for (int c = 0; c < NumChannels; ++c)
    {
        const auto& s = m[(size_t) c];
        const bool silent = s.muted || (anySolo && c != Master && ! s.solo);
        const int step = silent ? 0 : juce::jlimit (1, 255, juce::roundToInt ((s.db - minDb) * 2.0f) + 1);
        packed |= (std::uint64_t) step << (8 * c);
    }
    return packed;
}

float gainOf (std::uint64_t packed, int channel)
{
    const int step = (int) ((packed >> (8 * channel)) & 0xff);
    return step == 0 ? 0.0f : juce::Decibels::decibelsToGain ((float) (step - 1) * 0.5f + minDb, minDb);
}

const char* channelName (int channel) { return names[channel]; }
const char* channelKey (int channel)  { return keys[channel]; }

} // namespace mix
