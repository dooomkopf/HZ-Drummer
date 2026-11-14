#include "app/MidiExport.h"

#include "engine/PatternPlayer.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr int ticksPerQuarter = 1920;
    constexpr double clockRate = 48000.0;   // only the player's clock: positions are converted back to quarters
    constexpr int drumChannel = 10;
    constexpr int maxLength = ticksPerQuarter / 8;   // a thirty-second note: drum notes only need a short gate
    constexpr double lateSeconds = 0.5;               // hits after the end of the music: laid back (100 ms), humanized, flams

    struct Note
    {
        std::int64_t tick = 0;
        int note = 38, velocity = 96;
    };

    int midiVelocity (float v)   { return std::clamp ((int) std::lround (v * 127.0f), 1, 127); }
    bool powerOfTwo (int n)      { return n > 0 && (n & (n - 1)) == 0; }
}

int gmNoteFor (hz::Voice v)
{
    using I = hz::Instrument;
    using A = hz::Articulation;
    switch (v.instrument)
    {
        case I::Kick:    return 36;
        case I::Snare:   return v.articulation == A::Rim ? 40 : v.articulation == A::Side ? 37 : 38;
        case I::HiHat:   return v.articulation == A::Pedal ? 44 : (v.articulation == A::Open || v.articulation == A::Half) ? 46 : 42;
        case I::Ride:    return v.articulation == A::Bell ? 53 : 51;
        case I::Crash1:  return 49;
        case I::Crash2:  return 57;
        case I::China:   return 52;
        case I::Splash:  return 55;
        case I::Tom1:    return 48;   // as the MIDI import reads them back (tools/import/midi)
        case I::Tom2:    return 45;
        case I::Tom3:    return 43;
        case I::Tom4:    return 41;
        case I::Cowbell: return 56;
        case I::Count:   break;
    }
    return 38;
}

juce::String writeMidiExport (const ExportTimeline& t, double tempo, const juce::String& title, const juce::File& file,
                              const std::function<bool()>& shouldStop)
{
    if (t.plan == nullptr)
        return "nichts zu exportieren";
    const auto& plan = *t.plan;

    hz::PatternPlayer player;
    player.prepare (clockRate);
    player.setTempo (tempo);
    player.setNextPlan (&plan);
    player.start();

    const double samplesPerQuarter = clockRate * 60.0 / tempo;
    const auto musicEnd = (std::int64_t) std::llround (t.musicEnd * samplesPerQuarter);
    const auto end = std::min ((std::int64_t) std::llround (plan.length * samplesPerQuarter),   // the silent bars hold them
                               musicEnd + (std::int64_t) (lateSeconds * clockRate));
    std::array<hz::PlayEvent, 256> hits {};
    std::array<hz::PulseEvent, 128> pulses {};
    std::vector<Note> notes;
    juce::MidiMessageSequence conductor, drums;
    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, title), 0.0);
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60.0e6 / tempo)), 0.0);
    drums.addEvent (juce::MidiMessage::textMetaEvent (3, "Drums"), 0.0);
    int numerator = 0, denominator = 0;

    for (std::int64_t done = 0; done < end;)
    {
        if (shouldStop && shouldStop())
            return "abgebrochen";
        const int len = (int) std::min<std::int64_t> (256, end - done);
        int numHits = 0, numPulses = 0;
        player.process (len, hits.data(), (int) hits.size(), numHits, pulses.data(), (int) pulses.size(), numPulses);
        const auto tickOf = [&] (int offset)
        {
            return std::max<std::int64_t> (0, std::llround ((double) (done + offset) / samplesPerQuarter * ticksPerQuarter));
        };
        for (int i = 0; i < numHits; ++i)
            notes.push_back ({ tickOf (hits[(size_t) i].sampleOffset), gmNoteFor (hits[(size_t) i].voice),
                               midiVelocity (hits[(size_t) i].velocity) });
        for (int i = 0; i < numPulses; ++i)
        {
            const auto& p = pulses[(size_t) i];
            if (done + p.sampleOffset >= musicEnd)
                continue;   // the silent bars: no meter, no click
            const auto tick = tickOf (p.sampleOffset);
            if (p.pulse == 0 && p.bar >= 0 && p.bar < (int) plan.bars.size())
            {
                const auto& m = plan.bars[(size_t) p.bar].meter;
                if ((m.numerator != numerator || m.denominator != denominator) && powerOfTwo (m.denominator))
                    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (m.numerator, m.denominator), (double) tick);
                numerator = m.numerator;
                denominator = m.denominator;
            }
            if (p.click)
                notes.push_back ({ tick, p.accent == hz::Meter::Accent::Downbeat ? 34 : 33,
                                   p.accent == hz::Meter::Accent::Weak ? 80 : 110 });
        }
        done += len;
    }

    // each note ends a thirty-second later, or just before the next one on the same key (fast rolls)
    std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.tick < b.tick; });
    std::array<std::int64_t, 128> nextOn;
    nextOn.fill (-1);
    std::vector<std::int64_t> lengths (notes.size(), maxLength);
    for (size_t i = notes.size(); i-- > 0;)
    {
        auto& next = nextOn[(size_t) std::clamp (notes[i].note, 0, 127)];
        if (next >= 0)
            lengths[i] = std::clamp<std::int64_t> (next - notes[i].tick - 1, 1, maxLength);
        next = notes[i].tick;
    }
    auto endTick = (std::int64_t) std::llround (t.musicEnd * ticksPerQuarter);   // a rest at the end stays
    for (size_t i = 0; i < notes.size(); ++i)
    {
        drums.addEvent (juce::MidiMessage::noteOn (drumChannel, notes[i].note, (juce::uint8) notes[i].velocity), (double) notes[i].tick);
        drums.addEvent (juce::MidiMessage::noteOff (drumChannel, notes[i].note), (double) (notes[i].tick + lengths[i]));
        endTick = std::max (endTick, notes[i].tick + lengths[i]);
    }
    drums.updateMatchedPairs();
    conductor.addEvent (juce::MidiMessage::endOfTrack(), (double) endTick);   // last: nothing may follow it
    drums.addEvent (juce::MidiMessage::endOfTrack(), (double) endTick);

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerQuarter);
    midi.addTrack (conductor);
    midi.addTrack (drums);
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return "Datei nicht anlegbar: " + file.getFullPathName();
    if (! midi.writeTo (out, 1))
        return "MIDI-Datei nicht geschrieben";
    out.flush();
    return out.getStatus().wasOk() ? juce::String() : out.getStatus().getErrorMessage();
}
