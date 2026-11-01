#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <optional>

// Which album a song is on, for the view Band › Album › Song (AlbumView): read from alben/alben.json next to the
// program (the shipped list, MusicBrainz) and alben.json in the data folder (one's own additions, they win).
// Bands and songs match without case and signs ("1,000 Eyes" = "1000 Eyes", "Damage, Inc." = "damageinc").
struct AlbumPlace
{
    juce::String album;   // as written in the file
    int year = 0;         // 0: unknown
    int number = 0;       // track number, 0: unknown
    bool live = false;    // a live album
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

private:
    juce::String read (const juce::File&, bool overrides);   // overrides: its songs replace what the shipped list said
    std::map<juce::String, std::map<juce::String, AlbumPlace>> bands;   // key (band) -> key (song) -> place
};
