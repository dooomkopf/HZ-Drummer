// Tests for the song format (v1 references, v2 copies, bar ranges, rest/click parts, Ablage)
// and the arranger model (undo/redo, Ablage)
#include "app/ArrangerModel.h"
#include "app/BadgeRegistry.h"
#include "app/PatternIO.h"
#include "app/Song.h"
#include "engine/TabParser.h"

#include <cstdio>

extern int songTestFailures;
int songTestFailures = 0;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++songTestFailures; } } while (0)

namespace
{
    std::vector<hz::Pattern> smallLibrary()
    {
        // A: 4 bars 4/4, one snare per bar on beat 1; B: 1 bar 7/8
        auto a = hz::parseTabs ("name: Groove A\nid: user.a\ngrid: 1/4\nsn | x . . . | x . . . | x . . . | x . . . |\n").patterns[0];
        auto b = hz::parseTabs ("name: Fill B\nid: user.b\nmeter: 7/8\ngrid: 1/8\nbd | x . x . x . . |\n").patterns[0];
        return { a, b };
    }

    juce::File tempFile (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hz-song-test-" + name);
    }

    int countEvents (const hz::Pattern& p)
    {
        int n = 0;
        for (auto& l : p.layers)
            n += (int) l.events.size();
        return n;
    }
}

static void testReadV1()
{
    const auto f = tempFile ("v1.json");
    f.replaceWithText (R"({ "formatVersion": 1, "id": "s1", "name": "Alt", "tempo": 0,
        "parts": [ { "pattern": "user.a", "label": "Strophe", "repeats": 2 },
                   { "pattern": "user.b", "label": "", "repeats": 1 },
                   { "pattern": "user.a", "label": "Strophe 2", "repeats": 1 } ] })");
    Song s;
    juce::String err;
    CHECK (loadSong (f, s, err));
    CHECK (s.parts.size() == 3 && ! s.parts[0].copy && s.parts[0].sourceId == "user.a" && s.parts[0].repeats == 2);

    hz::Pattern built;
    std::vector<SongSpan> spans;
    CHECK (buildSongPattern (s, smallLibrary(), built, spans, err));
    CHECK (built.bars.size() == 4 * 2 + 1 + 4);
    CHECK (spans.size() == 4);
    CHECK (spans[0].colour == spans[3].colour);   // same source, same colour (stable hash of the source id)
    CHECK (spans[2].label == "Fill B");                                                // no label: pattern name

    // a missing source is reported, never skipped silently
    auto lib = smallLibrary();
    lib.pop_back();
    CHECK (! buildSongPattern (s, lib, built, spans, err) && err.contains ("user.b"));
    f.deleteFile();
}

static void testWriteV2RoundTrip()
{
    Song s;
    s.id = "s2";
    s.name = "Neu";
    s.tempo = 150.0;
    SongPart a;  a.sourceId = "user.a"; a.firstBar = 2; a.numBars = 2; a.repeats = 3; a.label = "Ende";
    SongPart rest; rest.kind = PartKind::Rest; rest.restBars = 2;
    SongPart click; click.kind = PartKind::Click; click.meter = hz::Meter::make (7, 8, { 2, 2, 3 }); click.restBars = 1;
    SongPart b;  b.sourceId = "user.b";
    s.parts = { a, rest, click };

    juce::String err;
    Song unsaved = s;
    CHECK (! saveSong (unsaved, tempFile ("x.json"), err));   // references without copies are refused

    CHECK (embedCopies (s, smallLibrary(), err));
    CHECK (s.parts[0].copy && s.parts[0].copy->name == "Groove A");

    const auto f = tempFile ("v2.json");
    CHECK (saveSong (s, f, err));
    Song back;
    CHECK (loadSong (f, back, err));
    CHECK (back.parts.size() == 3);
    CHECK (back.parts[0].copy && back.parts[0].firstBar == 2 && back.parts[0].numBars == 2 && back.parts[0].repeats == 3);
    CHECK (back.parts[0].label == "Ende" && back.parts[0].sourceId == "user.a");
    CHECK (back.parts[1].kind == PartKind::Rest && back.parts[1].restBars == 2);
    CHECK (back.parts[2].kind == PartKind::Click && back.parts[2].meter.numerator == 7 && back.parts[2].meter.numGroups == 3);

    // the Ablage file: same part form, a missing file is an empty list
    std::vector<SongPart> ablage { makePatternPart (smallLibrary()[1]), rest }, ablageBack;
    const auto af = tempFile ("ablage.json");
    af.deleteFile();
    CHECK (loadPartList (af, ablageBack, err) && ablageBack.empty());
    CHECK (savePartList (ablage, af, err) && loadPartList (af, ablageBack, err));
    CHECK (ablageBack.size() == 2 && ablageBack[0].copy && ablageBack[0].copy->name == "Fill B" && ablageBack[1].kind == PartKind::Rest);
    af.deleteFile();

    // plays without the library: the copies are enough
    hz::Pattern built;
    std::vector<SongSpan> spans;
    CHECK (buildSongPattern (back, {}, built, spans, err));
    CHECK (built.bars.size() == 2 * 3 + 2 + 1);
    CHECK (countEvents (built) == 2 * 3);   // two snares per pass of bars 3-4, rests and clicks are silent
    CHECK (built.tempo == 150.0);
    CHECK (spans[0].label == "Ende" && spans[3].label.startsWith ("Pause"));
    f.deleteFile();
}

static void testBarRange()
{
    auto lib = smallLibrary();
    SongPart p;
    p.sourceId = "user.a";
    p.firstBar = 1;
    p.numBars = 2;
    hz::Pattern out;
    juce::String err;
    CHECK (partPattern (p, lib, out, err));
    CHECK (out.bars.size() == 2 && countEvents (out) == 2);
    CHECK (out.layers[0].events[0].pos == hz::Rational (0));     // shifted to the start
    CHECK (partTitle (p, out) == juce::String ("Groove A (T. 2") + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + "3)");

    p.numBars = 9;   // beyond the pattern
    CHECK (! partPattern (p, lib, out, err));

    // an invalid range in a file is reported when loading
    const auto f = tempFile ("bad.json");
    f.replaceWithText (R"({ "formatVersion": 2, "id": "s3", "name": "Kaputt",
        "parts": [ { "type": "pattern", "source": "user.a", "bars": [3, 1] } ] })");
    Song s;
    CHECK (! loadSong (f, s, err) && err.contains ("Taktbereich"));
    f.deleteFile();
}

static void testArrangerModel()
{
    const auto lib = smallLibrary();
    ArrangerModel m;
    int scratchSaves = 0;
    m.onScratchChange = [&] { ++scratchSaves; };

    Song s;
    s.id = "neu";
    s.name = "Neu";
    m.load (s);
    CHECK (! m.hasUnsavedChanges() && ! m.canUndo());

    m.insertParts (0, { makePatternPart (lib[0]), makePatternPart (lib[1]) });   // one step
    m.undo();
    CHECK (! m.hasUnsavedChanges());   // back at the loaded content = saved
    m.redo();
    m.insertParts (0, { makePatternPart (lib[0]), makePatternPart (lib[1]) });
    m.undo();
    CHECK (m.song().parts.size() == 2 && m.hasUnsavedChanges());
    m.setRepeats (0, 3);
    m.movePart (1, 0);
    CHECK (m.song().parts[0].sourceId == "user.b" && m.song().parts[1].repeats == 3);
    m.setRange (1, 2, 2);
    CHECK (m.song().parts[1].firstBar == 2 && m.song().parts[1].numBars == 2);
    m.setRange (1, 0, 4);   // all bars = whole pattern
    CHECK (m.song().parts[1].numBars == 0);

    m.undo(); m.undo(); m.undo(); m.undo();
    CHECK (m.song().parts.size() == 2 && m.song().parts[0].sourceId == "user.a" && m.song().parts[0].repeats == 1);
    m.undo();
    CHECK (m.song().parts.empty() && ! m.hasUnsavedChanges());   // back at the saved state
    m.redo();
    CHECK (m.song().parts.size() == 2 && m.hasUnsavedChanges());

    // saving under a new id: the id reaches every snapshot, undo keeps it, content decides "unsaved"
    Song written = m.song();
    written.id = "neu-1234";
    m.adoptSaved (written);
    CHECK (! m.hasUnsavedChanges() && m.song().id == "neu-1234");
    m.undo();
    CHECK (m.song().id == "neu-1234" && m.hasUnsavedChanges());
    m.redo();
    CHECK (! m.hasUnsavedChanges());

    // Ablage: no duplicates, its own undo steps, does not make the song unsaved, saved on every change
    CHECK (m.addToScratch ({ makePatternPart (lib[0]), makePatternPart (lib[0]), makePatternPart (lib[1]) }) == 2);
    CHECK (m.scratch().size() == 2 && ! m.hasUnsavedChanges() && scratchSaves == 1);
    CHECK (m.addToScratch ({ makePatternPart (lib[1]) }) == 0 && scratchSaves == 1);
    m.clearScratch();
    CHECK (m.scratch().empty() && scratchSaves == 2);
    m.undo();   // clearing is undoable
    CHECK (m.scratch().size() == 2 && scratchSaves == 3);

    // a new song keeps the Ablage
    m.load (s);
    CHECK (m.scratch().size() == 2 && m.song().parts.empty());

    // local variant of a block, and updating from the (changed) source
    m.insertPart (0, makePatternPart (lib[0]));
    auto variant = lib[0];
    variant.layers[0].events.clear();   // a different content
    m.setCopy (0, variant, true);
    CHECK (m.song().parts[0].edited && m.song().parts[0].copy->layers[0].events.empty());
    hz::Pattern built;
    std::vector<SongSpan> spans;
    juce::String err;
    CHECK (buildSongPattern (m.song(), lib, built, spans, err));
    CHECK (spans[0].edited && spans[0].sourceDiffers);   // the library still has the original
    CHECK (m.updateFromSource ({ 0 }, lib) == 1);
    CHECK (! m.song().parts[0].edited && samePatternContent (*m.song().parts[0].copy, lib[0]));
    CHECK (buildSongPattern (m.song(), lib, built, spans, err) && ! spans[0].sourceDiffers);
    m.undo();
    CHECK (m.song().parts[0].edited);

    // identity: moving keeps the uid, duplicating gives a new one, dialogs find the block again
    m.insertPart (1, makePatternPart (lib[1]));
    const auto uidA = m.song().parts[0].uid, uidB = m.song().parts[1].uid;
    CHECK (! uidA.empty() && uidA != uidB);
    m.movePart (0, 1);
    CHECK (m.indexOfUid (uidA) == 1 && m.indexOfUid (uidB) == 0);
    m.insertPart (2, m.song().parts[1]);   // duplicate of A
    CHECK (m.song().parts[2].uid != uidA && m.indexOfUid (uidA) == 1);
    m.removePart (1);
    CHECK (m.indexOfUid (uidA) == -1);

    // Ablage: a locally edited copy is a block of its own, but counts as "used" by source
    const auto plain = makePatternPart (lib[0]);
    SongPart changed = plain;
    changed.copy = std::make_shared<const hz::Pattern> (variant);
    CHECK (sameSource (plain, changed) && ! sameBlock (plain, changed));
    CHECK (m.addToScratch ({ plain, changed }) == 1);   // 'plain' is already in the Ablage, the variant is new

    // the flag survives the file
    const auto f = tempFile ("edited.json");
    Song e = m.song();
    e.id = "e";
    CHECK (saveSong (e, f, err));
    Song back;
    CHECK (loadSong (f, back, err) && back.parts.size() == 2 && back.parts[1].edited);   // the duplicate of the edited block
    f.deleteFile();
}

static void testArrangerLimits()
{
    ArrangerModel m;
    int rejected = 0, changes = 0;
    juce::String lastError;
    m.validateSong = [&] (const Song& song) { return songChangeError (m.song(), song, {}); };
    m.onRejected = [&] (const juce::String& error) { ++rejected; lastError = error; };
    m.onChange = [&] { ++changes; };

    Song full;
    full.id = "limit";
    full.name = "Voll";
    SongPart rest;
    rest.kind = PartKind::Rest;
    rest.restBars = 8;
    rest.repeats = 64;
    full.parts = { rest };
    m.load (full);
    hz::Pattern built;
    std::vector<SongSpan> spans;
    juce::String error;
    CHECK (buildSongPattern (m.song(), {}, built, spans, error) && built.bars.size() == 512);
    SongPart extra;
    extra.kind = PartKind::Rest;
    const int beforeChanges = changes;
    m.insertParts (1, { extra, extra });
    CHECK (rejected == 1 && lastError.contains ("512"));
    CHECK (m.song().parts.size() == 1 && ! m.canUndo() && ! m.hasUnsavedChanges());
    CHECK (changes == beforeChanges);
    m.setRepeats (0, 63);
    CHECK (m.song().parts[0].repeats == 63);
    m.insertPart (1, extra);
    CHECK (m.song().parts.size() == 2);
    m.undo();
    CHECK (m.song().parts.size() == 1 && m.canRedo());
    m.setRepeats (0, 64);
    m.undo();
    CHECK (m.canRedo());
    // A rejected insertion must not discard the redo history.
    m.insertParts (1, std::vector<SongPart> (9, extra));
    CHECK (m.canRedo() && m.song().parts.size() == 1);
    m.redo();
    CHECK (m.song().parts[0].repeats == 64);

    // Exact event boundary, then reject an extra pattern, repeat and edited copy.
    auto dense = smallLibrary()[1];
    dense.bars[0].meter = hz::Meter::make (4, 4);
    dense.layers = { { 1, hz::LayerRole::Hands, {} } };
    for (int i = 0; i < 128; ++i)
    {
        hz::PatternEvent event;
        event.id = (std::uint32_t) i + 1;
        event.pos = hz::Rational (i, 32);
        dense.layers[0].events.push_back (event);
    }
    CHECK (dense.validate().empty());
    auto part = makePatternPart (dense);
    part.repeats = 64;
    full.parts = { part };
    m.load (full);
    CHECK (buildSongPattern (m.song(), {}, built, spans, error) && countEvents (built) == 8192);
    m.insertPart (1, makePatternPart (dense));
    CHECK (lastError.contains ("8192") && m.song().parts.size() == 1 && ! m.canUndo());
    auto denser = dense;
    auto event = denser.layers[0].events.back();
    event.id = 129;
    denser.layers[0].events.push_back (event);
    CHECK (! m.setCopy (0, denser, true));
    CHECK (m.song().parts[0].copy->layers[0].events.size() == 128 && ! m.canUndo());
    m.setRepeats (0, 63);
    m.insertPart (1, makePatternPart (dense));
    CHECK (m.song().parts.size() == 2);
    m.setRepeats (0, 64);
    CHECK (m.song().parts[0].repeats == 63 && lastError.contains ("8192"));

    CHECK (m.updateFromSource ({ 0 }, { denser }) == 0);

    // A legacy over-limit song retains its spans and can be repaired by removing parts.
    extra.restBars = 8;
    full.parts = { rest, extra, extra };
    m.load (full);
    CHECK (! buildSongPattern (m.song(), {}, built, spans, error) && ! spans.empty());
    // Reducing repeats is allowed even if the song still exceeds the bar limit.
    m.setRepeats (0, 63);
    CHECK (m.song().parts[0].repeats == 63);
    m.undo();
    CHECK (m.song().parts[0].repeats == 64);
    m.removePart (2);
    CHECK (m.song().parts.size() == 2);
    m.removePart (1);
    CHECK (buildSongPattern (m.song(), {}, built, spans, error));
}

// Badges: every id gets a badge of its own as long as there are free ones, and keeps it
static void testBadgeRegistry()
{
    const auto f = tempFile ("abzeichen.json");
    f.deleteFile();
    std::vector<hz::Pattern> lib;
    for (int i = 0; i < 200; ++i)
    {
        hz::Pattern p;
        p.id = "lib.test-" + std::to_string (i);
        lib.push_back (p);
    }
    BadgeRegistry reg;
    reg.load (f);
    CHECK (reg.ensure (lib));
    auto st = reg.stats (lib);
    CHECK (st.patterns == 200 && st.used == BadgeRegistry::totalBadges());   // all 168 taken, the rest must share
    CHECK (st.shared == 2 * (200 - BadgeRegistry::totalBadges()) || st.shared >= 200 - BadgeRegistry::totalBadges());

    std::vector<hz::Pattern> few (lib.begin(), lib.begin() + 100);
    BadgeRegistry reg2;
    reg2.load (f);
    const auto st2 = reg2.stats (few);
    CHECK (st2.shared == 0 && st2.used == 100);                       // the first 100 never share
    CHECK (reg2.badgeFor ("lib.test-7").colour == reg.badgeFor ("lib.test-7").colour
        && reg2.badgeFor ("lib.test-7").shape == reg.badgeFor ("lib.test-7").shape);   // stable across a reload
    CHECK (! reg2.ensure (few));                                      // nothing new: nothing changes
    f.deleteFile();
}

void runSongTests()
{
    testBadgeRegistry();
    testReadV1();
    testWriteV2RoundTrip();
    testBarRange();
    testArrangerModel();
    testArrangerLimits();
}

// Checks the user's real songs against their patterns (read-only)
void checkSongFolder (const juce::File& songs, const std::vector<hz::Pattern>& library)
{
    int ok = 0, bad = 0;
    for (auto& f : songs.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        Song s;
        juce::String err;
        hz::Pattern built;
        std::vector<SongSpan> spans;
        if (loadSong (f, s, err) && buildSongPattern (s, library, built, spans, err))
            ++ok;
        else
        {
            std::printf ("  FEHLER %s\n", err.toRawUTF8());
            ++bad;
            continue;
        }

        // The arranger's "unsaved" compares content keys: loading a file twice, embedding copies, and a
        // write/read cycle must all give the same key, or a saved song looks unsaved after every start.
        Song again;
        CHECK (loadSong (f, again, err) && embedCopies (again, library, err));
        Song withCopies = s;
        CHECK (embedCopies (withCopies, library, err));
        Song reread;
        const auto tmp = tempFile ("roundtrip.json");
        CHECK (saveSong (withCopies, tmp, err) && loadSong (tmp, reread, err));
        tmp.deleteFile();
        const auto key = ArrangerModel::contentKey (withCopies);
        const juce::String keys[] = { ArrangerModel::contentKey (again), ArrangerModel::contentKey (reread) };
        const char* names[] = { "zweites Laden", "Schreiben und Lesen" };
        for (int i = 0; i < 2; ++i)
            if (keys[i] != key)
            {
                // show where the keys start to differ
                int at = 0;
                while (at < key.length() && at < keys[i].length() && key[at] == keys[i][at])
                    ++at;
                const int from = juce::jmax (0, at - 60);
                std::printf ("  UNGLEICH %s nach %s, Zeichen %d:\n    %s\n    %s\n", f.getFileName().toRawUTF8(), names[i], at,
                             key.substring (from, at + 80).toRawUTF8(), keys[i].substring (from, at + 80).toRawUTF8());
                ++bad;
            }
    }
    std::printf ("%d Songs geladen, %d fehlerhaft\n", ok, bad);
    CHECK (bad == 0);

    // Informational: does the arranger's saved working state match its song file? (what the
    // arranger decides at startup; read-only, nothing is written)
    const auto state = songs.getSiblingFile ("arranger.json");
    Song work;
    juce::String err;
    if (state.existsAsFile() && loadSong (state, work, err))
    {
        embedCopies (work, library, err);
        const auto file = songs.getChildFile (juce::String (work.id) + ".json");
        Song onDisk;
        if (work.id.empty() || ! file.existsAsFile() || ! loadSong (file, onDisk, err))
            std::printf ("Arbeitsstand \"%s\": keine gespeicherte Datei (%s)\n", work.name.c_str(), file.getFileName().toRawUTF8());
        else
        {
            embedCopies (onDisk, library, err);
            const auto a = ArrangerModel::contentKey (work), b = ArrangerModel::contentKey (onDisk);
            int at = 0;
            while (at < a.length() && at < b.length() && a[at] == b[at])
                ++at;
            if (a == b)
                std::printf ("Arbeitsstand \"%s\": entspricht %s\n", work.name.c_str(), file.getFileName().toRawUTF8());
            else
                std::printf ("Arbeitsstand \"%s\": weicht von %s ab, Zeichen %d:\n    %s\n    %s\n", work.name.c_str(), file.getFileName().toRawUTF8(),
                             at, a.substring (juce::jmax (0, at - 60), at + 80).toRawUTF8(), b.substring (juce::jmax (0, at - 60), at + 80).toRawUTF8());
        }
    }
}
