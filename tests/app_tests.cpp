#include "app/AlbumView.h"
// Tests für die App-Schicht (JUCE): Pattern-Dateiformat
#include "app/PatternIO.h"
#include "app/EditorStyle.h"
#include "app/Library.h"
#include "app/MixState.h"
#include "app/QuickSearch.h"
#include "app/PreviewMenuItem.h"
#include "app/SearchQuery.h"
#include "app/SearchIndex.h"
#include "app/TrainerSlots.h"
#include "engine/PatternEdit.h"
#include "engine/TabParser.h"

#include <cmath>
#include <cstdio>
#include <map>

static int failures = 0;

#include "app/Song.h"
void runSongTests();
void runFolderTests();
extern int folderTestFailures;
void checkSongFolder (const juce::File& songs, const std::vector<hz::Pattern>& library);
extern int songTestFailures;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static bool samePattern (const hz::Pattern& a, const hz::Pattern& b)
{
    if (a.id != b.id || a.name != b.name || a.revision != b.revision || a.tags != b.tags) return false;
    if (a.bars.size() != b.bars.size() || a.layers.size() != b.layers.size()) return false;
    if (a.tempo != b.tempo || a.swing.subdivision != b.swing.subdivision || std::abs (a.swing.ratio - b.swing.ratio) > 1e-4f) return false;
    if (std::abs (a.humanize - b.humanize) > 1e-4f || a.humanizeSeed != b.humanizeSeed) return false;
    if (a.origin.parentId != b.origin.parentId || a.origin.parentRevision != b.origin.parentRevision) return false;

    for (size_t i = 0; i < a.bars.size(); ++i)
        if (a.bars[i].grid != b.bars[i].grid || a.bars[i].lengthInQuarters() != b.bars[i].lengthInQuarters()
            || a.bars[i].meter.numGroups != b.bars[i].meter.numGroups)
            return false;

    for (size_t l = 0; l < a.layers.size(); ++l)
    {
        const auto& ea = a.layers[l].events;
        const auto& eb = b.layers[l].events;
        if (a.layers[l].role != b.layers[l].role || ea.size() != eb.size()) return false;
        for (size_t i = 0; i < ea.size(); ++i)
            if (ea[i].id != eb[i].id || ! (ea[i].voice == eb[i].voice) || ea[i].pos != eb[i].pos
                || std::abs (ea[i].velocity - eb[i].velocity) > 1e-3f || ea[i].limb != eb[i].limb || ea[i].flam != eb[i].flam
                || std::abs (ea[i].lagMs - eb[i].lagMs) > 1e-3f)
                return false;
    }
    return true;
}

// Jedes Bibliotheks-Tab übersteht Tab -> Modell -> JSON -> Modell unverändert
static void testLibraryRoundTrip (const juce::File& tabDir)
{
    int count = 0;
    for (auto& f : tabDir.findChildFiles (juce::File::findFiles, true, "*.tab"))
    {
        auto r = hz::parseTabs (f.loadFileAsString().toStdString());
        CHECK (r.error.empty());
        for (auto& p : r.patterns)
        {
            hz::Pattern back;
            juce::String err;
            const bool ok = patternFromJson (juce::JSON::parse (juce::JSON::toString (patternToJson (p))), back, err);
            CHECK (ok);
            if (! ok) std::printf ("  %s: %s\n", p.id.c_str(), err.toRawUTF8());
            CHECK (samePattern (p, back));
            ++count;
        }
    }
    CHECK (count >= 40);
}

// Bearbeitetes Pattern mit Polyrhythmus, Flam, Herkunft und Umlauten speichern und laden
static void testSaveLoadFile()
{
    auto p = hz::parseTabs ("name: Prüfung äöü\ngrid: 1/8\nswing: 1/8 0.62\ncr(3) | X x x |\nsn | . . f . . . X . |\nbd | x . . x x . . . |\n").patterns[0];
    hz::insertBar (p, 0, true);
    p.origin.parentId = "lib.test";
    p.origin.parentRevision = 3;
    p.revision = 7;

    auto file = juce::File::createTempFile (".json");
    juce::String err;
    CHECK (savePattern (p, file, err));

    hz::Pattern back;
    CHECK (loadPattern (file, back, err));
    CHECK (samePattern (p, back));
    CHECK (back.name == "Prüfung äöü");
    file.deleteFile();

    // a hit played later (Strg+Klick im Raster): "lag" in ms, only written when set
    auto lagged = p;
    const auto& first = lagged.layers[0].events[0];
    CHECK (hz::setEventLag (lagged, first.voice, first.pos, 20.0f));
    const auto json = juce::JSON::toString (patternToJson (lagged));
    CHECK (json.contains ("\"lag\"") && ! juce::JSON::toString (patternToJson (p)).contains ("\"lag\""));
    CHECK (patternFromJson (juce::JSON::parse (json), back, err) && samePattern (lagged, back));
    CHECK (std::abs (back.layers[0].events[0].lagMs - 20.0f) < 1e-3f);
    CHECK (hz::setEventLag (lagged, first.voice, first.pos, -60.0f));   // earlier: negative
    CHECK (patternFromJson (juce::JSON::parse (juce::JSON::toString (patternToJson (lagged))), back, err) && samePattern (lagged, back));
    auto damaged = juce::JSON::toString (patternToJson (lagged)).replace ("\"lag\": -60.0", "\"lag\": \"kaputt\"");
    CHECK (damaged.contains ("kaputt") && ! patternFromJson (juce::JSON::parse (damaged), back, err));   // not silently 0

    // kaputte Dateien werden abgelehnt, nicht übernommen
    auto bad = juce::File::createTempFile (".json");
    bad.replaceWithText ("{\"formatVersion\":1,\"id\":\"x\",\"name\":\"y\",\"bars\":[{\"meter\":\"7/8 (3+3)\",\"grid\":\"1/8\"}]}");
    CHECK (! loadPattern (bad, back, err));
    bad.replaceWithText ("{\"formatVersion\":99,\"id\":\"x\",\"name\":\"y\"}");
    CHECK (! loadPattern (bad, back, err));
    bad.replaceWithText ("kein json");
    CHECK (! loadPattern (bad, back, err));
    bad.deleteFile();
}

static void testFuzzySearch()
{
    CHECK (fuzzyScore ("arc", "Carcass CJQ – Intro") > 0);                       // Teilwort
    CHECK (fuzzyScore ("cjq", "Corporal Jigsore Quandary") > 0);                 // Wortanfänge
    CHECK (fuzzyScore ("7/8", "Prog 7/8 (2+2+3)") > 0);
    CHECK (fuzzyScore ("thrash gallop", "Thrash Gallop") > 0);                   // mehrere Wörter
    CHECK (fuzzyScore ("thrash doom", "Thrash Gallop") == 0);                    // alle müssen passen
    CHECK (fuzzyScore ("über", "Tech-Death 7er-Kick über 4/4") > 0);             // Umlaute
    CHECK (fuzzyScore ("xyz", "Carcass") == 0);
    // Wortanfang schlägt Wortmitte, Teilwort schlägt Buchstabenfolge
    CHECK (fuzzyScore ("car", "Carcass") > fuzzyScore ("car", "Scarcity"));
    CHECK (fuzzyScore ("doom", "Doom Half-Time") > fuzzyScore ("doom", "D-Beat ohne Opfer Mut"));
}

// One search core serves both views: names, abbreviations and actual meter/tempo values.
static void testSharedSearch()
{
    const juce::String title = "Exhorder – My Time – Hauptriff";
    const juce::String text = title + " thrash 7/8 204";
    const std::vector<std::pair<int, int>> meters { { 4, 4 }, { 7, 8 } };
    const auto query = SearchQuery::parse ("Exhorder My Time 7/8 200-210 bpm");
    CHECK (query.words == "exhorder my time" && query.meter == "7/8");
    CHECK (query.tempoMin == 200 && query.tempoMax == 210);
    CHECK (query.score (title, text, 204, meters, false) > 0);
    CHECK (query.score (title, text, 180, meters, false) == 0);
    CHECK (query.score (title, text, 204, { { 4, 4 } }, false) == 0);
    CHECK (SearchQuery::parse ("my time").score (title, text, 204, meters, false) > 0);
    CHECK (SearchQuery::parse ("cjq").score ("Corporal Jigsore Quandary", "Corporal Jigsore Quandary", 140, {}, false) > 0);
    CHECK (SearchQuery::parse ("204bpm").score (title, text, 204, meters, false) > 0);
    CHECK (SearchQuery::parse ("204bpm").score (title, text, 150, meters, false) == 0);
    CHECK (SearchQuery::parse ("7/8").score ("7/8", "7/8 taktart", 0, { { 7, 8 } }, false) > 0);
    CHECK (SearchQuery::parse ("200 bpm").score ("7/8", "7/8 taktart", 0, { { 7, 8 } }, false) == 0);
    CHECK (SearchQuery::parse ("sdf").score ("Schneller Death Fill", "Schneller Death Fill song death fill", 180, meters, false) > 0); // real initials
    CHECK (SearchQuery::parse ("sdf").score ("Standard Groove", "Standard Groove viel Schlagzeug und feine Akzente", 180, meters, false) == 0);
    CHECK (SearchQuery::parse ("sdf").score ("Groove", "Groove song death fill", 180, meters, false) == 0); // metadata initials do not count
    CHECK (SearchQuery::parse ("bla").score ("Blast", "Blast grind", 180, meters, false) > 0);
    CHECK (SearchQuery::parse ("bla ").score ("Blast", "Blast grind", 180, meters, false) == 0);
    CHECK (SearchQuery::parse ("bla ").score ("bla Test", "bla Test", 180, meters, false) > 0);
    CHECK (SearchQuery::parse ("140").words == "140"); // bare numbers may be names
    const auto phrase = SearchQuery::parse ("my time");
    CHECK (phrase.score (title, text, 204, meters, true) > phrase.score (title, text, 204, meters, false));

    // the lanes of a pattern are search words: "besen" finds the brushes, "flam" the flams
    const auto brushed = hz::parseTabs ("name: Besen\ngrid: 1/4\nbs | x . x . |\nws | . x . x |\nhp | x . x . |\nsn | . . f . |\n").patterns.at (0);
    const auto words = laneSearchWords (brushed);
    CHECK (words.contains ("besen") && words.contains ("wischer") && words.contains ("pedal") && words.contains ("flam"));
    CHECK (! words.contains ("china") && ! words.contains ("ride"));
    CHECK (SearchQuery::parse ("besen").score ("Besen", words.joinIntoString (" "), 120, { { 4, 4 } }, false) > 0);
}

// All destructive checks operate only on a fresh temporary folder.
static void testLibraryDeletion()
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("hz-delete-test", "", true);
    CHECK (folder.createDirectory());
    const auto patterns = folder.getChildFile ("patterns/nested");
    const auto songs = folder.getChildFile ("songs");
    CHECK (patterns.createDirectory() && songs.createDirectory());
    const auto pattern = patterns.getChildFile ("unrelated-filename.json");
    const auto song = songs.getChildFile ("different-name.json");
    CHECK (pattern.replaceWithText (R"({"id":"user.test"})"));
    CHECK (removeOwnLibraryFile ("lib.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (song.replaceWithText (R"({"id":"song-id","parts":[{"pattern":"user.test"}]})"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    // Embedded copies still count as usage: user wants all used patterns protected.
    CHECK (song.replaceWithText (R"({"id":"song-id","parts":[{"source":"user.test","copy":{"id":"user.test"}}]})"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (song.existsAsFile());
    CHECK (song.replaceWithText (R"({"id":"song-id","parts":[]})")); // isolate subsequent protection cases
    const auto scratch = folder.getChildFile ("ablage.json");
    CHECK (scratch.replaceWithText (R"({"parts":[{"source":"user.test"}]})"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (scratch.deleteFile());
    const auto state = folder.getChildFile ("arranger.json");
    CHECK (state.replaceWithText (R"({"parts":[{"source":"user.test"}]})"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (state.deleteFile());
    CHECK (scratch.replaceWithText ("broken JSON"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (scratch.deleteFile());
    const auto duplicate = patterns.getChildFile ("duplicate.json");
    CHECK (duplicate.replaceWithText (R"({"id":"user.test"})"));
    CHECK (removeOwnLibraryFile ("user.test", folder).isNotEmpty() && pattern.existsAsFile());
    CHECK (duplicate.deleteFile());
    CHECK (removeOwnLibraryFile ("song.song-id", folder).isEmpty() && ! song.exists());
    CHECK (pattern.existsAsFile()); // deleting the arrangement does not delete its patterns
    CHECK (removeOwnLibraryFile ("user.test", folder).isEmpty() && ! pattern.exists());
    auto trash = listOwnTrash (folder);
    CHECK (trash.size() == 2); // arrangement and pattern
    auto patternEntry = std::find_if (trash.begin(), trash.end(), [] (const auto& entry) { return entry.libraryId == "user.test"; });
    CHECK (patternEntry != trash.end());
    if (patternEntry != trash.end())
    {
        CHECK (pattern.replaceWithText (R"({"id":"user.test"})"));
        CHECK (restoreOwnTrash (patternEntry->key, folder).isNotEmpty()); // never overwrite
        CHECK (pattern.deleteFile());
        CHECK (restoreOwnTrash (patternEntry->key, folder).isEmpty() && pattern.existsAsFile());
        CHECK (listOwnTrash (folder).size() == 1);
    }
    CHECK (restoreOwnTrash ("../escape", folder).isNotEmpty());
    CHECK (emptyOwnTrash (folder) && listOwnTrash (folder).empty() && emptyOwnTrash (folder));   // also when already empty
    CHECK (removeOwnLibraryFile ("user.missing", folder).isNotEmpty());
    CHECK (folder.deleteRecursively());
}

// Mixer: welche Kanäle ein Plan überhaupt erklingen lässt (die übrigen Regler werden blass)
static void testChannelsInUse()
{
    hz::PatternPlan plan;
    plan.bars.push_back ({});
    hz::PlanEvent kick;
    kick.voice = { hz::Instrument::Kick, hz::Articulation::Normal };
    plan.events.push_back (kick);

    auto used = mix::channelsUsedBy (plan);
    CHECK (used[mix::Kick] && used[mix::Master]);
    CHECK (! used[mix::Toms] && ! used[mix::Click]);

    plan.bars.front().click = true;   // a "Klick" bar of a song
    CHECK (mix::channelsUsedBy (plan)[mix::Click]);

    // Grundwerte der Regler: Klick etwas leiser als das Schlagzeug
    CHECK (mix::defaultDb (mix::Click) == -8.0f && mix::defaultDb (mix::Kick) == 0.0f && mix::defaultDb (mix::Master) == 0.0f);
}

static void testMenuClickSeparation()
{
    using Result = MenuClickGesture::Result;
    MenuClickGesture gesture;
    const juce::ModifierKeys none;
    const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier);
    const juce::ModifierKeys right (juce::ModifierKeys::rightButtonModifier);
    CHECK (gesture.release (none, true) == Result::None);
    gesture.press (right);
    CHECK (gesture.release (none, true) == Result::Context); // button flag lost on release
    gesture.press (right);
    CHECK (gesture.release (left, true) == Result::Context);
    gesture.press (left);
    CHECK (gesture.release (right, true) == Result::Context);
    gesture.press (left);
    CHECK (gesture.release (none, true) == Result::Select);
    gesture.press (left);
    CHECK (gesture.release (none, false) == Result::None);
    CHECK (gesture.release (none, true) == Result::None);
}

// the editor's hint "Swing wirkt in diesem Raster auf keine Stelle": swing needs an offbeat of its subdivision
static void testSwingFindsPlace()
{
    using editorstyle::swingFindsPlace;
    auto triplets = hz::parseTabs ("name: 3 gegen 2\ngrid: 1/12\nswing: 1/8 0.667\nsn | x . . . x . . . x . . . |\nbd | x . . . . . x . . . . . |\n").patterns.at (0);
    CHECK (! swingFindsPlace (triplets));    // no straight "und" in the grid, no hit on one
    triplets.bars[0].grid = { 1, 8 };
    CHECK (swingFindsPlace (triplets));      // the grid has the "und" places now

    auto eighths = hz::parseTabs ("name: Achtel\ngrid: 1/8\nswing: 1/16 0.58\nhh | x x x x x x x x |\n").patterns.at (0);
    CHECK (! swingFindsPlace (eighths));     // sixteenth swing, but no "e" or "a" anywhere
    eighths.swing = { { 1, 8 }, 0.58f };
    eighths.bars[0].grid = { 1, 4 };
    CHECK (swingFindsPlace (eighths));       // no grid place, but the hits on the "und" move (drawn as diamonds)
    eighths.swing.ratio = 0.5f;
    CHECK (! swingFindsPlace (eighths));     // no swing
}

// Strg(+Shift)+Klick im Editor: 10-ms-Stufen; ein genauer Wert aus einem Import springt erst auf die nächste Stufe
static void testSteppedLag()
{
    using editorstyle::steppedLag;
    CHECK (steppedLag (0.0f, false) == 10.0f && steppedLag (0.0f, true) == -10.0f);
    CHECK (steppedLag (20.0f, false) == 30.0f && steppedLag (20.0f, true) == 10.0f);
    CHECK (steppedLag (40.8f, false) == 50.0f && steppedLag (40.8f, true) == 40.0f);      // AC/DC-Import
    CHECK (steppedLag (-40.8f, false) == -40.0f && steppedLag (-40.8f, true) == -50.0f);
    CHECK (steppedLag (95.0f, false) == 100.0f && steppedLag (100.0f, false) == 100.0f);   // nicht über ±100
    CHECK (steppedLag (-100.0f, true) == -100.0f);
}

// the grid the editor suggests for a bar with diamonds
static void testFittingGrid()
{
    using editorstyle::gridHoldsHits;
    using editorstyle::fittingGrid;
    auto withHits = [] (const char* tabs, std::initializer_list<hz::Rational> extra)
    {
        auto p = hz::parseTabs (tabs).patterns.at (0);
        for (auto pos : extra)
        {
            auto hit = p.layers[0].events[0];
            hit.pos = pos;
            p.layers[0].events.push_back (hit);
        }
        return p;
    };
    // quarters and a triplet ("Lernen: 3 gegen 2 (Rauten)"): 1/12
    auto quarters = withHits ("name: a\ngrid: 1/8\nhh | x . x . x . x . |\n", { { 4, 3 } });
    CHECK (! gridHoldsHits (quarters, 0, { 1, 8 }) && gridHoldsHits (quarters, 0, { 1, 12 }));
    CHECK (fittingGrid (quarters, 0) == hz::Rational (1, 12));
    // eighths and triplets ("Polyrhythmus 3 gegen 2"): 1/24
    CHECK (fittingGrid (withHits ("name: b\ngrid: 1/8\nhh | x x x x x x x x |\n", { { 4, 3 }, { 8, 3 } }), 0) == hz::Rational (1, 24));
    // sixteenths and two 32nd triplets (Exhorder, Riff 1): only the new 1/48
    const auto exhorder = withHits ("name: c\ngrid: 1/16\nhh | x x x x x x x x x x x x x x x x |\n", { { 17, 6 }, { 35, 12 } });
    CHECK (fittingGrid (exhorder, 0) == hz::Rational (1, 48));
    // a 32nd and a triplet together: no grid of the menu has a place for both
    CHECK (fittingGrid (withHits ("name: d\ngrid: 1/16\nhh | x . . . . . . . . . . . . . . . |\n", { { 1, 8 }, { 1, 3 } }), 0).num == 0);

    // for swing ("Lernen: 3 gegen 2 (feines Raster)"): every hit keeps its cell, and places the swing moves come in
    auto triplets = hz::parseTabs ("name: e\ngrid: 1/12\nswing: 1/8 0.667\nsn | x . . . x . . . x . . . |\n").patterns.at (0);
    CHECK (! editorstyle::barHasSwingPlace (triplets, 0, { 1, 12 }) && editorstyle::barHasSwingPlace (triplets, 0, { 1, 24 }));
    CHECK (fittingGrid (triplets, 0, true) == hz::Rational (1, 24));   // eighth swing: the straight "und"
    triplets.swing.subdivision = { 1, 16 };
    CHECK (fittingGrid (triplets, 0, true) == hz::Rational (1, 48));   // sixteenth swing: "e" and "a"
}

// Trainer-Speicherplätze: speichern und wieder laden, freie Plätze bleiben frei, kaputte Werte werden begrenzt,
// eine kaputte oder fehlende Datei gibt lauter freie Plätze
static void testTrainerSlots()
{
    const auto file = juce::File::createTempFile (".json");
    trainerslots::Slots slots;
    slots[0].name = "Seven T. 5-8, 100\u2192140";
    slots[0].patternId = "user.necrophagist-seven-03-riff-b";
    slots[0].loopFirst = 4;
    slots[0].loopLast = 7;
    slots[0].loopOn = true;
    slots[0].from = 100.0;
    slots[0].to = 140.0;
    slots[0].step = 2.0;
    slots[0].repeats = 3;
    slots[3].name = "Metronom 7/8";
    slots[3].meterKey = "7/8";
    CHECK (trainerslots::save (slots, file));
    const auto back = trainerslots::load (file);
    CHECK (back[0].name == slots[0].name && back[0].patternId == slots[0].patternId);
    CHECK (back[0].loopFirst == 4 && back[0].loopLast == 7 && back[0].loopOn);
    CHECK (back[0].from == 100.0 && back[0].to == 140.0 && back[0].step == 2.0 && back[0].repeats == 3);
    CHECK (back[3].meterKey == "7/8" && back[3].patternId.empty() && back[3].loopFirst < 0);
    CHECK (back[1].isFree() && back[7].isFree());

    file.replaceWithText (R"({"plaetze": [{"name": "Zu wild", "baustein": "x", "von": 500, "bis": 20, "wiederholungen": 9, "loopVon": 5, "loopBis": 2}]})");
    const auto wild = trainerslots::load (file);
    CHECK (wild[0].from < wild[0].to && wild[0].to <= 320.0 && wild[0].repeats == 5 && wild[0].loopFirst < 0);

    CHECK (! trainerslots::save (slots, juce::File ("/proc/hz-drummer-gibt-es-nicht/trainer.json")));   // not writable: false

    file.replaceWithText ("kein JSON");
    const auto broken = trainerslots::load (file);
    CHECK (broken[0].isFree());
    file.deleteFile();
    CHECK (trainerslots::load (file)[0].isFree());
}

// "alben <datenordner> [<alben.json des Programms>]": wie die Album-Ansicht einen Datenordner einordnet, nur gelesen;
// Bausteine über ihre Tags, Arrangements über die Stimmen ihrer Teile (albumview::vote), wie Library::placeSongs
static int dumpAlbumView (const juce::File& data, const juce::File& shipped)
{
    AlbumIndex index;
    if (const auto error = index.load (shipped, data.getChildFile ("alben.json")); error.isNotEmpty())
        std::printf ("alben.json: %s\n", error.toRawUTF8());
    std::map<std::string, std::pair<juce::String, juce::String>> tagsOf;   // pattern id -> (band, song)
    std::vector<albumview::Entry> patterns;
    for (const auto& f : data.getChildFile ("patterns").findChildFiles (juce::File::findFiles, true, "*.json"))
    {
        const auto v = juce::JSON::parse (f);
        juce::String band, song;
        if (const auto* tags = v["tags"].getArray())
            for (const auto& t : *tags)
            {
                const auto s = t.toString();
                if (s.startsWithIgnoreCase ("band:") && band.isEmpty()) band = s.substring (5).trim();
                if (s.startsWithIgnoreCase ("song:") && song.isEmpty()) song = s.substring (5).trim();
            }
        const auto id = v["id"].toString().toStdString();
        tagsOf[id] = { band, song };
        patterns.push_back ({ id, v["name"].toString(), band, song, false });
    }
    std::vector<albumview::Entry> songs;
    for (const auto& f : data.getChildFile ("songs").findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        const auto v = juce::JSON::parse (f);
        std::vector<albumview::Vote> votes;
        if (const auto* parts = v["parts"].getArray())
            for (const auto& part : *parts)
            {
                if (part.hasProperty ("type") && part["type"].toString() != "pattern")
                    continue;   // v2: a rest or a click votes not
                if (part["copy"].isObject())   // v2: the part's own copy carries the tags
                {
                    juce::String band, song;
                    if (const auto* tags = part["copy"]["tags"].getArray())
                        for (const auto& t : *tags)
                        {
                            const auto s = t.toString();
                            if (s.startsWithIgnoreCase ("band:") && band.isEmpty()) band = s.substring (5).trim();
                            if (s.startsWithIgnoreCase ("song:") && song.isEmpty()) song = s.substring (5).trim();
                        }
                    if (band.isNotEmpty())
                        votes.push_back ({ band, song });
                    continue;
                }
                const auto id = (part.hasProperty ("source") ? part["source"] : part["pattern"]).toString().toStdString();
                if (const auto it = tagsOf.find (id); it != tagsOf.end() && it->second.first.isNotEmpty())
                    votes.push_back (it->second);
            }
        const auto place = albumview::vote (votes);
        songs.push_back ({ "song." + v["id"].toString().toStdString(), v["name"].toString(),
                           place ? place->first : juce::String(), place ? place->second : juce::String(), true });
    }
    const auto show = [] (const char* title, const FolderNode& root)
    {
        std::printf ("%s: %d oben ohne Ordner\n", title, (int) root.items.size());
        for (const auto& band : root.folders)
        {
            std::printf ("  %s: %d Ordner, %d direkt\n", band.name.toRawUTF8(), (int) band.folders.size(), (int) band.items.size());
            for (const auto& album : band.folders)
                std::printf ("    %s: %d Unterordner, %d Eintraege\n", album.name.toRawUTF8(), (int) album.folders.size(), (int) album.items.size());
        }
    };
    show ("Bausteine", albumview::build (patterns, index));
    show ("Arrangements", albumview::build (songs, index));
    std::printf ("%d Bausteine, %d Arrangements, %d Bands mit Alben\n", (int) patterns.size(), (int) songs.size(), index.bandCount());
    return 0;
}

int main (int argc, char* argv[])
{
    if (argc > 2 && juce::String (argv[1]) == "alben")   // the shipped list: given, else alben/alben.json of the working folder
        return dumpAlbumView (juce::File (juce::String (argv[2])), argc > 3 ? juce::File (juce::String (argv[3]))
                                                                             : juce::File::getCurrentWorkingDirectory().getChildFile ("alben/alben.json"));
    const auto tabDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "library-src");
    testLibraryRoundTrip (tabDir);
    testSaveLoadFile();
    testFuzzySearch();
    testSharedSearch();
    testLibraryDeletion();
    testMenuClickSeparation();
    testChannelsInUse();
    testSwingFindsPlace();
    testSteppedLag();
    testFittingGrid();
    testTrainerSlots();
    runSongTests();
    runFolderTests();

    // optional: zusätzlich einen Ordner mit Pattern-Dateien prüfen (z.B. eigene/importierte Patterns)
    if (argc > 2)
    {
        juce::StringArray errors;
        const auto patterns = loadPatternFolder (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), errors);
        std::printf ("%d Pattern-Dateien geladen, %d fehlerhaft\n", (int) patterns.size(), errors.size());
        for (auto& e : errors)
            std::printf ("  FEHLER %s\n", e.toRawUTF8());
        CHECK (errors.isEmpty());

        // optional: and the user's songs against these patterns plus the bundled library
        if (argc > 3)
        {
            auto all = loadPatternFolder (juce::File::getCurrentWorkingDirectory().getChildFile ("library"), errors);
            all.insert (all.end(), patterns.begin(), patterns.end());
            checkSongFolder (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]), all);
        }
    }
    failures += songTestFailures + folderTestFailures;

    if (failures == 0)
        std::printf ("Alle App-Tests bestanden.\n");
    return failures == 0 ? 0 : 1;
}
