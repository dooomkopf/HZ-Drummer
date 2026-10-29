#pragma once

#include <juce_core/juce_core.h>

// UTF-8 text constants for the UI: "Größe"_u
// (juce::String from const char* expects only ASCII and would corrupt umlauts)
inline juce::String operator""_u (const char* text, std::size_t length)
{
    return juce::String::fromUTF8 (text, (int) length);
}
