#include "app/Loudness.h"

#include <algorithm>
#include <cmath>

LoudnessMeter::LoudnessMeter (double sampleRate)
{
    // the K-weighting of BS.1770 for any sample rate (coefficients as derived in libebur128)
    const double pi = 3.14159265358979323846;
    Biquad shelf, highPass;
    {
        const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const double k = std::tan (pi * f0 / sampleRate), vh = std::pow (10.0, gainDb / 20.0), vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;
        shelf.b0 = (vh + vb * k / q + k * k) / a0;
        shelf.b1 = 2.0 * (k * k - vh) / a0;
        shelf.b2 = (vh - vb * k / q + k * k) / a0;
        shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        shelf.a2 = (1.0 - k / q + k * k) / a0;
    }
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double k = std::tan (pi * f0 / sampleRate), a0 = 1.0 + k / q + k * k;
        highPass.b0 = 1.0;
        highPass.b1 = -2.0;
        highPass.b2 = 1.0;
        highPass.a1 = 2.0 * (k * k - 1.0) / a0;
        highPass.a2 = (1.0 - k / q + k * k) / a0;
    }
    filters = { shelf, highPass, shelf, highPass };
    blockLength = std::max (1, (int) std::lround (0.1 * sampleRate));
}

void LoudnessMeter::add (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double l = filters[1].process (filters[0].process (left[i]));
        const double r = filters[3].process (filters[2].process (right[i]));
        energy += l * l + r * r;
        if (++counted == blockLength)
        {
            blocks.push_back (energy);
            energy = 0.0;
            counted = 0;
        }
    }
}

double LoudnessMeter::integrated() const
{
    // gating blocks of 400 ms every 100 ms
    std::vector<double> z;
    for (size_t j = 0; j + 4 <= blocks.size(); ++j)
        z.push_back ((blocks[j] + blocks[j + 1] + blocks[j + 2] + blocks[j + 3]) / (4.0 * blockLength));
    const auto loudness = [] (double meanSquare) { return -0.691 + 10.0 * std::log10 (std::max (meanSquare, 1.0e-20)); };
    const auto gatedMean = [&] (double threshold)
    {
        double sum = 0.0;
        int n = 0;
        for (const double v : z)
            if (loudness (v) > threshold)
            {
                sum += v;
                ++n;
            }
        return n > 0 ? sum / n : 0.0;
    };
    const double absolute = gatedMean (-70.0);
    if (absolute <= 0.0)
        return -70.0;
    const double relative = gatedMean (std::max (-70.0, loudness (absolute) - 10.0));
    return relative > 0.0 ? loudness (relative) : -70.0;
}

PeakLimiter::PeakLimiter (double sampleRate, float ceilingGain, double lookAheadSeconds, double releaseSeconds)
    : ceiling (ceilingGain),
      lookAhead (std::max (1, (int) std::lround (lookAheadSeconds * sampleRate))),
      release (1.0 - std::exp (-1.0 / std::max (1.0, releaseSeconds * sampleRate)))
{
    window.assign ((size_t) lookAhead, 1.0f);
    delayL.assign ((size_t) lookAhead, 0.0f);
    delayR.assign ((size_t) lookAhead, 0.0f);
    windowSum = lookAhead;
}

void PeakLimiter::push (float l, float r)
{
    // the gain sample n needs; the minimum over the last lookAhead of them belongs to sample n - lookAhead + 1, and the
    // average of the last lookAhead minima is never above what any sample in reach needs: the ceiling holds
    const float level = std::max (std::abs (l), std::abs (r));
    const float needed = level > ceiling ? ceiling / level : 1.0f;
    while (! minimum.empty() && minimum.back().second >= needed)
        minimum.pop_back();
    minimum.emplace_back (index, needed);
    while (minimum.front().first <= index - lookAhead)
        minimum.pop_front();
    const auto slot = (size_t) (index % lookAhead);
    windowSum += minimum.front().second - window[slot];
    window[slot] = minimum.front().second;
    const double target = std::min (1.0, windowSum / lookAhead);
    gain = target < gain ? target : gain + (target - gain) * release;

    // the delayed sample lookAhead - 1 back
    const auto out = (size_t) ((index + 1) % lookAhead);
    const float dl = lookAhead > 1 ? delayL[out] : l, dr = lookAhead > 1 ? delayR[out] : r;
    delayL[slot] = l;
    delayR[slot] = r;
    if (index >= lookAhead - 1)
    {
        outL.push_back (dl * (float) gain);
        outR.push_back (dr * (float) gain);
    }
    ++index;
}

bool PeakLimiter::process (const float* left, const float* right, int numSamples, const Emit& emit)
{
    outL.clear();
    outR.clear();
    for (int i = 0; i < numSamples; ++i)
        push (left[i], right[i]);
    return outL.empty() || emit (outL.data(), outR.data(), (int) outL.size());
}

bool PeakLimiter::flush (const Emit& emit)
{
    outL.clear();
    outR.clear();
    for (int i = 0; i < lookAhead - 1; ++i)
        push (0.0f, 0.0f);
    return outL.empty() || emit (outL.data(), outR.data(), (int) outL.size());
}
