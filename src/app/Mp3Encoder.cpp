#include "app/Mp3Encoder.h"

#include "app/Text.h"

#include <type_traits>

// The few LAME functions used, declared here as in lame.h (the C interface has stayed the same since 3.99), so that
// no headers are needed: the library is opened with dlopen and the functions are looked up by name.
struct Mp3Encoder::Api
{
    juce::DynamicLibrary library;
    void* (*init)() = nullptr;
    int (*setInSamplerate) (void*, int) = nullptr;
    int (*setNumChannels) (void*, int) = nullptr;
    int (*setVbr) (void*, int) = nullptr;
    int (*setVbrQuality) (void*, float) = nullptr;
    int (*initParams) (void*) = nullptr;
    int (*encodeFloat) (void*, const float*, const float*, int, unsigned char*, int) = nullptr;
    int (*flush) (void*, unsigned char*, int) = nullptr;
    size_t (*lametagFrame) (void*, unsigned char*, size_t) = nullptr;
    int (*close) (void*) = nullptr;
    bool ok = false;

    Api()
    {
        for (const char* name : { "libmp3lame.so.0", "libmp3lame.so", "libmp3lame.0.dylib", "libmp3lame.dll" })
            if (library.open (name))
                break;
        if (library.getNativeHandle() == nullptr)
            return;
        const auto get = [this] (auto& fn, const char* symbol)
        {
            fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (library.getFunction (symbol));
            return fn != nullptr;
        };
        ok = get (init, "lame_init") && get (setInSamplerate, "lame_set_in_samplerate")
          && get (setNumChannels, "lame_set_num_channels") && get (setVbr, "lame_set_VBR")
          && get (setVbrQuality, "lame_set_VBR_quality") && get (initParams, "lame_init_params")
          && get (encodeFloat, "lame_encode_buffer_ieee_float") && get (flush, "lame_encode_flush")
          && get (lametagFrame, "lame_get_lametag_frame") && get (close, "lame_close");
    }
};

namespace
{
    constexpr int vbrDefault = 4;   // vbr_mtrh, LAME's default VBR mode
}

const Mp3Encoder::Api* Mp3Encoder::api()
{
    static const Api loaded;   // once, thread-safe
    return loaded.ok ? &loaded : nullptr;
}

bool Mp3Encoder::available()
{
    return api() != nullptr;
}

Mp3Encoder::~Mp3Encoder()
{
    if (lame != nullptr)
        api()->close (lame);
}

bool Mp3Encoder::fail (const juce::String& why)
{
    lastError = why;
    return false;
}

bool Mp3Encoder::open (const juce::File& file, int sampleRate)
{
    const auto* lib = api();
    if (lib == nullptr)
        return fail ("libmp3lame fehlt (Paket libmp3lame0)");
    lame = lib->init();
    if (lame == nullptr)
        return fail ("LAME startet nicht");
    lib->setInSamplerate (lame, sampleRate);
    lib->setNumChannels (lame, 2);
    lib->setVbr (lame, vbrDefault);
    lib->setVbrQuality (lame, 0.0f);   // V0
    if (lib->initParams (lame) < 0)
        return fail ("LAME nimmt die Einstellungen nicht an (" + juce::String (sampleRate) + " Hz)");
    out = std::make_unique<juce::FileOutputStream> (file);
    if (! out->openedOk())
        return fail ("Datei nicht anlegbar: " + file.getFullPathName());
    return true;
}

bool Mp3Encoder::write (const float* left, const float* right, int numSamples)
{
    if (lame == nullptr || out == nullptr)
        return fail ("nicht geöffnet"_u);
    const int needed = numSamples + numSamples / 4 + 7200;   // LAME's worst case for one call
    if (needed > bufferSize)
    {
        buffer.malloc ((size_t) needed);
        bufferSize = needed;
    }
    const int bytes = api()->encodeFloat (lame, left, right, numSamples, buffer.get(), bufferSize);
    if (bytes < 0)
        return fail ("LAME-Fehler " + juce::String (bytes));
    return out->write (buffer.get(), (size_t) bytes) || fail ("Schreiben fehlgeschlagen");
}

bool Mp3Encoder::finish()
{
    if (lame == nullptr || out == nullptr)
        return fail ("nicht geöffnet"_u);
    if (bufferSize < 7200)
    {
        buffer.malloc (7200);
        bufferSize = 7200;
    }
    const int bytes = api()->flush (lame, buffer.get(), bufferSize);
    if (bytes < 0 || ! out->write (buffer.get(), (size_t) bytes))
        return fail ("Abschluss fehlgeschlagen");

    // the VBR header: LAME left room for it in the first frame (no ID3 tag before it)
    unsigned char tag[2880];
    const auto tagBytes = api()->lametagFrame (lame, tag, sizeof (tag));
    if (tagBytes > 0 && tagBytes <= sizeof (tag))
    {
        out->flush();
        if (! out->setPosition (0) || ! out->write (tag, tagBytes))
            return fail ("VBR-Kopf nicht geschrieben");
    }
    out->flush();
    const bool ok = out->getStatus().wasOk();
    out = nullptr;
    api()->close (lame);
    lame = nullptr;
    return ok || fail ("Schreiben fehlgeschlagen");
}
