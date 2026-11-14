#pragma once

#include "engine/Pattern.h"

#include <memory>

// What the export button writes (~/HZ-Drummer-Export): the selection as one plan that is played once from its start.
// A pattern or a loop range repeats, a bar of clicks may count in, and empty bars follow the music so that its last
// hits ring out inside the plan (the player never wraps around into the first bar again).
struct ExportSpan
{
    double from = 0.0, to = 0.0;   // quarters in the source plan (a loop range); to <= from: all of it
    int repeats = 1;               // passes of the span
    bool click = false;            // a click on every pulse of the music ("mit Klick")
    bool countIn = false;          // one bar of clicks in the meter of the first bar first ("mit Einzählen")
};

struct ExportTimeline
{
    std::unique_ptr<hz::PatternPlan> plan;   // nullptr: the source had no bars in the span
    double musicStart = 0.0;                 // quarters: where the count-in ends
    double musicEnd = 0.0;                   // quarters: where the last pass ends and the ringing out begins
};

// tailSeconds: how long the empty bars after the music last at this tempo (at least)
ExportTimeline makeExportTimeline (const hz::PatternPlan& source, const ExportSpan&, double tempo, double tailSeconds);
