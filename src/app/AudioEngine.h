#pragma once

#include "app/MixState.h"
#include "engine/ClickSynth.h"
#include "engine/PatternPlayer.h"
#include "engine/OutputGuard.h"
#include "engine/SamplePlayer.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Real-time side of the app: plays PatternPlans through the sampler (and the click synth).
//
// Threading contract
//  - Methods marked [UI] must only be called on the message thread, [Audio] only from the audio callback.
//  - UI -> audio: one atomic control snapshot, one atomic mixer snapshot, one atomic kit pointer,
//    and a bounded plan queue. Audio -> UI: a display-event FIFO, a retire FIFO for plans and one
//    atomic level per mixer channel.
//  - Plans are owned by the engine's UI side. The audio thread only borrows them and hands
//    replaced plans back through the retire FIFO; only then are they freed (on the UI thread).
//  - Kits are owned by KitManager; kitInUse() tells it when the audio thread has let go of one.
//  - The audio path never allocates, locks or calls into the UI.
class AudioEngine
{
public:
    // Complete transport state, published as one 64-bit word -> always consistent.
    // 'generation' increments on every start/stop so that a quick stop->start is never lost.
    struct Control
    {
        std::uint16_t tempoTenths = 1200;   // BPM * 10
        bool click = false;                 // metronome click on the pulses
        bool play = false;
        std::uint16_t generation = 0;

        std::uint64_t pack() const;
        static Control unpack (std::uint64_t);
    };

    // A pulse (beat) as it becomes audible; used for the displays
    struct DisplayEvent
    {
        double timeMs = 0.0;   // wall-clock time at which the pulse is heard
        int bar = 0, pulse = 0, loop = 0, meter = 0, numBars = 1, planTag = -1;
        std::uint16_t generation = 0;
    };

    // [UI]
    void setControl (const Control& c)            { control = c.pack(); }
    void setMixState (std::uint64_t packed)       { mixState = packed; }
    void setKit (const hz::DrumKit* kit)          { kitForAudio = kit; }
    const hz::DrumKit* kitInUse() const           { return kitInUseByAudio.load(); }
    double deviceSampleRate() const               { return deviceRate.load(); }
    void seek (double quarters)                   { seekTarget = quarters; ++seekSerial; }   // in the playing plan
    // the loop range in the playing plan (quarters, on bar lines); both ends travel in one atomic word
    void setLoopRange (double startQuarters, double endQuarters) { loopRange = packRange ((float) startQuarters, (float) endQuarters); }
    void clearLoopRange()                         { loopRange = packRange (-1.0f, -1.0f); }
    // one hit right now, outside any pattern (a click on a lane name in the raster); the newest request wins
    // (message thread only)
    void previewVoice (hz::Voice v, float velocity)
    {
        const std::uint32_t hit = (std::uint32_t) v.instrument | ((std::uint32_t) v.articulation << 8)
                                | ((std::uint32_t) juce::roundToInt (juce::jlimit (0.0f, 1.0f, velocity) * 255.0f) << 16);
        // request number and hit in one atomic: the audio thread never pairs a number with another request's hit
        previewRequest = ((std::uint64_t) ++previewCount << 32) | hit;
    }
    void sendPlan (std::unique_ptr<hz::PatternPlan>);
    void collectRetiredPlans();                   // frees plans the audio thread has released
    void popDisplayEvents (std::vector<DisplayEvent>& out);
    float takeLevel (int channel) { return levels[(size_t) channel].exchange (0.0f); }   // peak since the last call, linear

    // [Audio]
    void prepare (double sampleRate, int outputLatencySamples);
    void process (float* left, float* right, int numSamples);

private:
    void triggerVoice (const hz::PlayEvent&, std::int64_t blockStart);
    void returnRetiredPlans();

    // UI -> audio
    std::atomic<std::uint64_t> control { 0 };
    std::atomic<std::uint64_t> mixState { 0 };
    std::atomic<const hz::DrumKit*> kitForAudio { nullptr };
    std::atomic<const hz::DrumKit*> kitInUseByAudio { nullptr };
    std::atomic<double> deviceRate { 0.0 };
    std::atomic<double> seekTarget { 0.0 };
    static std::uint64_t packRange (float start, float end)
    {
        std::uint32_t a, b;
        std::memcpy (&a, &start, 4);
        std::memcpy (&b, &end, 4);
        return ((std::uint64_t) a << 32) | b;
    }
    std::atomic<std::uint64_t> loopRange { packRange (-1.0f, -1.0f) };
    std::uint64_t appliedLoopRange = packRange (-1.0f, -1.0f);   // [Audio]
    std::atomic<std::uint32_t> seekSerial { 0 };   // incremented per request; the audio thread compares
    std::atomic<std::uint64_t> previewRequest { 0 };   // request number << 32 | voice and velocity of previewVoice
    std::uint32_t previewCount = 0;                     // message thread
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free);
    static_assert (std::atomic<const hz::DrumKit*>::is_always_lock_free);

    juce::AbstractFifo planFifo { 16 };
    std::array<const hz::PatternPlan*, 16> planQueue {};
    std::vector<std::unique_ptr<hz::PatternPlan>> ownedPlans;   // UI side owns every plan
    std::unique_ptr<hz::PatternPlan> unsentPlan;                // newest plan if the queue was full

    // audio -> UI
    juce::AbstractFifo displayFifo { 256 };
    std::array<DisplayEvent, 256> displayBuffer {};
    juce::AbstractFifo retireFifo { 64 };
    std::array<const hz::PatternPlan*, 64> retireQueue {};
    std::array<std::atomic<float>, mix::NumChannels> levels {};
    static_assert (std::atomic<float>::is_always_lock_free);

    // audio thread only
    hz::PatternPlayer player;
    hz::ClickSynth click;
    hz::SamplePlayer sampler;
    hz::OutputGuard guard;
    float masterNow = 1.0f, clickNow = 1.0f;   // smoothed mixer gains
    static constexpr int maxHitsPerChunk = 24;   // see process(): burst guard
    std::array<hz::PlayEvent, 256> playEvents {};
    std::array<hz::PulseEvent, 128> pulseEvents {};
    std::array<hz::ClickEvent, 128> clickEvents {};
    std::array<const hz::PatternPlan*, 16> retireBacklog {};
    int retireBacklogSize = 0;
    double sampleRate = 48000.0;
    int latencySamples = 0;
    std::uint16_t appliedGeneration = 0;
    std::uint32_t appliedSeek = 0;
    std::uint32_t appliedPreview = 0;
    std::int64_t audioClock = 0;
    std::int64_t lastKickSample = -1000000;
    bool lastKickLeft = true;
};
