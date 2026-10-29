#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace hz
{

// Meter with grouping, e.g. 7/8 = 2+2+3.
// A "pulse" is one note-value unit of the denominator (in 7/8 that is an eighth).
struct Meter
{
    static constexpr int maxGroups = 16;

    enum class Accent : std::uint8_t { Downbeat, Group, Weak };

    int numerator = 4;
    int denominator = 4;
    std::array<std::uint8_t, maxGroups> groups {};
    int numGroups = 0;

    // Empty grouping -> sensible default (6/8 = 3+3, 7/8 = 2+2+3, 4/4 = 1+1+1+1)
    static Meter make (int numerator, int denominator, std::initializer_list<int> grouping = {});

    bool isValid() const;
    int pulsesPerBar() const { return numerator; }
    double quartersPerPulse() const { return 4.0 / denominator; }
    Accent accentAt (int pulse) const;
};

} // namespace hz
