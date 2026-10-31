#pragma once

#include "engine/Pattern.h"

#include <array>
#include <cstdint>

namespace hz
{

// A hit to play within the current block
struct PlayEvent
{
    int sampleOffset = 0;
    Voice voice;
    float velocity = 0.75f;
    Limb limb = Limb::Auto;
};

// Beat for display and metronome click
struct PulseEvent
{
    int sampleOffset = 0;
    int bar = 0;           // Bar within the pattern
    int pulse = 0;         // Beat within the bar (denominator unit)
    int loop = 0;          // Pass since start
    int meterTag = 0;      // PlanBar::meterTag
    Meter::Accent accent = Meter::Accent::Weak;
    int numBars = 1;       // Bars in the running pattern
    int planTag = -1;      // PatternPlan::tag
    bool click = false;    // PlanBar::click: the engine sounds a click on this pulse
};

// Plays immutable PatternPlans sample-accurately. Runs exclusively on the audio thread:
// no allocations, no locks. Plans belong to the caller; superseded plans are returned via
// popRetired() and may only be deleted afterwards (outside the audio thread).
class PatternPlayer
{
public:
    static constexpr double maxHumanizeMs = 12.0;   // Time offset at humanize 1.0
    static constexpr float maxHumanizeVelocity = 0.15f;

    void prepare (double sampleRate);
    void setTempo (double quarterBpm);
    void setHumanizeScale (float s) { humanizeScale = s; }   // global control 0..1, multiplied by the pattern value

    // New plan: takes effect immediately if nothing is running, otherwise at the end of the current pass.
    // A waiting, never-played plan is superseded in the process.
    void setNextPlan (const PatternPlan* plan);

    void start();   // continues where stop() paused (or at a seek position)
    void stop();    // pauses; seek (0) rewinds

    // Jump to a position (quarters from the start of the current plan); the next hit there plays next.
    // If stopped, the next start() begins at this position.
    void seek (double quarters);

    // A loop range in the current plan (quarters, normally on bar lines): at its end playback jumps back to
    // its start, counted as a pass. Hits that belong to the range stay inside it (an early hit on its first
    // beat is played at the start, as at the plan start). end <= start: no range. The caller clears or
    // replaces it when another plan is selected.
    void setLoopRange (double startQuarters, double endQuarters) { rangeStart = startQuarters; rangeEnd = endQuarters; }
    void clearLoopRange()                                        { rangeStart = rangeEnd = -1.0; }

    bool isPlaying() const { return playing; }
    const PatternPlan* getCurrentPlan() const { return current; }

    // Delivers hits and beats that fall into this block
    void process (int numSamples, PlayEvent* events, int maxEvents, int& numEvents,
                  PulseEvent* pulses, int maxPulses, int& numPulses);

    // Superseded plans (one per call), nullptr if none is waiting
    const PatternPlan* popRetired();

    // Each setNextPlan() and each pass can supersede at most one plan. The caller
    // only hands in new plans while there is room here, so no pointer is lost.
    int freeRetireSlots() const { return (int) retired.size() - numRetired; }

private:
    void retire (const PatternPlan*);
    void applyPending();
    void collect (std::int64_t from, std::int64_t to, std::int64_t loopEnd, double endQ,
                  std::int64_t blockStart, double samplesPerQuarter, PlayEvent* events, int maxEvents, int& numEvents,
                  PulseEvent* pulses, int maxPulses, int& numPulses);
    std::int64_t sampleAt (double q, double samplesPerQuarter) const;
    double humanizedPos (const PlanEvent&) const;
    float humanizedVelocity (const PlanEvent&) const;

    double sampleRate = 48000.0;
    double tempo = 120.0;
    float humanizeScale = 1.0f;

    const PatternPlan* current = nullptr;
    const PatternPlan* pending = nullptr;
    bool seamless = false;   // pending replaces current in place (same id and length)
    double pendingStart = -1.0;   // seek() while a plan waits: where that plan starts (-1 = at 0)
    std::array<const PatternPlan*, 32> retired {};
    int numRetired = 0;

    // Transport: integer sample counter plus anchor. Position in the pattern (quarters) at sample s:
    //   q(s) = anchorQ + (s - anchorSample) / samplesPerQuarter
    // The anchor is only reset on tempo changes and at the pattern start -> no rounding drift,
    // every hit falls into exactly one block.
    bool playing = false;
    std::int64_t clock = 0;
    std::int64_t anchorSample = 0;
    double anchorQ = 0.0;
    double anchorSpq = 0.0;
    double loopSpq = 0.0;    // Tempo at the start of the pass: converts humanize ms to quarters.
                             // Fixed per pass -> positions don't change on tempo changes,
                             // every hit is triggered exactly once.
    int loop = 0;
    double startAt = 0.0;    // position for the next start() (set by seek while stopped)
    double rangeStart = -1.0, rangeEnd = -1.0;   // the loop range as requested (setLoopRange)
    double loopFrom = 0.0, loopTo = 0.0;         // ... clamped to the current plan; loopTo <= loopFrom: none
    bool rangePass = false;                      // this pass began with a jump back to the range start ...
    double rangePassFrom = 0.0;                  // ... here: hits written before it stay out of this pass
    void updateLoopRange();
    // The floor of the running pass: no hit sounds before floorSample, and hits whose (humanized) place lies
    // before floorQ are not played at all: they were handed out already (before a tempo change or a pause) or
    // lie before the start of the pass or of a seek. At such a start, early hits written at or after it are
    // played on it (floorKeepsWritten), as at the plan start; a first hit that rounds to half a sample before
    // it as well. So no hit is lost or doubled where the sample grid shifts.
    std::int64_t floorSample = 0;
    double floorQ = 0.0;
    bool floorKeepsWritten = true;
    double pauseFloor = 0.0;          // stop(): the floor to go on with (what had been handed out up to the pause)
    bool pauseKeepsWritten = false;
    bool resuming = false;            // the next start() continues the paused pass (no seek, no new plan in between):
                                      // its pass count, humanize tempo and range state stay, so nothing moves
};

// Deterministic random value in [-1, 1] from seed, event, pass and channel
float humanizeNoise (std::uint32_t seed, std::uint32_t eventId, int loop, int channel);

} // namespace hz
