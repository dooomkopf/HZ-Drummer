#pragma once

#include "app/AlbumIndex.h"
#include "app/FolderTree.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// The view Band › "yyyy - Album" › "nn. Song" of songs and own patterns, computed from the entries' band and
// song and the AlbumIndex; nothing of it is stored, the own folders (FolderTree) stay as they are.
// A band with at least one album: songs the index does not know go to "Sonstige" at the end. A band without
// any album: its songs lie right under the band, as in the own folders. An entry without a band: at the top.
// Arrangements lie in their album folder (their label carries the number, see Library::entryLabel);
// patterns lie in a folder per song.
namespace albumview
{
    struct Entry
    {
        std::string id;
        juce::String name, band, song;   // band or song empty: unknown
        bool arrangement = false;        // an arrangement (one entry per song) or a pattern (several per song)
    };

    constexpr const char* restFolder = "Sonstige";

    juce::String albumFolder (const AlbumPlace&);                        // "1986 - Master of Puppets"
    juce::String songFolder (const AlbumPlace&, const juce::String&);    // "01. Battery"
    FolderNode build (const std::vector<Entry>&, const AlbumIndex&);
    juce::StringArray topLevel (const std::vector<Entry>&);              // the bands, as build() puts them on top

    // the (band, song) of an arrangement from the votes of its pattern parts: more than half of them with the
    // same pair -> that song; else all of one band -> the band without a song; else nothing
    using Vote = std::pair<juce::String, juce::String>;
    std::optional<Vote> vote (const std::vector<Vote>&);
}
