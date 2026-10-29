#pragma once

#include "engine/Pattern.h"

#include <string>
#include <vector>

namespace hz
{

// Drum tab format (authoring and import format), several patterns per text separated by "---":
//
//   name:     Thrash Skank
// id:       lib.thrash.skank          (optional, otherwise derived from the name)
//   tags:     thrash, skank
//   tempo:    220 (180-250)
// meter:    4/4                       (one per bar, comma-separated; the last applies to the rest)
// grid:     1/8                       (note value per character; 1/12 = eighth triplets; per bar possible)
// swing:    1/8 0.62                  (optional)
// humanize: 0.3                       (optional)
//
//   cr | X . . . . . . . |
//   sn | . x . x . x . x |
//   bd | x . x . x . x . |
//
// Tracks: bd (kick), bl/br (kick left/right), sn, sr (rim), ss (sidestick), hh, hp (hi-hat pedal),
//         rd (ride), rb (ride bell), cr/c1, c2, ch (china), sp (splash), t1..t4, cb (cowbell)
// Characters: . or - rest, g ghost, x normal, X accent, f/F flam, o/O hi-hat open, h hi-hat half-open
// Multiple bars: further |...| blocks or follow-up lines of the same track. # starts comments.
// Polyrhythms: "cr(3) | X x x |" splits this track's bar into 3 equal steps (3 against 2/4 ...).
struct TabParseResult
{
    std::vector<Pattern> patterns;
    std::string error;   // empty = OK; otherwise with line number
};

TabParseResult parseTabs (const std::string& text);

} // namespace hz
