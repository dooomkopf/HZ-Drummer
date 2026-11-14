#pragma once

#include <deque>
#include <functional>
#include <vector>

// Loudness for the export (MP3, variant B): the integrated loudness of ITU-R BS.1770-4 / EBU R 128 in LUFS, and a
// look-ahead peak limiter that holds every sample at or below a ceiling. Both stereo, both offline (they allocate).
class LoudnessMeter
{
public:
    explicit LoudnessMeter (double sampleRate);

    void add (const float* left, const float* right, int numSamples);
    double integrated() const;   // LUFS, gated (absolute -70, relative -10 LU); -70 when nothing is loud enough

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    std::vector<Biquad> filters;     // K-weighting: shelf, then high-pass; for left, then right
    std::vector<double> blocks;      // energy of every 100 ms (left + right, weighted)
    double energy = 0.0;
    int counted = 0, blockLength = 4800;
};

// Brick-wall limiter with look-ahead: the gain falls in a straight line over the look-ahead to exactly what the coming
// peak needs, and rises again with the release time. The output is the input, later by the look-ahead minus one sample;
// process() leaves that delay out (the first samples are not emitted), flush() emits the rest: the length stays.
class PeakLimiter
{
public:
    using Emit = std::function<bool (const float* left, const float* right, int numSamples)>;

    PeakLimiter (double sampleRate, float ceiling, double lookAheadSeconds = 0.005, double releaseSeconds = 0.1);

    bool process (const float* left, const float* right, int numSamples, const Emit&);
    bool flush (const Emit&);

private:
    void push (float l, float r);

    float ceiling;
    int lookAhead;
    double release;
    std::deque<std::pair<long long, float>> minimum;   // falling gains of the window (index, needed gain)
    std::vector<float> window, delayL, delayR;          // the last look-ahead gains of the minimum; the delayed input
    double windowSum = 0.0, gain = 1.0;
    long long index = 0;
    std::vector<float> outL, outR;
};
