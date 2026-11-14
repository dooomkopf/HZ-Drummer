#include "app/AudioExport.h"

#include "app/AudioEngine.h"
#include "app/Loudness.h"
#include "app/MidiExport.h"
#include "app/Mp3Encoder.h"
#include "app/Text.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

namespace
{
    constexpr int block = 1024;
    constexpr double settleSeconds = 0.3;    // stopped first, not written: the mixer's 20 ms smoothing settles
    constexpr double ringOutSeconds = 3.0;   // at most, after the music
    constexpr double fadeSeconds = 0.05;     // where the 3 s cut a sound that still rings
    constexpr double quietSeconds = 0.1;     // below quietLevel this long: the end
    constexpr float quietLevel = 1.0e-4f;    // -80 dBFS
    constexpr float peakDb = -1.0f;          // the loudest peak lifted to here (room for the MP3 encoder's overshoot)
    constexpr float maxGain = 16.0f;         // ... by at most +24 dB (an almost silent export is not blown up)
    constexpr int maxNameBytes = 200;        // file names: ext4 allows 255 bytes, minus "-v99.flac.part"
    constexpr double targetLufs = -14.0;     // B: the reference of Spotify and YouTube

    // B: the gain from a loudness to the target (an export without loud parts stays as it is)
    float loudnessGain (double lufs, float current)
    {
        if (lufs <= -69.0)
            return current;
        return juce::jmin (maxGain, current * juce::Decibels::decibelsToGain ((float) (targetLufs - lufs)));
    }
}

ExportJob::ExportJob (Request r)
    : juce::Thread ("HZ-Drummer Export"), request (std::move (r))
{
    if (! startThread())
    {
        result = "Der Export-Thread startet nicht.";
        done = true;
    }
}

ExportJob::~ExportJob()
{
    stopThread (-1);   // the loops ask threadShouldExit() every few milliseconds: never killed halfway
}

juce::String ExportJob::extension (Format f)
{
    switch (f)
    {
        case Format::Midi: return ".mid";
        case Format::Mp3:  return ".mp3";
        case Format::Wav:  return ".wav";
        case Format::Flac: return ".flac";
    }
    return ".wav";
}

juce::File ExportJob::uniqueFile (const juce::File& folder, const juce::String& name, Format f)
{
    auto stem = juce::File::createLegalFileName (name).trim();
    while (stem.isNotEmpty() && (int) stem.getNumBytesAsUTF8() > maxNameBytes)
        stem = stem.dropLastCharacters (1).trimEnd();
    if (stem.isEmpty())
        stem = "Export";
    auto file = folder.getChildFile (stem + extension (f));
    for (int v = 2; file.exists() || file.getSiblingFile (file.getFileName() + ".part").exists(); ++v)
        file = folder.getChildFile (stem + "-v" + juce::String (v) + extension (f));
    return file;
}

void ExportJob::run()
{
    const auto part = request.file.getSiblingFile (request.file.getFileName() + ".part");
    juce::String error;
    try
    {
        part.deleteFile();
        error = request.format == Format::Midi
                    ? writeMidiExport (request.timeline, request.tempo, request.title, part, [this] { return threadShouldExit(); })
                    : renderAudio (part);
        if (error.isEmpty() && threadShouldExit())
            error = "abgebrochen";
        if (error.isEmpty())
        {
            if (request.file.exists())   // turned up meanwhile: the next free version, nothing is overwritten
                request.file = uniqueFile (request.file.getParentDirectory(), request.title, request.format);
            if (! part.moveFileTo (request.file))
                error = "Datei nicht umbenannt: " + part.getFullPathName();
        }
    }
    catch (const std::exception& e)
    {
        error = "Fehler: " + juce::String (e.what());
    }
    catch (...)
    {
        error = "unbekannter Fehler";
    }
    if (error.isNotEmpty())
        part.deleteFile();
    result = error;
    fraction = 1.0f;
    done = true;
}

// A: the first pass measures the peak, the write lifts it to -1 dBFS. B: the first pass measures the loudness; the
// limiter takes a little of it off the peaks, so a second pass measures once more through it at the new gain and
// corrects. The engine plays the very same every time (fixed seeds).
juce::String ExportJob::renderAudio (const juce::File& part)
{
    const auto* kit = request.kit;
    if (kit == nullptr || request.timeline.plan == nullptr)
        return kit == nullptr ? "kein Kit geladen" : "nichts zu exportieren";
    const double rate = kit->sampleRate;
    const float ceiling = juce::Decibels::decibelsToGain (peakDb);
    const bool b = request.level == Level::Loudness;

    float peak = 0.0f, gain = 1.0f;
    LoudnessMeter raw (rate);
    const Sink measureRaw = [&raw] (float* l, float* r, int n) { raw.add (l, r, n); return true; };
    if (auto error = renderPass (b ? measureRaw : Sink {}, 1.0f, 0.0f, b ? 0.3f : 0.5f, peak); error.isNotEmpty())
        return error;
    if (b)
    {
        gain = loudnessGain (raw.integrated(), 1.0f);
        LoudnessMeter limited (rate);
        PeakLimiter limiter (rate, ceiling);
        const PeakLimiter::Emit measure = [&limited] (const float* l, const float* r, int n) { limited.add (l, r, n); return true; };
        if (auto error = renderPass ([&] (float* l, float* r, int n) { return limiter.process (l, r, n, measure); }, gain, 0.3f, 0.6f, peak);
            error.isNotEmpty())
            return error;
        limiter.flush (measure);
        gain = loudnessGain (limited.integrated(), gain);
    }
    else if (peak > 0.0f)
        gain = juce::jmin (maxGain, ceiling / peak);

    // the file: MP3 through LAME, WAV and FLAC with 24 bit
    Mp3Encoder mp3;
    juce::WavAudioFormat wav;
    juce::FlacAudioFormat flac;
    juce::AudioFormat& format = request.format == Format::Flac ? static_cast<juce::AudioFormat&> (flac) : wav;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::int64_t frames = 0;
    PeakLimiter::Emit write;
    if (request.format == Format::Mp3)
    {
        if (! mp3.open (part, juce::roundToInt (rate)))
            return mp3.error();
        write = [&mp3] (const float* l, const float* r, int n) { return mp3.write (l, r, n); };
    }
    else
    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (part);
        if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
            return "Datei nicht anlegbar: " + part.getFullPathName();
        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)
                                                             .withQualityOptionIndex (request.format == Format::Flac ? 5 : 0);
        writer = format.createWriterFor (stream, options);
        if (writer == nullptr)
            return "Schreiber nicht anlegbar (" + juce::String (rate) + " Hz)";
        write = [&writer, &frames] (const float* l, const float* r, int n)
        {
            const float* channels[] = { l, r };
            frames += n;
            return writer->writeFromFloatArrays (channels, 2, n);
        };
    }

    // the pass that writes, B through the limiter
    PeakLimiter limiter (rate, ceiling);
    const auto error = renderPass ([&] (float* l, float* r, int n) { return b ? limiter.process (l, r, n, write) : write (l, r, n); },
                                   gain, b ? 0.6f : 0.5f, 1.0f, peak);
    if (error.isNotEmpty())
        return mp3.error().isNotEmpty() ? mp3.error() : error;
    if (b && ! limiter.flush (write))
        return mp3.error().isNotEmpty() ? mp3.error() : "Schreiben fehlgeschlagen";
    if (request.format == Format::Mp3)
        return mp3.finish() ? juce::String() : mp3.error();

    writer = nullptr;   // writes the header with the final sizes
    // read back what is on the disk: a last write that failed (a full disk) shows as a wrong length
    std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (new juce::FileInputStream (part), true));
    if (reader == nullptr || reader->lengthInSamples != frames)
        return "Die Datei wurde nicht vollständig geschrieben (Datenträger voll?)."_u;
    return {};
}

juce::String ExportJob::renderPass (const Sink& sink, float gain, float progressFrom, float progressTo, float& peak)
{
    const double rate = request.kit->sampleRate;
    auto engine = std::make_unique<AudioEngine>();
    engine->prepare (rate, 0);
    engine->setKit (request.kit);
    engine->setMixState (request.mix);
    engine->sendPlan (std::make_unique<hz::PatternPlan> (*request.timeline.plan));
    AudioEngine::Control control;
    control.tempoTenths = (std::uint16_t) juce::jlimit (200, 4000, juce::roundToInt (request.tempo * 10.0));
    engine->setControl (control);   // stopped: it takes the plan, the gains settle
    juce::AudioBuffer<float> buffer (2, block);
    for (int i = 0, n = (int) std::ceil (settleSeconds * rate / block); i < n; ++i)
    {
        buffer.clear();
        engine->process (buffer.getWritePointer (0), buffer.getWritePointer (1), block);
    }
    control.play = true;
    control.generation = 1;
    engine->setControl (control);   // starts at sample 0 of the plan

    const double samplesPerQuarter = rate * 60.0 / (control.tempoTenths / 10.0);
    const auto musicEnd = (std::int64_t) std::llround (request.timeline.musicEnd * samplesPerQuarter);
    const auto hardEnd = musicEnd + (std::int64_t) (ringOutSeconds * rate);
    const auto fadeLength = (std::int64_t) (fadeSeconds * rate);
    const auto quietLength = (std::int64_t) (quietSeconds * rate);
    std::int64_t rendered = 0, quietSince = -1;
    peak = 0.0f;

    while (rendered < hardEnd)
    {
        if (threadShouldExit())
            return "abgebrochen";
        const int len = (int) std::min<std::int64_t> (block, rendered < musicEnd ? musicEnd - rendered : hardEnd - rendered);
        buffer.clear();
        float* left = buffer.getWritePointer (0);
        float* right = buffer.getWritePointer (1);
        engine->process (left, right, len);
        engine->collectRetiredPlans();

        const float level = juce::jmax (buffer.getMagnitude (0, 0, len), buffer.getMagnitude (1, 0, len));
        if (rendered >= musicEnd)   // ringing out: a fade before the hard end, the end once it is quiet
        {
            for (int i = 0; i < len; ++i)
                if (const auto toEnd = hardEnd - (rendered + i); toEnd < fadeLength)
                {
                    const auto fade = (float) toEnd / (float) fadeLength;
                    left[i] *= fade;
                    right[i] *= fade;
                }
            quietSince = level < quietLevel ? (quietSince < 0 ? rendered : quietSince) : -1;
        }
        peak = juce::jmax (peak, buffer.getMagnitude (0, 0, len), buffer.getMagnitude (1, 0, len));

        if (sink != nullptr)
        {
            if (! juce::approximatelyEqual (gain, 1.0f))
                buffer.applyGain (0, len, gain);
            if (! sink (left, right, len))
                return "Schreiben fehlgeschlagen";
        }
        rendered += len;
        fraction = juce::jmin (0.99f, progressFrom + (progressTo - progressFrom) * (float) rendered / (float) hardEnd);
        if (quietSince >= 0 && rendered - quietSince >= quietLength)
            break;
    }
    return {};
}
