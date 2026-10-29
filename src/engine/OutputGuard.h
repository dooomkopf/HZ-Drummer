#pragma once

#include <algorithm>
#include <cmath>

namespace hz
{

// Last stage before the sound card, protects the speakers:
//  - non-finite samples (NaN/inf from a bug) become silence
//  - a stereo-linked peak limiter: instant attack, smooth release, ceiling slightly below full scale
//  - a hard clamp as the final backstop
// Normal playing stays far below the ceiling, so the limiter only acts on faults or extreme mixes.
// Realtime-safe, no allocation.
class OutputGuard
{
public:
    static constexpr float ceiling = 0.89f;   // about -1 dBFS

    void prepare (double sampleRate)
    {
        // gain recovers with a time constant of about 80 ms
        release = (float) std::exp (-1.0 / (0.080 * sampleRate));
        gain = 1.0f;
    }

    void process (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = std::isfinite (left[i]) ? left[i] : 0.0f;
            float r = right != nullptr && std::isfinite (right[i]) ? right[i] : 0.0f;

            const float peak = std::max (std::abs (l), std::abs (r));
            const float needed = peak > ceiling ? ceiling / peak : 1.0f;   // largest gain that stays below the ceiling
            gain = needed < gain ? needed                                   // attack: at once
                                 : std::min (needed, 1.0f - (1.0f - gain) * release);   // release: smooth
            if (gain < 0.999f)
                ++limitedSamples;

            left[i] = std::clamp (l * gain, -1.0f, 1.0f);
            if (right != nullptr)
                right[i] = std::clamp (r * gain, -1.0f, 1.0f);
        }
    }

    float currentGain() const { return gain; }
    long long limited() const { return limitedSamples; }   // diagnostics: samples with gain reduction

private:
    float release = 0.9997f;
    float gain = 1.0f;
    long long limitedSamples = 0;
};

}
