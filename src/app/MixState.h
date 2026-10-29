#pragma once

#include "engine/Pattern.h"

#include <array>
#include <cstdint>

// Mixer state shared between the mixer UI and the audio engine.
// The audio thread receives one consistent 64-bit snapshot per block:
// 8 channels x 1 byte, 0 = silent, otherwise half-dB steps starting at -48 dB.
namespace mix
{

enum Channel { Kick, Snare, HiHat, Toms, Cymbals, Ride, Click, Master, NumChannels };

struct Setting
{
    float db = 0.0f;
    bool muted = false;
    bool solo = false;   // not persisted; lasts for the session only
};

using Settings = std::array<Setting, NumChannels>;

// Mixer channel a drum voice is routed to
int channelFor (hz::Voice);

// Channels a plan makes sound: its drum voices, the click of "Klick" bars, and always the total
std::array<bool, NumChannels> channelsUsedBy (const hz::PatternPlan&);

// Applies mute/solo rules and encodes all channels into one word
std::uint64_t pack (const Settings&);

// Linear gain of one channel from a packed snapshot
float gainOf (std::uint64_t packed, int channel);

const char* channelName (int channel);   // UI label (German)
const char* channelKey (int channel);    // key in the settings file

} // namespace mix
