#pragma once

#include "engine/Pattern.h"

#include <juce_core/juce_core.h>

#include <vector>

// Pattern <-> JSON (binding storage format, formatVersion 1)
juce::var patternToJson (const hz::Pattern&);
bool patternFromJson (const juce::var&, hz::Pattern&, juce::String& error);

bool savePattern (const hz::Pattern&, const juce::File&, juce::String& error);
bool loadPattern (const juce::File&, hz::Pattern&, juce::String& error);

// All *.json files in a folder (recursive); faulty files end up in errors
std::vector<hz::Pattern> loadPatternFolder (const juce::File& folder, juce::StringArray& errors);
