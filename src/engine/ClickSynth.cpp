#include "engine/ClickSynth.h"

#include <algorithm>
#include <cmath>

namespace hz
{

namespace
{
    constexpr double twoPi = 6.283185307179586;
    constexpr float silence = 1.0e-4f;
}

void ClickSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    decay = (float) std::exp (-1.0 / (0.035 * sampleRate)); // approx. 35 ms decay time
    voices = {};
}

void ClickSynth::trigger (Meter::Accent accent)
{
    double freq = 900.0;
    float amp = 0.35f;

    switch (accent)
    {
        case Meter::Accent::Downbeat: freq = 1800.0; amp = 0.9f;  break;
        case Meter::Accent::Group:    freq = 1300.0; amp = 0.6f;  break;
        case Meter::Accent::Weak:     break;
    }

    auto& v = voices[(size_t) nextVoice];
    nextVoice = (nextVoice + 1) % numVoices;
    v.phase = 0.0;
    v.phaseInc = twoPi * freq / sampleRate;
    v.amp = amp;
}

void ClickSynth::render (float* left, float* right, int numSamples, const ClickEvent* events, int numEvents)
{
    int ev = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        while (ev < numEvents && events[ev].sampleOffset <= i)
            trigger (events[ev++].accent);

        float s = 0.0f;
        for (auto& v : voices)
        {
            if (v.amp < silence)
                continue;

            s += v.amp * (float) std::sin (v.phase);
            v.phase += v.phaseInc;
            v.amp *= decay;
        }

        s *= gain;
        peakLevel = std::max (peakLevel, std::abs (s));
        left[i] += s;
        if (right != nullptr)
            right[i] += s;
    }
}

} // namespace hz
