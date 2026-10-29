#include "engine/Meter.h"

namespace hz
{

Meter Meter::make (int numerator, int denominator, std::initializer_list<int> grouping)
{
    Meter m;
    m.numerator = numerator;
    m.denominator = denominator;

    auto add = [&m] (int g) { if (m.numGroups < maxGroups) m.groups[(std::size_t) m.numGroups++] = (std::uint8_t) g; };

    if (grouping.size() > 0)
    {
        for (int g : grouping)
            add (g);
    }
    else if (denominator <= 4)
    {
        for (int i = 0; i < numerator; ++i)
            add (1);
    }
    else if (numerator % 3 == 0 && numerator > 3)
    {
        for (int i = 0; i < numerator / 3; ++i)
            add (3);
    }
    else
    {
        // odd eighths/sixteenths: pairs, with a single triple at the end (5 = 2+3, 7 = 2+2+3, 11 = 2+2+2+2+3)
        int rest = numerator;
        while (rest > 3) { add (2); rest -= 2; }
        if (rest > 0) add (rest);
    }

    return m;
}

bool Meter::isValid() const
{
    if (numerator < 1 || numerator > 64 || numGroups < 1)
        return false;

    if (denominator != 1 && denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16 && denominator != 32)
        return false;

    int sum = 0;
    for (int i = 0; i < numGroups; ++i)
        sum += groups[(std::size_t) i];

    return sum == numerator;
}

Meter::Accent Meter::accentAt (int pulse) const
{
    if (pulse == 0)
        return Accent::Downbeat;

    int start = 0;
    for (int i = 0; i < numGroups && start <= pulse; ++i)
    {
        if (start == pulse)
            return Accent::Group;
        start += groups[(std::size_t) i];
    }

    return Accent::Weak;
}

} // namespace hz
