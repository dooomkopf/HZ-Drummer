#pragma once

#include "engine/Meter.h"

#include <string>
#include <vector>

namespace hz
{

// All meters for the menu, ordered by denominator, numerator and grouping:
//  - x/4:  2/4 .. 7/4, single beats
//  - x/8:  3/8 .. 13/8, all decompositions into 2s and 3s (7/8 = 2+2+3 | 2+3+2 | 3+2+2)
//  - x/16: 5/16 .. 17/16, decompositions into 3s and 4s (if none possible: 2s and 3s)
// Within one numerator: few groups first, then large groups first (9/8: 3+3+3, 2+2+2+3, ...)
std::vector<Meter> buildMeterCatalog();

// "4/4", "7/8 (2+2+3)"
std::string meterLabel (const Meter&);

// Grouping as text, e.g. "2+2+3"
std::string groupingLabel (const Meter&);

// Reads "7/8", "7/8 (2+2+3)", "17/16:4+4+4+5" or "17/16 4+4+4+5"; false on invalid input
bool parseMeter (const std::string& text, Meter& result);

bool sameMeter (const Meter&, const Meter&);

} // namespace hz
