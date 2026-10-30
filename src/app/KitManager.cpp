#include "app/KitManager.h"

#include "app/AudioEngine.h"
#include "app/KitLoader.h"
#include "app/Text.h"

KitManager::KitManager (juce::File kitFolder) : folder (std::move (kitFolder))
{
    if (! folder.isDirectory())
        statusText = "Kein Kit gefunden (Ordner kits/) - nur Klick verfügbar"_u;
}

KitManager::~KitManager()
{
    // the engine must be shut down before this point (it may still reference our kits)
    if (pending.valid())
        pending.wait();
}

juce::File KitManager::findDefaultKit()
{
    auto dirs = findKitsFolder().findChildFiles (juce::File::findDirectories, false);
    dirs.sort();
    for (auto& d : dirs)
        if (d.getChildFile ("kit.json").existsAsFile())
            return d;
    return {};
}

void KitManager::startLoad (double rate)
{
    if (! folder.isDirectory() || pending.valid())
        return;

    statusText = "Lade Kit ...";
    pending = std::async (std::launch::async, [dir = folder, rate]
    {
        LoadResult r;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        try
        {
            r.kit = loadKit (dir, rate, r.error);
        }
        catch (const std::exception& e)   // e.g. out of memory: a "Kit-Fehler" in the status, not the end of the program
        {
            r.kit = nullptr;
            r.error = juce::String (e.what());
        }
        r.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        return r;
    });
}

bool KitManager::poll (AudioEngine& engine)
{
    const auto before = statusText;

    // Free replaced kits only once the audio thread has confirmed the newest kit. Before that it may
    // still be switching between older ones (reading kitInUse alone is not enough).
    if (engine.kitInUse() == current.get())
        retired.clear();

    if (pending.valid() && pending.wait_for (std::chrono::seconds (0)) == std::future_status::ready)
    {
        auto result = pending.get();
        if (result.kit == nullptr)
        {
            statusText = "Kit-Fehler: " + result.error;
        }
        else
        {
            if (current != nullptr)
                retired.push_back (std::move (current));
            current = std::move (result.kit);
            engine.setKit (current.get());
            statusText = "Kit: " + juce::String (current->name) + "  (" + juce::String (result.seconds, 1) + " s geladen, "
                       + juce::String ((int) current->sampleRate) + " Hz)";
        }
    }

    // the sound card runs at a different sample rate than the kit -> reload to match
    const double rate = engine.deviceSampleRate();
    const bool needsLoad = current == nullptr ? (folder.isDirectory() && ! statusText.startsWith ("Kit-Fehler"))
                                              : std::abs (current->sampleRate - rate) > 0.5;
    if (rate > 0.0 && ! pending.valid() && needsLoad)
        startLoad (rate);

    return statusText != before;
}
