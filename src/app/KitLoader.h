#pragma once

#include "engine/SamplePlayer.h"

#include <juce_core/juce_core.h>

#include <memory>

// Loads a pre-mixed kit (kit.json + stereo WAVs) and converts it to the target sample rate.
// Instruments are loaded in parallel (one thread per core).
std::unique_ptr<hz::DrumKit> loadKit (const juce::File& kitDir, double targetSampleRate, juce::String& error);

// Looks for a folder (e.g. "kits", "library") next to the program or in parent folders (development tree)
juce::File findAppFolder (const juce::String& name);
inline juce::File findKitsFolder() { return findAppFolder ("kits"); }
