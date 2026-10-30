#pragma once

#include "engine/SamplePlayer.h"

#include <juce_core/juce_core.h>

#include <future>
#include <memory>
#include <vector>

class AudioEngine;

// Owns the drum kits: loads them in the background at the device sample rate, hands them to the
// AudioEngine and frees a replaced kit only after the audio thread has stopped using it.
// All methods run on the message thread.
class KitManager
{
public:
    explicit KitManager (juce::File kitFolder);
    ~KitManager();

    // Call regularly (timer). Starts (re)loads when needed and publishes finished kits to the engine.
    // Returns true if the status text changed.
    bool poll (AudioEngine&);

    const juce::String& status() const { return statusText; }
    const hz::DrumKit* kit() const     { return current.get(); }   // the kit in use (nullptr while none is loaded)

    // first folder in kits/ that contains a kit.json (alphabetically); invalid if none
    static juce::File findDefaultKit();

private:
    struct LoadResult
    {
        std::unique_ptr<hz::DrumKit> kit;
        juce::String error;
        double seconds = 0.0;
    };

    void startLoad (double sampleRate);

    juce::File folder;
    std::unique_ptr<hz::DrumKit> current;
    std::vector<std::unique_ptr<hz::DrumKit>> retired;   // waiting until the audio thread lets go
    std::future<LoadResult> pending;
    juce::String statusText;
};
