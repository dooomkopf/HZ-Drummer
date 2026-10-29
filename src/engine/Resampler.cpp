#include "engine/Resampler.h"

#include <algorithm>
#include <cmath>

namespace hz
{

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int halfTaps = 24;      // filter length per side (in input samples when upsampling)
    constexpr int phases = 512;       // resolution of the precomputed filter table

    double sinc (double x) { return x == 0.0 ? 1.0 : std::sin (pi * x) / (pi * x); }

    // Blackman window on [-1, 1]
    double window (double x) { return std::abs (x) >= 1.0 ? 0.0 : 0.42 + 0.5 * std::cos (pi * x) + 0.08 * std::cos (2.0 * pi * x); }
}

std::vector<float> resample (const float* input, int numInput, double inputRate, double outputRate)
{
    if (numInput <= 0 || inputRate <= 0.0 || outputRate <= 0.0)
        return {};

    if (std::abs (inputRate - outputRate) < 1.0e-9)
        return std::vector<float> (input, input + numInput);

    const double step = inputRate / outputRate;              // input samples per output sample
    const double cutoff = std::min (1.0, 1.0 / step) * 0.95; // relative to input Nyquist; lower when downsampling
    const double span = halfTaps / cutoff;                   // filter width in input samples
    const int radius = (int) std::ceil (span);

    // Tabulate the filter kernel at "phases" points per input sample
    const int tableSize = radius * phases + 2;
    std::vector<float> table ((size_t) tableSize);
    for (int i = 0; i < tableSize; ++i)
    {
        const double d = (double) i / phases;
        table[(size_t) i] = (float) (cutoff * sinc (cutoff * d) * window (d / span));
    }

    auto kernel = [&] (double distance)
    {
        const double pos = std::abs (distance) * phases;
        const int i = (int) pos;
        if (i + 1 >= tableSize)
            return 0.0f;
        const float frac = (float) (pos - i);
        return table[(size_t) i] + frac * (table[(size_t) i + 1] - table[(size_t) i]);
    };

    // Time axes aligned: output j lies exactly at input j*step -> no delay.
    // The output is long enough to include the filter tail.
    const int numOutput = (int) std::ceil ((numInput + radius) / step);
    std::vector<float> out ((size_t) numOutput);

    for (int j = 0; j < numOutput; ++j)
    {
        const double t = j * step;
        const int centre = (int) std::floor (t);
        const int lo = std::max (0, centre - radius + 1);
        const int hi = std::min (numInput - 1, centre + radius);

        float sum = 0.0f;
        for (int k = lo; k <= hi; ++k)
            sum += input[k] * kernel (t - k);

        out[(size_t) j] = sum;
    }

    return out;
}

} // namespace hz
