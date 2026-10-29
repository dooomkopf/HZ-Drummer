#pragma once

#include "app/PlanFactory.h"
#include "engine/Meter.h"

#include <juce_core/juce_core.h>

#include <vector>

// What the main window's views show. Filled by MainComponent on the message thread.
// While playing it describes what is audible right now, otherwise the current selection.
struct PlaybackView
{
    bool live = false;                   // playing and at least one pulse has been heard
    bool paused = false;                 // stopped, but bar/pulse describe where playback will continue
    hz::Meter meter = hz::Meter::make (4, 4);
    int bar = -1, pulse = -1, loop = 0, numBars = 1;
    double pulseTimeMs = 0.0;            // wall-clock time at which 'pulse' was heard
    double tempo = 120.0;                // quarter-note BPM
    juce::String title;
    const PlanInfo* plan = nullptr;      // plan to visualise (may be null)
    const std::vector<SongSpan>* spans = nullptr;   // song parts, or null

    double quartersPerPulse() const      { return meter.quartersPerPulse(); }
    double msPerQuarter() const          { return 60000.0 / tempo; }

    // progress inside the current pulse (0..1), extrapolated from the last pulse time
    double pulsePhase() const
    {
        if (! live)
            return 0.0;
        const double ms = juce::Time::getMillisecondCounterHiRes() - pulseTimeMs;
        return juce::jlimit (0.0, 1.0, ms / (msPerQuarter() * quartersPerPulse()));
    }

    // progress inside the current bar (0..1)
    double barFraction() const
    {
        if (paused)
            return juce::jlimit (0.0, 0.999, (double) juce::jmax (0, pulse) / (double) meter.numerator);
        return live ? juce::jlimit (0.0, 0.999, (pulse + pulsePhase()) / (double) meter.numerator) : 0.0;
    }

    bool hasPosition() const             { return live || paused; }

    // position in bars from the start of the plan, or -1 if there is none
    double position() const              { return hasPosition() ? bar + barFraction() : -1.0; }
};
