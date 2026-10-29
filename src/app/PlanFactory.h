#pragma once

#include "app/Song.h"
#include "engine/Pattern.h"

#include <juce_core/juce_core.h>

#include <map>
#include <memory>
#include <vector>

class Library;
class MeterLibrary;

// Builds the immutable PatternPlans that the AudioEngine plays, and remembers per plan what the
// displays need (title, origin, bars/events, song parts). All on the message thread.

enum class MetronomeSound { Click, Kick, Snare, KickSnare };

std::unique_ptr<hz::PatternPlan> makeMetronomePlan (const hz::Meter&, MetronomeSound);
std::unique_ptr<hz::PatternPlan> makePatternPlan (const hz::Pattern&);

// Songs: every part is compiled with its own swing/humanize settings, then the plans are joined.
// Returns nullptr if a referenced pattern is missing.
std::unique_ptr<hz::PatternPlan> makeSongPlan (const Song&, const Library&);

// Everything the UI knows about a plan that was sent to the engine
struct PlanInfo
{
    juce::String title;
    std::string patternId;               // PatternPlan::patternId ("song.<id>" for songs)
    bool fromEditor = false;
    std::vector<hz::PlanBar> bars;       // copy for the beat display
    std::vector<hz::PlanEvent> events;
    std::vector<SongSpan> spans;         // songs only: parts and repeats
};

// Gives every plan a unique tag (sent along with it) and keeps its PlanInfo, so the display always
// shows what is actually audible, even after the library changed.
class PlanRegistry
{
public:
    // assigns plan->tag, fills meter tags for the display, stores the info
    void registerPlan (hz::PatternPlan&, MeterLibrary&, PlanInfo info);

    const PlanInfo* find (int tag) const;
    const PlanInfo* latest() const       { return find (lastTag); }
    int latestTag() const                { return lastTag; }

private:
    static constexpr size_t maxKept = 256;
    int nextTag = 1, lastTag = 0;
    std::map<int, PlanInfo> infos;
};
