#include "engine/SamplePlayer.h"

#include <algorithm>
#include <cmath>

namespace hz
{

namespace
{
    // Quietest playable target loudness relative to the quietest recording (for ghost notes, approx. -10 dB)
    constexpr float floorFactor = 0.1f;
}

void DrumKit::finalise()
{
    for (auto& inst : instruments)
    {
        auto& s = inst.samples;
        std::sort (s.begin(), s.end(), [] (const DrumSample& a, const DrumSample& b) { return a.power < b.power; });

        // Missing loudness values: distribute by order
        if (! s.empty() && s.front().power <= 0.0f)
            for (std::size_t i = 0; i < s.size(); ++i)
                s[i].power = (float) (i + 1) / (float) s.size();

        inst.minPower = s.empty() ? 0.0f : s.front().power;
        inst.maxPower = s.empty() ? 0.0f : s.back().power;
    }
}

int DrumKit::find (const std::string& instrumentName) const
{
    for (std::size_t i = 0; i < instruments.size(); ++i)
        if (instruments[i].name == instrumentName)
            return (int) i;
    return -1;
}

std::vector<std::string> DrumKit::resolveVoices (const std::vector<std::pair<std::string, std::string>>& mapping)
{
    voiceMap.fill (-1);
    kickLeft = -1;

    for (const auto& [voiceName, instrumentName] : mapping)
    {
        const int inst = find (instrumentName);
        if (inst < 0)
            continue;

        if (voiceName == "kick.left")
        {
            kickLeft = inst;
            continue;
        }

        Voice v;
        if (parseVoiceKey (voiceName, v))
            voiceMap[(std::size_t) voiceSlot (v)] = inst;
    }

    // Fallback rules: only without change of character, cycle-free (always towards the "Normal" articulation)
    const std::pair<Voice, Voice> fallbacks[] = {
        { { Instrument::HiHat, Articulation::Half }, { Instrument::HiHat, Articulation::Normal } },
        { { Instrument::Snare, Articulation::Rim },  { Instrument::Snare, Articulation::Normal } },
    };
    for (const auto& [from, to] : fallbacks)
        if (voiceMap[(std::size_t) voiceSlot (from)] < 0)
            voiceMap[(std::size_t) voiceSlot (from)] = voiceMap[(std::size_t) voiceSlot (to)];

    std::vector<std::string> missing;
    for (int i = 0; i < (int) Instrument::Count; ++i)
        if (const Voice v { (Instrument) i, Articulation::Normal }; voiceMap[(std::size_t) voiceSlot (v)] < 0)
            missing.push_back (voiceKey (v));
    return missing;
}

int DrumKit::instrumentFor (Voice v, bool leftFoot) const
{
    if (leftFoot && v.instrument == Instrument::Kick && kickLeft >= 0)
        return kickLeft;
    return voiceMap[(std::size_t) voiceSlot (v)];
}

VelocityPick pickForVelocity (const DrumInstrument& inst, float velocity, int lastPicked, std::uint32_t random)
{
    const auto& samples = inst.samples;
    const int n = (int) samples.size();
    if (n == 0)
        return {};

    // Velocity curve: logarithmic over the dynamic range of the recordings,
    // below the quietest recording it only gets quieter (ghost notes)
    const float v = std::clamp (velocity, 0.0f, 1.0f);
    const float lo = std::log (inst.minPower * floorFactor);
    const float hi = std::log (inst.maxPower);
    const float target = std::exp (lo + v * (hi - lo));

    // closest recording (distance on a log scale)
    int best = 0;
    float bestDist = 1.0e9f;
    for (int i = 0; i < n; ++i)
    {
        const float d = std::abs (std::log (samples[(std::size_t) i].power) - std::log (target));
        if (d < bestDist) { bestDist = d; best = i; }
    }

    // Round-robin only within the direct neighbourhood so the volume layer is preserved
    const int spread = n >= 12 ? 2 : (n >= 4 ? 1 : 0);
    const int from = std::max (0, best - spread);
    const int to = std::min (n - 1, best + spread);
    int pick = from + (int) (random % (std::uint32_t) (to - from + 1));
    if (pick == lastPicked && to > from)
        pick = pick == to ? from : pick + 1;

    // Fine correction: power is an energy measure -> amplitude ~ square root
    const float g = std::sqrt (target / samples[(std::size_t) pick].power);
    return { pick, std::clamp (g, 0.05f, 1.5f) };
}

void SamplePlayer::prepare (double sampleRate)
{
    fadeStep = (float) (1.0 / std::max (1.0, 0.003 * sampleRate));
}

void SamplePlayer::setKit (const DrumKit* newKit)
{
    kit = newKit;
    lastPicked.fill (-1);
    voices = {};
    faders = {};
}

std::uint32_t SamplePlayer::nextRandom()
{
    // xorshift32: fast, deterministic, realtime-safe
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

void SamplePlayer::setChannelGain (int channel, float g)
{
    if (channel >= 0 && channel < maxChannels)
        channelTarget[(std::size_t) channel] = g;
}

void SamplePlayer::trigger (int instrument, float velocity, int sampleOffset, int channel)
{
    if (kit == nullptr || instrument < 0 || instrument >= std::min ((int) kit->instruments.size(), maxInstruments))
        return;

    auto& last = lastPicked[(std::size_t) instrument];
    const auto pick = pickForVelocity (kit->instruments[(std::size_t) instrument], velocity, last, nextRandom());
    if (pick.sample < 0)
        return;
    last = pick.sample;

    // prefer a free voice, otherwise steal the oldest (quietest in its tail)
    int slot = -1;
    std::uint64_t oldest = ~0ull;
    for (int i = 0; i < numVoices; ++i)
    {
        if (voices[(std::size_t) i].sample == nullptr) { slot = i; break; }
        if (voiceAge[(std::size_t) i] < oldest) { oldest = voiceAge[(std::size_t) i]; slot = i; }
    }

    auto& v = voices[(std::size_t) slot];
    if (v.sample != nullptr)
    {
        // don't cut a stolen voice off hard, fade it out briefly from the start of the new hit
        auto& f = faders[(std::size_t) nextFader];
        nextFader = (nextFader + 1) % numFaders;
        f = v;
        f.fadeStart = std::max (0, sampleOffset);
        f.fadeLevel = 1.0f;
    }

    v.sample = &kit->instruments[(std::size_t) instrument].samples[(std::size_t) pick.sample];
    v.position = 0;
    v.startOffset = std::max (0, sampleOffset);
    v.gain = pick.gain;
    v.channel = std::clamp (channel, 0, maxChannels - 1);
    voiceAge[(std::size_t) slot] = ++triggerCount;
}

void SamplePlayer::renderVoice (Voice& v, float* left, float* right, int numSamples, bool fading)
{
    const auto& s = *v.sample;
    const int len = (int) s.left.size();

    // channel gain ramps linearly across the block from the previous to the new target
    const float c0 = channelPrevious[(std::size_t) v.channel], c1 = channelTarget[(std::size_t) v.channel];
    const float rampStep = (c1 - c0) / (float) std::max (1, numSamples);

    for (int i = v.startOffset; i < numSamples && v.position < len; ++i, ++v.position)
    {
        float level = v.gain * gain * (c0 + rampStep * (float) i);
        if (fading && i >= v.fadeStart)
        {
            v.fadeLevel -= fadeStep;
            if (v.fadeLevel <= 0.0f)
            {
                v.position = len;
                break;
            }
            level *= v.fadeLevel;
        }

        left[i] += level * s.left[(std::size_t) v.position];
        if (right != nullptr)
            right[i] += level * s.right[(std::size_t) v.position];
    }

    v.startOffset = 0;
    v.fadeStart = 0;
    if (v.position >= len)
        v.sample = nullptr;
}

void SamplePlayer::render (float* left, float* right, int numSamples)
{
    for (auto& f : faders)
        if (f.sample != nullptr)
            renderVoice (f, left, right, numSamples, true);

    for (auto& v : voices)
        if (v.sample != nullptr)
            renderVoice (v, left, right, numSamples, false);

    channelPrevious = channelTarget;
}

int SamplePlayer::getNumActiveVoices() const
{
    int n = 0;
    for (auto& v : voices)
        if (v.sample != nullptr)
            ++n;
    return n;
}

} // namespace hz
