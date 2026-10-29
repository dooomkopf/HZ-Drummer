#pragma once

#include "engine/Pattern.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hz
{

// A recorded hit (stereo, pre-mixed) with its measured loudness
struct DrumSample
{
    std::vector<float> left, right;
    float power = 0.0f;   // Loudness from the recording (DrumGizmo "power", energy measure)
};

struct DrumInstrument
{
    std::string name;
    std::vector<DrumSample> samples;   // after finalise(): ascending by power
    float minPower = 0.0f, maxPower = 0.0f;
};

// Fully loaded kit; immutable after finalise() and thus readable from the audio thread
struct DrumKit
{
    std::string name;
    double sampleRate = 48000.0;
    std::vector<DrumInstrument> instruments;

    // Pattern voice -> instrument index (-1 = missing). Filled by resolveVoices().
    std::array<int, numVoiceSlots> voiceMap {};
    int kickLeft = -1;   // own sample for the left foot, else -1

    void finalise();
    int find (const std::string& instrumentName) const;

    // Mapping from kit.json ("hihat.open" -> "hihat_open") plus central fallback rules.
    // Returns the voices that have neither a sample nor a fallback.
    std::vector<std::string> resolveVoices (const std::vector<std::pair<std::string, std::string>>& mapping);

    // Instrument for a hit; -1 if the kit doesn't have the voice
    int instrumentFor (Voice v, bool leftFoot) const;
};

// Velocity -> target loudness -> matching recording + fine correction.
// Separate from the player so it is testable.
struct VelocityPick
{
    int sample = -1;
    float gain = 1.0f;
};

// lastPicked: index played last (avoided), random: any random value
VelocityPick pickForVelocity (const DrumInstrument&, float velocity, int lastPicked, std::uint32_t random);

// Plays multi-velocity samples with round-robin.
// All methods run on the audio thread: no allocations, no locks.
// The kit belongs to the caller and must stay alive while it is set.
class SamplePlayer
{
public:
    SamplePlayer() { channelTarget.fill (1.0f); channelPrevious.fill (1.0f); }

    static constexpr int maxInstruments = 64;
    static constexpr int numVoices = 48;
    static constexpr int numFaders = 16;

    void prepare (double sampleRate);
    void setKit (const DrumKit* newKit);
    const DrumKit* getKit() const { return kit; }

    // velocity 0..1; sampleOffset within the next render() block
    // channel: mixer channel of this hit (0..maxChannels-1); its gain is applied while the voice sounds,
    // so mute/solo/volume changes also affect ringing cymbals.
    void trigger (int instrument, float velocity, int sampleOffset, int channel = 0);

    // Target gain per mixer channel; render() ramps from the previous value across the block (no clicks)
    static constexpr int maxChannels = 8;
    void setChannelGain (int channel, float gain);

    // Adds into the buffers; right may be nullptr (mono)
    void render (float* left, float* right, int numSamples);

    void setGain (float g) { gain = g; }
    int getNumActiveVoices() const;

private:
    struct Voice
    {
        const DrumSample* sample = nullptr;
        int position = 0;
        int startOffset = 0;   // Start within the current block
        float gain = 1.0f;
        int fadeStart = 0;     // faders only: start of the fade-out in the current block
        float fadeLevel = 1.0f;
        int channel = 0;       // mixer channel
    };

    void renderVoice (Voice&, float* left, float* right, int numSamples, bool fading);
    std::uint32_t nextRandom();

    const DrumKit* kit = nullptr;
    std::array<int, maxInstruments> lastPicked {};
    std::array<Voice, numVoices> voices {};
    std::array<Voice, numFaders> faders {};   // stolen voices fade out briefly here
    int nextFader = 0;
    std::uint64_t triggerCount = 0;
    std::array<std::uint64_t, numVoices> voiceAge {};
    std::uint32_t rng = 0x9e3779b9u;
    float gain = 0.8f;
    float fadeStep = 1.0f / 144.0f;   // approx. 3 ms at 48 kHz
    std::array<float, maxChannels> channelTarget {}, channelPrevious {};
};

} // namespace hz
