#include "app/KitLoader.h"
#include "app/Text.h"
#include "engine/Resampler.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <future>

namespace
{

struct SampleRef
{
    juce::String file;
    float power = 0.0f;
};

struct InstrumentRef
{
    juce::String name;
    std::vector<SampleRef> samples;
};

bool loadSample (juce::AudioFormatManager& formats, const juce::File& file, double targetRate,
                 hz::DrumSample& dest, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Kann nicht lesen: " + file.getFullPathName();
        return false;
    }

    const int len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf (2, len);
    reader->read (&buf, 0, len, 0, true, true);   // mono files end up on both channels

    dest.left = hz::resample (buf.getReadPointer (0), len, reader->sampleRate, targetRate);
    dest.right = hz::resample (buf.getReadPointer (1), len, reader->sampleRate, targetRate);
    return true;
}

} // namespace

std::unique_ptr<hz::DrumKit> loadKit (const juce::File& kitDir, double targetSampleRate, juce::String& error)
{
    const auto json = juce::JSON::parse (kitDir.getChildFile ("kit.json"));
    if (! json.isObject())
    {
        error = "kit.json fehlt oder ist ungültig in "_u + kitDir.getFullPathName();
        return nullptr;
    }

    std::vector<InstrumentRef> refs;
    if (auto* arr = json["instruments"].getArray())
    {
        for (auto& inst : *arr)
        {
            InstrumentRef r;
            r.name = inst["name"].toString();
            if (auto* smps = inst["samples"].getArray())
                for (auto& s : *smps)
                    r.samples.push_back ({ s["file"].toString(), (float) (double) s["power"] });
            refs.push_back (std::move (r));
        }
    }

    auto kit = std::make_unique<hz::DrumKit>();
    kit->name = json["name"].toString().toStdString();
    kit->sampleRate = targetSampleRate;
    kit->instruments.resize (refs.size());

    // One load job per instrument; std::async distributes across the available cores
    std::vector<std::future<juce::String>> jobs;
    for (size_t i = 0; i < refs.size(); ++i)
    {
        jobs.push_back (std::async (std::launch::async, [&, i]
        {
            juce::AudioFormatManager formats;   // own instance per thread
            formats.registerBasicFormats();

            auto& dest = kit->instruments[i];
            dest.name = refs[i].name.toStdString();
            dest.samples.resize (refs[i].samples.size());

            juce::String err;
            for (size_t k = 0; k < refs[i].samples.size(); ++k)
            {
                dest.samples[k].power = refs[i].samples[k].power;
                if (! loadSample (formats, kitDir.getChildFile (refs[i].samples[k].file), targetSampleRate, dest.samples[k], err))
                    return err;
            }
            return juce::String();
        }));
    }

    for (auto& j : jobs)
    {
        auto err = j.get();
        if (err.isNotEmpty() && error.isEmpty())
            error = err;
    }

    if (error.isNotEmpty())
        return nullptr;

    kit->finalise();

    std::vector<std::pair<std::string, std::string>> mapping;
    if (auto* voices = json["voices"].getDynamicObject())
        for (auto& prop : voices->getProperties())
            mapping.push_back ({ prop.name.toString().toStdString(), prop.value.toString().toStdString() });
    else
        mapping = { { "kick", "kick" }, { "snare", "snare" } };   // older kit.json without mapping
    kit->resolveVoices (mapping);
    return kit;
}

juce::File findAppFolder (const juce::String& name)
{
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

    for (int i = 0; i < 6 && dir.exists(); ++i, dir = dir.getParentDirectory())
        if (dir.getChildFile (name).isDirectory())
            return dir.getChildFile (name);

    return {};
}
