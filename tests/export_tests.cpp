// Tests for the export button: the timeline (count-in, repeats, loop range, silent bars), the MIDI file (notes, tempo,
// time signatures, ticks) and the sound through a small made-up kit into WAV, FLAC and, where libmp3lame is there, MP3.
#include "app/AudioExport.h"
#include "app/ExportPlan.h"
#include "app/Loudness.h"
#include "app/MidiExport.h"
#include "app/MixState.h"
#include "app/Mp3Encoder.h"
#include "app/Text.h"
#include "engine/SamplePlayer.h"
#include "engine/TabParser.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <thread>

extern int exportTestFailures;
int exportTestFailures = 0;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++exportTestFailures; } } while (0)

namespace
{
    // 2 bars 4/4: kick on 1 and 3, snare on 2 and 4, no humanizing (positions exact)
    hz::PatternPlan twoBars()
    {
        auto p = hz::parseTabs ("name: Export\nid: user.export\ngrid: 1/4\n"
                                "bd | x . x . | x . x . |\nsn | . x . x | . x . x |\n").patterns[0];
        p.humanize = 0.0f;
        return hz::buildPlan (p);
    }

    juce::File sandbox()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hz-export-test");
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }

    // a kit of two instruments, each one decaying tone of 0.2 s
    hz::DrumKit smallKit()
    {
        hz::DrumKit kit;
        kit.name = "Test";
        kit.sampleRate = 48000.0;
        for (const char* name : { "kick", "snare" })
        {
            hz::DrumInstrument inst;
            inst.name = name;
            hz::DrumSample s;
            for (int i = 0; i < 9600; ++i)
            {
                const auto v = 0.5f * std::sin (0.05f * (float) i) * std::exp (-(float) i / 2000.0f);
                s.left.push_back (v);
                s.right.push_back (v);
            }
            s.power = 0.5f;
            inst.samples.push_back (s);
            kit.instruments.push_back (inst);
        }
        kit.finalise();
        kit.resolveVoices ({ { "kick", "kick" }, { "snare", "snare" } });
        return kit;
    }

    void testTimeline()
    {
        const auto source = twoBars();
        ExportSpan span;
        span.repeats = 3;
        span.countIn = true;
        auto t = makeExportTimeline (source, span, 120.0, 4.0);
        CHECK (t.plan != nullptr);
        if (t.plan == nullptr)
            return;
        CHECK (std::abs (t.musicStart - 4.0) < 1e-9);              // one bar of 4/4 counts in
        CHECK (std::abs (t.musicEnd - 4.0 - 3 * 8.0) < 1e-9);       // then 3 passes of 8 quarters
        CHECK (t.plan->events.size() == 3 * source.events.size());
        CHECK (t.plan->bars.front().click && ! t.plan->bars[1].click);
        CHECK (t.plan->length >= t.musicEnd + 8.0 - 1e-9);          // 4 s at 120 BPM: 8 quarters of silent bars
        CHECK (t.plan->events.front().pos >= t.musicStart - 1e-9);
        CHECK (t.plan->events.back().pos < t.musicEnd);
        CHECK (t.plan->events[0].eventId != t.plan->events[source.events.size()].eventId);   // passes humanized apart

        // a range: the second bar only, twice, with a click on every bar of the music
        span = {};
        span.from = 4.0;
        span.to = 8.0;
        span.repeats = 2;
        span.click = true;
        t = makeExportTimeline (source, span, 120.0, 0.0);
        CHECK (t.plan != nullptr && std::abs (t.musicEnd - 8.0) < 1e-9);
        CHECK (t.plan != nullptr && t.plan->events.size() == 2 * 4 && t.plan->bars.size() == 2 && t.plan->bars[1].click);

        span.from = 20.0;   // behind the end: nothing
        span.to = 24.0;
        CHECK (makeExportTimeline (source, span, 120.0, 4.0).plan == nullptr);
    }

    void testMidi (const juce::File& dir)
    {
        ExportSpan span;
        span.repeats = 2;
        span.countIn = true;
        const auto t = makeExportTimeline (twoBars(), span, 100.0, 4.0);
        const auto file = dir.getChildFile ("test.mid");
        CHECK (writeMidiExport (t, 100.0, "Test", file).isEmpty());

        juce::MidiFile midi;
        juce::FileInputStream in (file);
        CHECK (in.openedOk() && midi.readFrom (in));
        CHECK (midi.getTimeFormat() == 1920 && midi.getNumTracks() == 2);
        if (midi.getNumTracks() != 2)
            return;
        juce::MidiMessageSequence tempos, signatures;
        midi.findAllTempoEvents (tempos);
        midi.findAllTimeSigEvents (signatures);
        CHECK (tempos.getNumEvents() == 1 && std::abs (tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote() - 0.6) < 1e-6);
        CHECK (signatures.getNumEvents() == 1);   // 4/4 throughout: once

        int kicks = 0, snares = 0, clicks = 0;
        double firstKick = -1.0;
        const auto* drums = midi.getTrack (1);
        for (int i = 0; i < drums->getNumEvents(); ++i)
        {
            const auto& m = drums->getEventPointer (i)->message;
            if (! m.isNoteOn())
                continue;
            CHECK (m.getChannel() == 10);
            kicks += m.getNoteNumber() == 36 ? 1 : 0;
            snares += m.getNoteNumber() == 38 ? 1 : 0;
            clicks += (m.getNoteNumber() == 33 || m.getNoteNumber() == 34) ? 1 : 0;
            if (m.getNoteNumber() == 36 && firstKick < 0.0)
                firstKick = m.getTimeStamp();
        }
        CHECK (kicks == 8 && snares == 8 && clicks == 4);    // 2 passes x 2 bars; four count-in clicks
        CHECK (std::abs (firstKick - 4 * 1920) <= 1.0);      // after the count-in bar, on the tick
        CHECK (std::abs (midi.getLastTimestamp() - 20 * 1920) <= 1.0);   // ends with the music: 1 + 2 x 2 bars

        // a last hit laid back past the end of the music (320 BPM: the bar lasts 750 ms, the hit sounds at 803 ms)
        hz::PatternPlan late;
        late.length = 4.0;
        late.bars.push_back ({ 0.0, 4.0, hz::Meter::make (4, 4), 0, false });
        hz::PlanEvent hit;
        hit.pos = 3.75;
        hit.voice = { hz::Instrument::Snare, hz::Articulation::Normal };
        hit.extraDelayMs = 100.0f;
        late.events.push_back (hit);
        const auto lateFile = dir.getChildFile ("late.mid");
        CHECK (writeMidiExport (makeExportTimeline (late, {}, 320.0, 4.0), 320.0, "Spät"_u, lateFile).isEmpty());
        juce::MidiFile lateMidi;
        juce::FileInputStream lateIn (lateFile);
        CHECK (lateIn.openedOk() && lateMidi.readFrom (lateIn) && lateMidi.getNumTracks() == 2);
        int lateNotes = 0;
        if (lateMidi.getNumTracks() == 2)
            for (int i = 0; i < lateMidi.getTrack (1)->getNumEvents(); ++i)
                lateNotes += lateMidi.getTrack (1)->getEventPointer (i)->message.isNoteOn() ? 1 : 0;
        CHECK (lateNotes == 1);
    }

    void testLoudness()
    {
        // EBU Tech 3341, case 1: a 1 kHz sine at -23 dBFS on both channels measures -23 LUFS
        constexpr double rate = 48000.0;
        std::vector<float> sine ((size_t) (20 * rate));
        for (size_t i = 0; i < sine.size(); ++i)
            sine[i] = 0.0707946f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / rate);
        LoudnessMeter meter (rate);
        meter.add (sine.data(), sine.data(), (int) sine.size());
        CHECK (std::abs (meter.integrated() + 23.0) < 0.1);
        LoudnessMeter quiet (rate);
        std::vector<float> zeros (48000, 0.0f);
        quiet.add (zeros.data(), zeros.data(), (int) zeros.size());
        CHECK (quiet.integrated() <= -70.0);

        // the limiter: peaks of 1.0 held at the ceiling, quiet parts untouched, nothing longer or shorter
        std::vector<float> in (9600, 0.1f);
        in[4000] = 1.0f;
        in[6000] = -1.0f;
        PeakLimiter limiter (rate, 0.5f);
        std::vector<float> out;
        const PeakLimiter::Emit keep = [&out] (const float* l, const float*, int n) { out.insert (out.end(), l, l + n); return true; };
        CHECK (limiter.process (in.data(), in.data(), 5000, keep) && limiter.process (in.data() + 5000, in.data() + 5000, 4600, keep));
        CHECK (limiter.flush (keep));
        CHECK (out.size() == in.size());
        float highest = 0.0f;
        for (const float v : out)
            highest = std::max (highest, std::abs (v));
        CHECK (highest <= 0.5f + 1.0e-6f);
        CHECK (out.size() == in.size() && std::abs (out[100] - 0.1f) < 1.0e-6f);   // in time, unchanged before the peak
        CHECK (out.size() == in.size() && std::abs (out[4000]) > 0.45f);          // the peak itself stays (at the ceiling)
    }

    bool waitFor (ExportJob& job)
    {
        for (int i = 0; i < 3000 && ! job.finished(); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        return job.finished();
    }

    void testAudio (const juce::File& dir)
    {
        const auto kit = smallKit();
        mix::Settings mixer {};
        for (int ch = 0; ch < mix::NumChannels; ++ch)
            mixer[(size_t) ch] = { mix::defaultDb (ch), false, false };

        for (const auto format : { ExportJob::Format::Wav, ExportJob::Format::Flac, ExportJob::Format::Mp3 })
        {
            if (format == ExportJob::Format::Mp3 && ! Mp3Encoder::available())
            {
                std::printf ("  (MP3 nicht getestet: libmp3lame fehlt)\n");
                continue;
            }
            ExportJob::Request r;
            r.format = format;
            r.tempo = 120.0;
            r.timeline = makeExportTimeline (twoBars(), {}, 120.0, 4.0);
            r.mix = mix::pack (mixer);
            r.kit = &kit;
            r.title = "Test";
            r.file = ExportJob::uniqueFile (dir, "Test", format);
            ExportJob job (std::move (r));
            CHECK (waitFor (job));
            CHECK (job.error().isEmpty());
            CHECK (job.file().existsAsFile() && ! job.file().getSiblingFile (job.file().getFileName() + ".part").exists());
            if (format == ExportJob::Format::Mp3)
            {
                juce::MemoryBlock data;
                CHECK (job.file().loadFileAsData (data) && data.getSize() > 4000);
                const auto* bytes = static_cast<const unsigned char*> (data.getData());
                CHECK (data.getSize() > 2 && bytes[0] == 0xff && (bytes[1] & 0xe0) == 0xe0);   // starts with a frame (VBR header)
                continue;
            }
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.file()));
            CHECK (reader != nullptr);
            if (reader == nullptr)
                continue;
            CHECK (reader->sampleRate == 48000.0 && reader->numChannels == 2 && reader->bitsPerSample == 24);
            // 4 s of music (2 bars at 120 BPM), then the last snare rings out 0.2 s until quiet: well below the 3 s
            const double seconds = (double) reader->lengthInSamples / reader->sampleRate;
            CHECK (seconds > 4.05 && seconds < 4.6);
            juce::AudioBuffer<float> audio (2, (int) reader->lengthInSamples);
            reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);
            CHECK (audio.getMagnitude (0, 0, 4800) > 0.01f);   // the first kick at the very start
            const int n = audio.getNumSamples();
            const float top = juce::jmax (audio.getMagnitude (0, 0, n), audio.getMagnitude (1, 0, n));
            CHECK (std::abs (top - juce::Decibels::decibelsToGain (-1.0f)) < 1.0e-3f);   // A: the loudest peak at -1 dBFS
        }

        // B: -14 LUFS, the limiter holding the peaks at -1 dBFS
        {
            ExportJob::Request r;
            r.format = ExportJob::Format::Wav;
            r.level = ExportJob::Level::Loudness;
            r.tempo = 120.0;
            r.timeline = makeExportTimeline (twoBars(), {}, 120.0, 4.0);
            r.mix = mix::pack (mixer);
            r.kit = &kit;
            r.title = "Test B";
            r.file = ExportJob::uniqueFile (dir, "Test B", r.format);
            ExportJob job (std::move (r));
            CHECK (waitFor (job) && job.error().isEmpty());
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.file()));
            CHECK (reader != nullptr);
            if (reader != nullptr)
            {
                const int n = (int) reader->lengthInSamples;
                juce::AudioBuffer<float> audio (2, n);
                reader->read (&audio, 0, n, 0, true, true);
                LoudnessMeter meter (reader->sampleRate);
                meter.add (audio.getReadPointer (0), audio.getReadPointer (1), n);
                CHECK (std::abs (meter.integrated() + 14.0) < 1.0);
                CHECK (juce::jmax (audio.getMagnitude (0, 0, n), audio.getMagnitude (1, 0, n)) <= juce::Decibels::decibelsToGain (-1.0f) + 1.0e-4f);
            }
        }

        // the same name again: a second version, nothing overwritten
        const auto first = ExportJob::uniqueFile (dir, "Test", ExportJob::Format::Wav);
        CHECK (first.getFileName() == "Test-v2.wav");
        CHECK (ExportJob::uniqueFile (dir, "AC/DC: Back", ExportJob::Format::Midi).getFileName() == "ACDC Back.mid");
        const auto umlauts = ExportJob::uniqueFile (dir, juce::String::repeatedString ("ä"_u, 150), ExportJob::Format::Flac);
        CHECK ((int) umlauts.getFileName().getNumBytesAsUTF8() <= 200 + 5);   // fits ext4 with "-v99" and ".part"
    }
}

void runExportTests()
{
    const auto dir = sandbox();
    testTimeline();
    testMidi (dir);
    testLoudness();
    testAudio (dir);
    if (juce::SystemStats::getEnvironmentVariable ("HZ_EXPORT_KEEP", {}).isNotEmpty())   // to listen or check with ffprobe
        std::printf ("Export-Testdateien: %s\n", dir.getFullPathName().toRawUTF8());
    else
        dir.deleteRecursively();
}
