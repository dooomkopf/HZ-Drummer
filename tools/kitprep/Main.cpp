// kitprep: mixes a DrumGizmo kit (multi-mic) into stereo samples + kit.json for HZ-Drummer.
//
// Usage: kitprep <drumkit.xml> <targetfolder> [kitname] [samplerate, default 48000]
//
// Mixing rules:
//  - Trigger channels (Alesis, snare trigger) are discarded
//  - Overheads and room mics: hard left/right
//  - Close mics of the struck instrument: full, close mics of other drums: -14 dB (like a gate)
//  - Tail is cut off at -80 dB and faded out briefly
//  - Snare-bottom polarity: correlation is reported; flipping only with KITPREP_FLIP_SNARE_BOTTOM=1
// Instruments are processed in parallel (one worker per core).

#include "engine/Resampler.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <cmath>
#include <map>
#include <thread>

namespace
{

struct MicRule
{
    float gainDb = 0.0f;
    float pan = 0.0f;   // -1 left .. +1 right (audience perspective)
    bool closeMic = false;
};

// Basic mic settings by DrumGizmo channel name
const std::map<juce::String, MicRule> micRules {
    { "OH L",         { -2.0f, -1.0f, false } },
    { "OH R",         { -2.0f,  1.0f, false } },
    { "Amb L",        { -5.0f, -1.0f, false } },
    { "Amb R",        { -5.0f,  1.0f, false } },
    { "Kick L",       {  0.0f,  0.0f, true } },
    { "Kick R",       {  0.0f,  0.0f, true } },
    { "Snare top",    {  0.0f,  0.0f, true } },
    { "Snare bottom", { -9.0f,  0.0f, true } },
    { "Hihat",        { -4.0f,  0.35f, true } },
    { "Ride",         { -4.0f, -0.4f, true } },
    { "Tom1",         { -2.0f,  0.25f, true } },
    { "Tom2",         { -2.0f,  0.05f, true } },
    { "Tom3",         { -2.0f, -0.25f, true } },
    { "Tom4",         { -2.0f, -0.5f, true } },
};

// Which close mics belong to which instrument (prefix of the instrument name)
bool micBelongsTo (const juce::String& mic, const juce::String& instrument)
{
    if (mic.startsWith ("Kick"))  return instrument.startsWith ("kick");
    if (mic.startsWith ("Snare")) return instrument.startsWith ("snare");
    if (mic == "Hihat")           return instrument.startsWith ("hihat");
    if (mic == "Ride")            return instrument.startsWith ("ride");
    if (mic.startsWith ("Tom"))   return instrument == "tom_" + mic.substring (3);
    return false;
}

// Names under which the app finds instruments
juce::String outputName (const juce::String& dgName)
{
    if (dgName == "kick_r")          return "kick";
    if (dgName == "snare_on_center") return "snare";
    return dgName;
}

struct SampleOut
{
    juce::String file;
    double power = 0.0;
};

struct InstrumentJob
{
    juce::String name;     // DrumGizmo-Name
    juce::File xml;
    std::vector<SampleOut> result;
    juce::String error;
    double peak = 0.0;
    double bottomCorrelation = 0.0;   // sum over all hits (report only)
    int correlationCount = 0;
};

float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

void processInstrument (InstrumentJob& job, const juce::File& outDir, double targetRate)
{
    const bool flipSnareBottom = juce::SystemStats::getEnvironmentVariable ("KITPREP_FLIP_SNARE_BOTTOM", "0") == "1";

    auto doc = juce::XmlDocument::parse (job.xml);
    if (doc == nullptr)
    {
        job.error = "XML nicht lesbar: " + job.xml.getFullPathName();
        return;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const auto outName = outputName (job.name);
    auto instDir = outDir.getChildFile (outName);
    instDir.createDirectory();

    int index = 0;
    for (auto* sample : doc->getChildByName ("samples")->getChildWithTagNameIterator ("sample"))
    {
        // Each mic can live in its own file: remember file AND channel per entry
        struct Mix { juce::String mic; juce::File file; int channel; float gl, gr; };
        std::vector<Mix> mix;

        for (auto* af : sample->getChildWithTagNameIterator ("audiofile"))
        {
            const auto mic = af->getStringAttribute ("channel");
            auto it = micRules.find (mic);
            if (it == micRules.end())
                continue;   // triggers etc.

            const auto& rule = it->second;
            float db = rule.gainDb;
            if (rule.closeMic && ! micBelongsTo (mic, job.name))
                db -= 14.0f;

            // Equal-Power-Panning
            const float angle = (rule.pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
            const float g = dbToGain (db);
            mix.push_back ({ mic, job.xml.getParentDirectory().getChildFile (af->getStringAttribute ("file")),
                             af->getIntAttribute ("filechannel") - 1, g * std::cos (angle), g * std::sin (angle) });
        }

        // read each required file once
        std::map<juce::String, juce::AudioBuffer<float>> files;
        double fileRate = 0.0;
        int len = 0;
        for (const auto& m : mix)
        {
            const auto key = m.file.getFullPathName();
            if (files.count (key) > 0)
                continue;

            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (m.file));
            if (reader == nullptr)
            {
                job.error = "WAV nicht lesbar: " + key;
                return;
            }
            if (fileRate > 0.0 && std::abs (reader->sampleRate - fileRate) > 0.5)
            {
                job.error = "Unterschiedliche Sampleraten innerhalb eines Schlags: " + key;
                return;
            }
            fileRate = reader->sampleRate;

            auto& buf = files[key];
            buf.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&buf, 0, buf.getNumSamples(), 0, true, true);
            len = std::max (len, buf.getNumSamples());
        }

        auto channelOf = [&] (const Mix& m) -> const float*
        {
            const auto& buf = files[m.file.getFullPathName()];
            return m.channel >= 0 && m.channel < buf.getNumChannels() ? buf.getReadPointer (m.channel) : nullptr;
        };
        auto lengthOf = [&] (const Mix& m) { return files[m.file.getFullPathName()].getNumSamples(); };

        // Snare-bottom polarity: correlation to the top mic (first 30 ms) is only reported.
        // The measurement is too unreliable for automatic flipping; flipping only on request,
        // then uniformly for the whole kit (environment variable KITPREP_FLIP_SNARE_BOTTOM=1).
        float bottomSign = 1.0f;
        const Mix* top = nullptr;
        const Mix* bottom = nullptr;
        for (const auto& m : mix)
        {
            if (m.mic == "Snare top") top = &m;
            if (m.mic == "Snare bottom") bottom = &m;
        }
        // only measure on snare hits: for other instruments the signal is mere bleed
        if (job.name.startsWith ("snare") && top != nullptr && bottom != nullptr
            && channelOf (*top) != nullptr && channelOf (*bottom) != nullptr)
        {
            const float* a = channelOf (*top);
            const float* b = channelOf (*bottom);
            const int n = std::min ({ lengthOf (*top), lengthOf (*bottom), (int) (0.03 * fileRate) });
            double ab = 0.0, aa = 0.0, bb = 0.0;
            for (int i = 0; i < n; ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
            job.bottomCorrelation += ab / std::sqrt (aa * bb + 1.0e-20);
            ++job.correlationCount;
            if (flipSnareBottom)
                bottomSign = -1.0f;
        }

        juce::AudioBuffer<float> out (2, len);
        out.clear();
        for (const auto& m : mix)
        {
            const float* src = channelOf (m);
            if (src == nullptr)
                continue;
            const float sign = &m == bottom ? bottomSign : 1.0f;
            out.addFrom (0, 0, src, lengthOf (m), sign * m.gl);
            out.addFrom (1, 0, src, lengthOf (m), sign * m.gr);
        }

        // Cut off the tail (-80 dB relative to peak level) and fade out over 20 ms
        const float peak = std::max (out.getMagnitude (0, 0, len), out.getMagnitude (1, 0, len));
        const float threshold = peak * dbToGain (-80.0f);
        int end = len;
        while (end > 1 && std::abs (out.getSample (0, end - 1)) < threshold && std::abs (out.getSample (1, end - 1)) < threshold)
            --end;

        const int fade = std::min (end, (int) (0.02 * fileRate));
        end = std::min (len, end + fade);
        out.applyGainRamp (end - fade, fade, 1.0f, 0.0f);
        job.peak = std::max (job.peak, (double) peak);

        // resample to the target rate (default 48 kHz, like PipeWire)
        auto left = hz::resample (out.getReadPointer (0), end, fileRate, targetRate);
        auto right = hz::resample (out.getReadPointer (1), end, fileRate, targetRate);
        float* const chans[] = { left.data(), right.data() };
        const juce::AudioBuffer<float> final (chans, 2, (int) left.size());

        const auto rel = outName + "/" + juce::String (index).paddedLeft ('0', 3) + ".wav";
        auto outFile = outDir.getChildFile (rel);
        outFile.deleteFile();

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (targetRate)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (writer == nullptr)
        {
            job.error = "Kann nicht schreiben: " + outFile.getFullPathName();
            return;
        }
        writer->writeFromAudioSampleBuffer (final, 0, final.getNumSamples());

        job.result.push_back ({ rel, sample->getDoubleAttribute ("power") });
        ++index;
    }
}

} // namespace

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::printf ("Aufruf: kitprep <drumkit.xml> <zielordner> [kitname] [samplerate]\n");
        return 1;
    }

    const juce::File kitXml (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    auto doc = juce::XmlDocument::parse (kitXml);
    if (doc == nullptr)
    {
        std::printf ("Kann %s nicht lesen\n", kitXml.getFullPathName().toRawUTF8());
        return 1;
    }

    const juce::String kitName = argc > 3 ? juce::String (argv[3]) : doc->getStringAttribute ("name");
    const double targetRate = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 48000.0;
    outDir.createDirectory();

    std::vector<InstrumentJob> jobs;
    for (auto* inst : doc->getChildByName ("instruments")->getChildWithTagNameIterator ("instrument"))
        jobs.push_back ({ inst->getStringAttribute ("name"), kitXml.getParentDirectory().getChildFile (inst->getStringAttribute ("file")), {}, {}, 0.0, 0.0, 0 });

    // Worker pool: as many threads as cores, instruments distributed via a counter
    std::atomic<size_t> next { 0 };
    const unsigned numWorkers = std::max (1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (unsigned w = 0; w < numWorkers; ++w)
        workers.emplace_back ([&]
        {
            for (size_t i; (i = next++) < jobs.size(); )
            {
                processInstrument (jobs[i], outDir, targetRate);
                std::printf ("  %-20s %3d Schläge%s%s\n", jobs[i].name.toRawUTF8(), (int) jobs[i].result.size(),
                             jobs[i].correlationCount > 0 ? (" (Korrelation Snare oben/unten: "
                                 + juce::String (jobs[i].bottomCorrelation / jobs[i].correlationCount, 2) + ")").toRawUTF8() : "",
                             jobs[i].error.isNotEmpty() ? (" FEHLER: " + jobs[i].error).toRawUTF8() : "");
            }
        });
    for (auto& t : workers)
        t.join();

    juce::Array<juce::var> instruments;
    double kitPeak = 0.0;
    for (auto& j : jobs)
    {
        if (j.error.isNotEmpty())
            return 1;

        kitPeak = std::max (kitPeak, j.peak);
        juce::Array<juce::var> samples;
        for (auto& s : j.result)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("file", s.file);
            o->setProperty ("power", s.power);
            samples.add (o);
        }
        auto* inst = new juce::DynamicObject();
        inst->setProperty ("name", outputName (j.name));
        inst->setProperty ("samples", samples);
        instruments.add (inst);
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", 1);
    root->setProperty ("name", kitName);
    root->setProperty ("source", "DrumGizmo: " + doc->getStringAttribute ("name"));
    root->setProperty ("license", "CC-BY 4.0");
    root->setProperty ("peak", kitPeak);
    root->setProperty ("sampleRate", targetRate);
    root->setProperty ("instruments", instruments);

    // Pattern voices -> instruments of this kit (only those present)
    const std::pair<const char*, const char*> voiceCandidates[] = {
        { "kick", "kick" }, { "kick.left", "kick_l" },
        { "snare", "snare" }, { "snare.rim", "snare_rim_shot" },
        { "hihat", "hihat_closed1" }, { "hihat.open", "hihat_open" },
        { "ride", "ride" }, { "ride.bell", "ride_bell1" },
        { "crash1", "crash1" }, { "crash2", "crash2" }, { "china", "china_18_inch" },
        { "tom1", "tom_1" }, { "tom2", "tom_2" }, { "tom3", "tom_3" }, { "tom4", "tom_4" },
        { "cowbell", "zilbel" },
    };
    auto* voices = new juce::DynamicObject();
    for (const auto& [voice, inst] : voiceCandidates)
        for (auto& j : jobs)
            if (outputName (j.name) == inst)
                voices->setProperty (voice, juce::String (inst));
    root->setProperty ("voices", voices);

    outDir.getChildFile ("kit.json").replaceWithText (juce::JSON::toString (juce::var (root)));
    std::printf ("Fertig: %d Instrumente, Spitzenpegel %.2f -> %s\n", (int) jobs.size(), kitPeak, outDir.getFullPathName().toRawUTF8());
    return 0;
}
