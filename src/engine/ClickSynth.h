#pragma once

#include "engine/Sequencer.h"

#include <array>

namespace hz
{

// Synthetic metronome click (placeholder until the sample player).
// Fixed voice count, no allocation on the audio thread.
class ClickSynth
{
public:
    void prepare (double sampleRate);

    // Adds into the buffers (events must be sorted by sampleOffset); right may be nullptr (mono)
    void render (float* left, float* right, int numSamples, const ClickEvent* events, int numEvents);

    void setGain (float g) { gain = g; }

    // Highest output level since the last resetPeak() (for the level meter)
    float peak() const { return peakLevel; }
    void resetPeak() { peakLevel = 0.0f; }

private:
    struct Voice
    {
        double phase = 0.0;
        double phaseInc = 0.0;
        float amp = 0.0f;
    };

    void trigger (Meter::Accent accent);

    static constexpr int numVoices = 8;
    std::array<Voice, numVoices> voices {};
    int nextVoice = 0;
    double sampleRate = 48000.0;
    float decay = 0.999f;
    float gain = 0.5f;
    float peakLevel = 0.0f;
};

} // namespace hz
