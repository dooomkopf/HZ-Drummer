#pragma once

#include <juce_core/juce_core.h>

#include <memory>

// MP3 through LAME (libmp3lame, LGPL), loaded while the program runs: building needs no extra package, and where the
// library is missing the export menu greys MP3 out. Variable bitrate at quality V0 (about 245 kbit/s on average:
// up to 320 on cymbal crashes, less where it is quiet). The VBR header at the start lets players seek and know the
// exact length.
class Mp3Encoder
{
public:
    static bool available();   // libmp3lame could be loaded (looked for once)

    Mp3Encoder() = default;
    ~Mp3Encoder();

    bool open (const juce::File&, int sampleRate);   // stereo; false: error() says why
    bool write (const float* left, const float* right, int numSamples);
    bool finish();                                    // the last frames and the VBR header; closes the file
    const juce::String& error() const   { return lastError; }

private:
    struct Api;
    static const Api* api();
    bool fail (const juce::String& why);

    void* lame = nullptr;
    std::unique_ptr<juce::FileOutputStream> out;
    juce::HeapBlock<unsigned char> buffer;
    int bufferSize = 0;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE (Mp3Encoder)
};
