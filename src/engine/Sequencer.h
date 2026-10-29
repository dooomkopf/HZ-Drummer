#pragma once

#include "engine/Meter.h"

namespace hz
{

struct ClickEvent
{
    int sampleOffset = 0;   // Position within the current block
    Meter::Accent accent = Meter::Accent::Weak;
    int bar = 0;
    int pulse = 0;
    int meterTag = 0;       // ID of the meter assigned by the caller (e.g. preset index)
};

// Sample-accurate bar/pulse generator. Not thread-safe: used exclusively by the audio thread.
// No allocations, no locks.
class Sequencer
{
public:
    static constexpr double minTempo = 20.0;
    static constexpr double maxTempo = 400.0;

    void prepare (double sampleRate);

    // Tempo in quarters per minute; applies from the next pulse
    void setTempo (double quarterBpm);
    double getTempo() const { return tempo; }

    // Meter change takes effect at the next bar start (or immediately on start)
    void setMeter (const Meter& meter, int tag);

    void start();
    void stop();
    bool isPlaying() const { return playing; }

    // Delivers the pulses that fall into this block
    int process (int numSamples, ClickEvent* out, int maxEvents);

private:
    double samplesPerPulse() const;
    void applyPendingMeter();

    double sampleRate = 48000.0;
    double tempo = 120.0;
    Meter meter = Meter::make (4, 4);
    int meterTag = 0;
    Meter pendingMeter;
    int pendingTag = 0;
    bool hasPending = false;

    bool playing = false;
    double samplesToNextPulse = 0.0;
    int bar = 0;
    int pulse = 0;
};

} // namespace hz
