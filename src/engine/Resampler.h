#pragma once

#include <vector>

namespace hz
{

// Offline sample-rate conversion (windowed sinc with anti-aliasing, zero delay).
// Meant for kit preparation and loading, not for the audio thread.
std::vector<float> resample (const float* input, int numInput, double inputRate, double outputRate);

} // namespace hz
