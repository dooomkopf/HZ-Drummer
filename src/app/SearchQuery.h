#pragma once

#include "engine/Pattern.h"

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

// The typed text, split into free words and recognised filters:
//   "7/8"  -> meter;  "180 bpm", "180bpm", "160-190 bpm", ">180 bpm", "<120 bpm" -> tempo range.
// A bare number stays a free word (it may be part of a name).
struct SearchQuery
{
    juce::String words;              // free text, all words must match
    juce::String meter;              // "7/8" or empty
    int tempoMin = 0, tempoMax = 0;  // inclusive; 0 = no tempo filter

    // Identical filtering and ranking in the main player and arranger.
    int score (const juce::String& title, const juce::String& haystack, double tempo,
               const std::vector<std::pair<int, int>>& meters, bool favorite) const;
    static SearchQuery parse (const juce::String& text);
    bool hasFilters() const { return meter.isNotEmpty() || tempoMin > 0; }
    juce::String meterChip() const  { return meter.isNotEmpty() ? "Taktart " + meter : juce::String(); }
    juce::String tempoChip() const;  // "Tempo 160–190" or ""
};

// The lanes a pattern uses, in the words one would type ("besen", "sidestick", "china", "flam"): search words
juce::StringArray laneSearchWords (const hz::Pattern&);
