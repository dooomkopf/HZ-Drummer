#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

// Which album a song is on, for the view Band › Album › Song (AlbumView): read from alben/alben.json next to the
// program (the shipped list, MusicBrainz) and alben.json in the data folder (one's own additions, they win).
// Bands and songs match without case and signs ("1,000 Eyes" = "1000 Eyes", "Damage, Inc." = "damageinc").
struct AlbumPlace
{
    juce::String album;   // as written in the file
    int year = 0;         // 0: unknown
    int number = 0;       // track number, 0: unknown
    bool live = false;    // a live album
    bool listed = false;  // the number comes from the album's whole track list (not a single entry)
};

class AlbumIndex
{
public:
    // both files may be missing; a broken one is reported and skipped. Returns "" or what was wrong
    juce::String load (const juce::File& shipped, const juce::File& own);

    bool hasAlbums (const juce::String& band) const;
    std::optional<AlbumPlace> find (const juce::String& band, const juce::String& song) const;
    int bandCount() const   { return (int) bands.size(); }

    static juce::String key (const juce::String& text);   // "Damage, Inc." -> "damageinc"

    // the bands of an import's alben.json (staging) that neither the shipped list nor the own file knows go into
    // the own file; what is there stays as it is. added: how many bands came. Returns "" or what went wrong
    juce::String mergeNewBands (const juce::File& staging, const juce::File& own, int& added) const;

    // as mergeNewBands; besides, each imported (band, song) of a band the lists know that has no album there gets
    // its own from the staging file, alone, under "nummern" (an album the lists know keeps its name and year; off its
    // track list the number is 0). Nothing that has a place moves. As tools/gpdrums/install_band.py
    using Songs = std::vector<std::pair<juce::String, juce::String>>;
    juce::String mergeAlbums (const juce::File& staging, const juce::File& own, const Songs& imported,
                              int& addedBands, int& addedSongs) const;
    static Songs importedSongs (const juce::File& patternFolder);   // (band, song) from the tags of an import

private:
    int mergeSongs (const juce::DynamicObject& bandsIn, juce::DynamicObject& ownBands, const Songs& imported,
                    const std::set<juce::String>& knownBefore) const;
    juce::String read (const juce::File&, bool overrides);   // overrides: its songs replace what the shipped list said
    std::map<juce::String, std::map<juce::String, AlbumPlace>> bands;   // key (band) -> key (song) -> place
};
