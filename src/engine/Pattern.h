#pragma once

#include "engine/Meter.h"
#include "engine/Rational.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace hz
{

// Instrument and articulation kept separate (kit-independent). The kit maps both to its samples.
enum class Instrument : std::uint8_t
{
    Kick, Snare, HiHat, Ride, Crash1, Crash2, China, Splash, Tom1, Tom2, Tom3, Tom4, Cowbell,
    Count
};

enum class Articulation : std::uint8_t
{
    Normal,   // Kick, snare center, hi-hat closed, ride bow, cymbal, tom
    Open,     // Hi-hat open
    Half,     // Hi-hat half-open
    Pedal,    // Hi-hat with the foot
    Bell,     // Ride bell
    Rim,      // Snare rimshot
    Side,     // Snare sidestick
    Count
};

enum class Limb : std::uint8_t { Auto, Right, Left };

enum class LayerRole : std::uint8_t { Hands, Feet, Other };

struct Voice
{
    Instrument instrument = Instrument::Snare;
    Articulation articulation = Articulation::Normal;

    friend bool operator== (Voice a, Voice b) { return a.instrument == b.instrument && a.articulation == b.articulation; }
};

constexpr int numVoiceSlots = (int) Instrument::Count * (int) Articulation::Count;
inline int voiceSlot (Voice v) { return (int) v.instrument * (int) Articulation::Count + (int) v.articulation; }

// Short name for files/kit mapping: "hihat", "hihat.open", "ride.bell", "snare.rim"
std::string voiceKey (Voice);
bool parseVoiceKey (const std::string&, Voice&);

struct PatternEvent
{
    std::uint32_t id = 0;          // unique within a pattern revision
    Voice voice;
    Rational pos;                  // Quarters from pattern start
    float velocity = 0.75f;        // 0..1
    Limb limb = Limb::Auto;
    bool flam = false;             // Grace note is generated during planning
    float lagMs = 0.0f;            // ms later (+, "laid back") or earlier (-) than its position, -100..100; 0 = on the grid
};

struct PatternLayer
{
    std::uint32_t id = 0;
    LayerRole role = LayerRole::Hands;
    std::vector<PatternEvent> events;
};

struct PatternBar
{
    Meter meter = Meter::make (4, 4);
    Rational grid { 1, 16 };       // Grid step as note value (editing view only)

    Rational lengthInQuarters() const { return { (std::int64_t) meter.numerator * 4, meter.denominator }; }
};

struct Swing
{
    Rational subdivision { 1, 8 }; // Eighth- or sixteenth-note swing (note value)
    float ratio = 0.5f;            // 0.5 = straight, 0.667 = triplet feel, max. 0.75
};

struct PatternOrigin
{
    std::string parentId;
    int parentRevision = 0;
    std::vector<std::string> operators;
    std::uint32_t seed = 0;
};

struct Pattern
{
    static constexpr int formatVersion = 1;
    static constexpr int maxBars = 512;
    static constexpr size_t maxEvents = 8192;

    std::string id;
    std::string name;
    int revision = 1;
    std::vector<std::string> tags;
    double tempo = 120.0, tempoMin = 0.0, tempoMax = 0.0;
    std::vector<PatternBar> bars;
    std::vector<PatternLayer> layers;
    Swing swing;
    float humanize = 0.3f;         // 0..1
    std::uint32_t humanizeSeed = 1;
    PatternOrigin origin;

    Rational lengthInQuarters() const;
    Rational barStart (int bar) const;
    int barAt (Rational pos) const;
    std::uint32_t nextEventId() const;

    // Checks bounds (positions in the pattern, velocity, meters); returns error text or ""
    std::string validate() const;
};

// Immutable playback plan: built on the UI thread, only read on the audio thread
struct PlanEvent
{
    double pos = 0.0;              // Quarters from pattern start, after swing
    Voice voice;
    float velocity = 0.75f;
    Limb limb = Limb::Auto;
    std::uint32_t eventId = 0;     // for deterministic humanization
    float extraDelayMs = 0.0f;     // e.g. flam main hit after the grace note
    std::uint8_t part = 0;         // Sub-hit of the same event (flam: 0 grace note, 1 main hit)
    float humanize = 0.0f;         // humanize amount of the pattern this hit comes from (songs mix patterns)
};

struct PlanBar
{
    double start = 0.0;            // Quarters
    double length = 4.0;
    Meter meter;
    int meterTag = 0;              // assigned by the caller (e.g. index into the meter table)
    bool click = false;            // metronome click on the pulses of this bar (song "Klick" parts)
};

struct PatternPlan
{
    std::string patternId;
    double length = 4.0;           // Quarters
    std::vector<PlanBar> bars;
    std::vector<PlanEvent> events; // ascending by pos
    float humanize = 0.0f;
    std::uint32_t seed = 1;
    int tag = -1;                  // assigned by the caller (e.g. index in the library)
};

PatternPlan buildPlan (const Pattern&);

// How much later (in quarters) the pattern's swing plays an event at 'pos': only exact offbeats of the
// swing subdivision move (the "und" of eighths, "e" and "a" of sixteenths; triplets stay untouched).
double swingShift (const Pattern&, Rational pos);

} // namespace hz
