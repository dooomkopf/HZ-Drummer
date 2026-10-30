// Tests für die App-Schicht (JUCE): Pattern-Dateiformat
#include "app/PatternIO.h"
#include "app/Library.h"
#include "app/MixState.h"
#include "app/QuickSearch.h"
#include "app/PreviewMenuItem.h"
#include "app/SearchQuery.h"
#include "engine/PatternEdit.h"
#include "engine/TabParser.h"

#include <cmath>
#include <cstdio>

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
                || std::abs (ea[i].velocity - eb[i].velocity) > 1e-3f || ea[i].limb != eb[i].limb || ea[i].flam != eb[i].flam)
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

int main (int argc, char* argv[])
{
    const auto tabDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "library-src");
    testLibraryRoundTrip (tabDir);
    testSaveLoadFile();
    testFuzzySearch();
    testSharedSearch();
    testLibraryDeletion();
    testMenuClickSeparation();
    testChannelsInUse();
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
