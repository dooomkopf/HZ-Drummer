#include "engine/MeterCatalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace hz
{

namespace
{

void compositions (int rest, const std::vector<int>& parts, std::vector<int>& current, std::vector<std::vector<int>>& out)
{
    if (rest == 0)
    {
        if ((int) current.size() <= Meter::maxGroups)
            out.push_back (current);
        return;
    }

    for (int p : parts)
    {
        if (p > rest)
            continue;
        current.push_back (p);
        compositions (rest - p, parts, current, out);
        current.pop_back();
    }
}

std::vector<std::vector<int>> groupingsFor (int numerator, const std::vector<int>& parts)
{
    std::vector<std::vector<int>> out;
    std::vector<int> current;
    compositions (numerator, parts, current, out);

    std::sort (out.begin(), out.end(), [] (const auto& a, const auto& b)
    {
        if (a.size() != b.size())
            return a.size() < b.size();
        // same number of groups: "more even" arrangement with large groups at the back first (2+2+3 before 3+2+2)
        return a < b;
    });
    return out;
}

Meter fromGroups (int numerator, int denominator, const std::vector<int>& groups)
{
    Meter m;
    m.numerator = numerator;
    m.denominator = denominator;
    for (int g : groups)
        m.groups[(std::size_t) m.numGroups++] = (std::uint8_t) g;
    return m;
}

} // namespace

std::vector<Meter> buildMeterCatalog()
{
    std::vector<Meter> result;

    for (int n = 2; n <= 7; ++n)
        result.push_back (Meter::make (n, 4));

    for (int n = 3; n <= 13; ++n)
        for (const auto& g : groupingsFor (n, { 2, 3 }))
            result.push_back (fromGroups (n, 8, g));

    for (int n = 5; n <= 17; ++n)
    {
        auto groups = groupingsFor (n, { 3, 4 });
        if (groups.empty())
            groups = groupingsFor (n, { 2, 3 });
        for (const auto& g : groups)
            result.push_back (fromGroups (n, 16, g));
    }

    return result;
}

std::string groupingLabel (const Meter& m)
{
    std::string s;
    for (int i = 0; i < m.numGroups; ++i)
    {
        if (i > 0)
            s += '+';
        s += std::to_string (m.groups[(std::size_t) i]);
    }
    return s;
}

std::string meterLabel (const Meter& m)
{
    std::string s = std::to_string (m.numerator) + "/" + std::to_string (m.denominator);
    if (m.numGroups != m.numerator)   // only show real groupings, not 1+1+1+1
        s += " (" + groupingLabel (m) + ")";
    return s;
}

bool parseMeter (const std::string& text, Meter& result)
{
    // extract digits: numerator, denominator, then optional groups
    std::vector<int> numbers;
    int current = -1;
    for (char c : text + " ")
    {
        if (std::isdigit ((unsigned char) c))
            current = (current < 0 ? 0 : current * 10) + (c - '0');
        else if (current >= 0)
        {
            numbers.push_back (current);
            current = -1;
        }
    }

    if (numbers.size() < 2 || numbers[0] > 64)
        return false;

    Meter m;
    if (numbers.size() == 2)
    {
        m = Meter::make (numbers[0], numbers[1]);
    }
    else
    {
        if ((int) numbers.size() - 2 > Meter::maxGroups)
            return false;
        m = fromGroups (numbers[0], numbers[1], std::vector<int> (numbers.begin() + 2, numbers.end()));
    }

    if (! m.isValid())
        return false;

    result = m;
    return true;
}

bool sameMeter (const Meter& a, const Meter& b)
{
    if (a.numerator != b.numerator || a.denominator != b.denominator || a.numGroups != b.numGroups)
        return false;
    for (int i = 0; i < a.numGroups; ++i)
        if (a.groups[(std::size_t) i] != b.groups[(std::size_t) i])
            return false;
    return true;
}

} // namespace hz
