#pragma once

#include "app/ExportPlan.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>
#include <functional>

namespace hz { struct DrumKit; }

// One export of the export button, on its own thread: the timeline through a second AudioEngine (same kit, same mixer,
// same speaker protection, so the file sounds as the speakers do) into WAV or FLAC (24 bit) or MP3, or through the
// player into MIDI. Levels, chosen per format in the menu: A lifts the loudest peak to -1 dBFS; B brings the export to
// -14 LUFS like the streaming services, a limiter holding the peaks at -1 dBFS. After the music the hits ring out until
// it is quiet, at most 3 s (then a short fade). The kit must stay loaded meanwhile: MainComponent pauses the kit manager
// while a job runs. It writes "<file>.part" and renames it when done, so a failed or cancelled export leaves nothing
// behind; a file that turned up under the name meanwhile is not overwritten (the next free version is taken).
class ExportJob : private juce::Thread
{
public:
    enum class Format { Midi, Mp3, Wav, Flac };
    enum class Level { Peak, Loudness };   // A and B of the menu

    struct Request
    {
        Format format = Format::Wav;
        Level level = Level::Peak;
        ExportTimeline timeline;
        double tempo = 120.0;
        std::uint64_t mix = 0;            // mix::pack of the mixer
        const hz::DrumKit* kit = nullptr;
        juce::String title;
        juce::File file;
    };

    explicit ExportJob (Request);
    ~ExportJob() override;               // cancels a running export and waits for it

    bool finished() const                { return done.load(); }
    float progress() const               { return fraction.load(); }   // 0..1
    const juce::String& error() const    { return result; }            // once finished: "" or what went wrong
    const juce::File& file() const       { return request.file; }     // once finished: where it went

    static juce::String extension (Format);
    // folder/name.ext, or name-v2.ext, name-v3.ext … if that is taken: nothing is overwritten
    static juce::File uniqueFile (const juce::File& folder, const juce::String& name, Format);

private:
    using Sink = std::function<bool (float* left, float* right, int numSamples)>;
    void run() override;
    juce::String renderAudio (const juce::File& part);
    // one pass through a fresh engine (the same samples every time: fixed seeds); sink gets every block, times gain;
    // peak: the loudest sample before the gain
    juce::String renderPass (const Sink&, float gain, float progressFrom, float progressTo, float& peak);

    Request request;
    juce::String result;
    std::atomic<bool> done { false };
    std::atomic<float> fraction { 0.0f };
};
