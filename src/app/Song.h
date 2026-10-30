#pragma once

#include "engine/Pattern.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <string>
#include <vector>

// Song structure: a sequence of parts, each played 'repeats' times (see docs/ARRANGER.md).
//
// File format v2 (written by the arranger): every pattern part carries its own copy of the
// pattern plus the id it was taken from, so a song always sounds the same even if the source
// pattern changes or disappears. A part may use only some bars of its pattern. Besides patterns
// there are rest bars (silence) and click bars (metronome only).
//
// The "Ablage" (collected candidates) is not part of a song: it is one list of its own that is
// kept across songs and sessions (see loadPartList/savePartList).
//
// Format v1 (older song files) is still read: parts reference library patterns by id. Such
// files are never rewritten automatically; saving from the arranger writes v2.
enum class PartKind { Pattern, Rest, Click };

struct SongPart
{
    std::string uid;                             // identity of this block inside its song: kept when it moves,
                                                 // new for a duplicate; dialogs and the raster address blocks by it
    PartKind kind = PartKind::Pattern;
    std::string sourceId;                        // pattern the part comes from (v1: the reference itself)
    std::shared_ptr<const hz::Pattern> copy;     // v2: the song's own copy; null = look up sourceId (v1).
                                                 // Shared and immutable, so snapshots of a song stay cheap.
    int firstBar = 0, numBars = 0;               // bar range within the pattern, 0-based; numBars 0 = all
    hz::Meter meter = hz::Meter::make (4, 4);    // rest/click parts
    int restBars = 1;                            // rest/click parts: length in bars
    std::string label;                           // display name, e.g. "Strophe"; empty = derived
    int repeats = 1;
    bool edited = false;                         // the copy was changed inside the song (local variant)
};

struct Song
{
    static constexpr int formatVersion = 2;

    std::string id, name;
    double tempo = 0.0;    // 0 = tempo of the first pattern part
    std::vector<SongPart> parts;
};

// One pass of a part in the finished sequence (for display)
struct SongSpan
{
    juce::String label;
    juce::String source;   // name of the pattern the part comes from (shown when label differs)
    int part = 0;          // Index in Song::parts
    int repeat = 0, repeats = 1;
    int firstBar = 0, numBars = 0;
    int colour = 0;        // parts from the same source share a colour index ...
    int shape = 0;         // ... and a mark (theme::drawSourceBadge)
    bool edited = false;         // SongPart::edited
    bool sourceDiffers = false;  // the library's version of the source differs from the copy
};

bool loadSong (const juce::File&, Song&, juce::String& error);      // v1 or v2
bool saveSong (const Song&, const juce::File&, juce::String& error); // always v2; see embedCopies
juce::var songToJson (const Song&);                                  // the v2 form (also used to compare contents)

// v1 parts reference library patterns: give every part its own copy.
// Fails if a source is missing; the song is left unchanged then.
bool embedCopies (Song&, const std::vector<hz::Pattern>& library, juce::String& error);

// A plain list of parts in the same JSON form (the "Ablage" file); a missing file is an empty list
bool loadPartList (const juce::File&, std::vector<SongPart>&, juce::String& error);
bool savePartList (const std::vector<SongPart>&, const juce::File&, juce::String& error);

// Colour index (see theme::partColour) of everything that comes from this source pattern: a
// stable hash of the id, so Finden, Ablage and sequence agree, in every order and session
int sourceColourIndex (const std::string& sourceId);
int sourceShapeIndex (const std::string& sourceId);   // 0..9, the geometric mark of the badge
int partColourIndex (const SongPart&);   // rest and click parts have fixed colours
int partShapeIndex (const SongPart&);

std::string newBlockUid();

// Same source and bar range (rest/click: meter and length): "this Ablage card is used in the sequence"
bool sameSource (const SongPart&, const SongPart&);
// ... and the same content: a locally edited copy counts as a block of its own in the Ablage
bool sameBlock (const SongPart&, const SongPart&);

// Same hits, bars and settings? (id, revision and origin are ignored)
bool samePatternContent (const hz::Pattern&, const hz::Pattern&);

// The library has the part's source and its content differs from the copy (update available)
bool sourceDiffers (const SongPart&, const std::vector<hz::Pattern>& library);

// What a part plays once: its pattern (copy or library), cut to the bar range; rest and click
// parts give empty bars in their meter. Fails for a missing v1 source or an invalid range.
bool partPattern (const SongPart&, const std::vector<hz::Pattern>& library, hz::Pattern& out, juce::String& error);

// Display name of a part: its label, otherwise derived ("Groove A (T3-4)", "Pause 4/4")
juce::String partTitle (const SongPart&, const hz::Pattern& played);

// Joins the parts (with repeats) into one pattern for display and editing.
bool buildSongPattern (const Song&, const std::vector<hz::Pattern>& library,
                       hz::Pattern& result, std::vector<SongSpan>& spans, juce::String& error);

// Validate before accepting an edit; permit gradual reductions of legacy over-limit songs.
juce::String songChangeError (const Song& before, const Song& after, const std::vector<hz::Pattern>& library);
