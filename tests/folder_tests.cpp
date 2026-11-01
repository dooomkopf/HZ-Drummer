// Tests für die Ordnerstruktur (System → Ordnerstruktur): Standardplätze, gespeicherte Plätze,
// Ordner anlegen/umbenennen/löschen, Verschieben, Reihenfolge, Datei lesen und schreiben
#include "app/FolderTree.h"
#include "app/BrowseOrder.h"
#include "app/FolderSort.h"
#include "app/MenuFolderSlots.h"
#include "app/AlbumView.h"

#include <cstdio>
#include <map>

extern int folderTestFailures;
int folderTestFailures = 0;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++folderTestFailures; } } while (0)

namespace
{
    FolderPath path (std::initializer_list<const char*> names)
    {
        FolderPath p;
        for (auto* n : names)
            p.add (juce::String::fromUTF8 (n));
        return p;
    }

    // drei eigene Patterns zweier Bands, eine davon klein geschrieben
    std::vector<FolderTree::Entry> patterns()
    {
        return { { "user.b1", path ({ "Exhorder", "Desecrator" }) },
                 { "user.a1", path ({ "Carcass", "CJQ" }) },
                 { "user.a2", path ({ "carcass", "ISA" }) } };
    }

    juce::File tempFile (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hz-folder-test-" + name);
    }
}

// Ohne gespeicherte Ordner: Standardplätze, Ordner alphabetisch, gleiche Namen verschmelzen
static void testDefaultPlaces()
{
    FolderTree tree;
    const auto root = tree.resolve (FolderTree::Patterns, patterns());
    CHECK (root.folders.size() == 2);
    CHECK (root.folders[0].name == "Carcass" && root.folders[1].name == "Exhorder");
    CHECK (root.folders[0].folders.size() == 2);   // CJQ und ISA im selben Band-Ordner
    CHECK (folders::countItems (root) == 3);

    FolderPath at;
    CHECK (folders::locate (root, "user.a2", at) && at == path ({ "Carcass", "ISA" }));

    // Songs ohne Standardpfad: oberste Ebene, in der gegebenen Reihenfolge
    const auto songs = tree.resolve (FolderTree::Songs, { { "song.z", {} }, { "song.a", {} } });
    CHECK (songs.folders.empty() && songs.items.size() == 2 && songs.items[0] == "song.z");
}

// Gespeicherte Plätze gewinnen, verschwundene Einträge fallen weg, neue landen am Standardplatz
static void testStoredPlaces()
{
    FolderTree tree;
    auto root = tree.resolve (FolderTree::Patterns, patterns());
    CHECK (folders::addFolder (root, {}, "Thrash").isEmpty());
    CHECK (folders::moveItem (root, "user.b1", path ({ "Thrash" })).isEmpty());
    root.items.push_back ("user.weg");   // ein gelöschtes Pattern
    tree.set (FolderTree::Patterns, root);

    auto entries = patterns();
    entries.push_back ({ "user.a3", path ({ "Carcass", "Neu" }) });
    const auto resolved = tree.resolve (FolderTree::Patterns, entries);
    FolderPath at;
    CHECK (folders::locate (resolved, "user.b1", at) && at == path ({ "Thrash" }));
    at.clear();
    CHECK (folders::locate (resolved, "user.a3", at) && at == path ({ "Carcass", "Neu" }));
    CHECK (! folders::locate (resolved, "user.weg", at));
    CHECK (folders::countItems (resolved) == 4);
}

// Ordner anlegen, umbenennen, löschen: Namen ohne Groß/Klein eindeutig, nur leere löschen
static void testFolderEdits()
{
    FolderTree tree;
    auto root = tree.resolve (FolderTree::Patterns, patterns());
    CHECK (folders::addFolder (root, {}, "  ").isNotEmpty());
    CHECK (folders::addFolder (root, {}, "CARCASS").isNotEmpty());
    CHECK (folders::addFolder (root, path ({ "Nirgends" }), "X").isNotEmpty());
    CHECK (folders::addFolder (root, {}, "Leer").isEmpty());
    CHECK (folders::addFolder (root, path ({ "Leer" }), "Unten").isEmpty());

    CHECK (folders::renameFolder (root, path ({ "Leer" }), "Leer").isEmpty());   // eigener Name bleibt erlaubt
    CHECK (folders::renameFolder (root, path ({ "Leer" }), "Ablage").isEmpty());
    CHECK (folders::find (root, path ({ "Ablage", "Unten" })) != nullptr);

    CHECK (folders::removeFolder (root, path ({ "Carcass" })).isNotEmpty());   // nicht leer
    CHECK (folders::removeFolder (root, path ({ "Ablage" })).isEmpty());       // samt leerem Unterordner
    CHECK (folders::find (root, path ({ "Ablage" })) == nullptr);
    CHECK (folders::countItems (root) == 3);
}

// Umbenennen in einen vorhandenen Namen legt beide Ordner zusammen, in der neuen Schreibweise
static void testRenameMerges()
{
    FolderTree tree;
    auto root = tree.resolve (FolderTree::Patterns, patterns());   // Carcass (CJQ, ISA), Exhorder (Desecrator)
    CHECK (folders::addFolder (root, {}, "techdeath").isEmpty());
    CHECK (folders::addFolder (root, path ({ "techdeath" }), "CJQ").isEmpty());
    CHECK (folders::moveItem (root, "user.b1", path ({ "techdeath" })).isEmpty());
    CHECK (folders::renameFolder (root, path ({ "techdeath" }), "CARCASS").isEmpty());
    CHECK (root.folders.size() == 2 && root.folders[0].name == "CARCASS");
    CHECK (root.folders[0].folders.size() == 2);   // die beiden CJQ sind einer
    FolderPath at;
    CHECK (folders::locate (root, "user.b1", at) && at == path ({ "CARCASS" }));
    CHECK (folders::countItems (root) == 3);

    // nur die Schreibweise ändern
    CHECK (folders::renameFolder (root, path ({ "carcass" }), "Carcass").isEmpty());
    CHECK (root.folders[0].name == "Carcass" && root.folders.size() == 2);
    CHECK (folders::renameFolder (root, path ({ "Carcass" }), " ").isNotEmpty());
}

// Ordner verschieben: nicht in sich selbst, keine Namensgleichheit am Ziel, Reihenfolge per Index
static void testMoveFolders()
{
    FolderTree tree;
    auto root = tree.resolve (FolderTree::Patterns, patterns());
    CHECK (folders::addFolder (root, {}, "Metal").isEmpty());
    CHECK (folders::moveFolder (root, path ({ "Carcass" }), path ({ "Carcass", "CJQ" })).isNotEmpty());
    CHECK (folders::moveFolder (root, path ({ "Carcass" }), path ({ "Metal" })).isEmpty());
    CHECK (folders::find (root, path ({ "Metal", "Carcass", "ISA" })) != nullptr);
    CHECK (root.folders.size() == 2);

    CHECK (folders::addFolder (root, {}, "carcass").isEmpty());
    CHECK (folders::moveFolder (root, path ({ "carcass" }), path ({ "Metal" })).isNotEmpty());

    // oberste Ebene: Exhorder, Metal, carcass -> carcass nach vorn
    CHECK (folders::moveFolder (root, path ({ "carcass" }), {}, 0).isEmpty());
    CHECK (root.folders[0].name == "carcass" && root.folders[1].name == "Exhorder");
    // Index zählt in der Liste vor dem Verschieben: ans Ende hinter Metal
    CHECK (folders::moveFolder (root, path ({ "carcass" }), {}, 3).isEmpty());
    CHECK (root.folders[2].name == "carcass");

    CHECK (folders::shiftFolder (root, path ({ "Metal" }), -1));
    CHECK (root.folders[0].name == "Metal");
    CHECK (! folders::shiftFolder (root, path ({ "Metal" }), -1));
    CHECK (folders::countItems (root) == 3);
}

// Einträge verschieben und sortieren
static void testMoveItems()
{
    FolderTree tree;
    auto root = tree.resolve (FolderTree::Songs, { { "song.a", {} }, { "song.b", {} }, { "song.c", {} } });
    CHECK (folders::addFolder (root, {}, "Thrash").isEmpty());
    CHECK (folders::moveItem (root, "song.b", path ({ "Thrash" })).isEmpty());
    CHECK (folders::moveItem (root, "song.x", path ({ "Thrash" })).isNotEmpty());
    CHECK (folders::moveItem (root, "song.a", path ({ "Fehlt" })).isNotEmpty());
    CHECK (root.items.size() == 2 && root.folders[0].items.size() == 1);

    // song.c vor song.a, dann wieder zurück ans Ende
    CHECK (folders::moveItem (root, "song.c", {}, 0).isEmpty());
    CHECK (root.items[0] == "song.c");
    CHECK (folders::moveItem (root, "song.c", {}, 2).isEmpty());
    CHECK (root.items[1] == "song.c");
    CHECK (folders::shiftItem (root, "song.c", -1) && root.items[0] == "song.c");
    CHECK (! folders::shiftItem (root, "song.c", -1));

    CHECK (folders::moveItem (root, "song.b", {}, 1).isEmpty());   // aus dem Ordner an Position 1
    CHECK (root.items.size() == 3 && root.items[1] == "song.b" && root.folders[0].items.empty());
}

// Datei: Hin und zurück; kaputte Datei wird nicht überschrieben
static void testFolderFile()
{
    const auto file = tempFile ("ordner.json");
    FolderTree tree;
    CHECK (tree.load (file.getNonexistentSibling()).isEmpty());   // keine Datei: keine Ordner

    auto root = tree.resolve (FolderTree::Patterns, patterns());
    CHECK (folders::addFolder (root, path ({ "Carcass" }), juce::String::fromUTF8 ("Übungen")).isEmpty());
    tree.set (FolderTree::Patterns, root);

    // Bibliothek: Stil-Ordner "techdeath" geht durch Umbenennen in "Tech-Death" auf
    const std::vector<FolderTree::Entry> bundled { { "lib.a", path ({ "Tech-Death" }) }, { "lib.b", path ({ "techdeath" }) } };
    auto library = tree.resolve (FolderTree::Bundled, bundled);
    CHECK (library.folders.size() == 2);
    CHECK (folders::renameFolder (library, path ({ "techdeath" }), "Tech-Death").isEmpty());
    CHECK (library.folders.size() == 1 && library.folders[0].name == "Tech-Death" && library.folders[0].items.size() == 2);
    tree.set (FolderTree::Bundled, library);
    tree.setHidden (FolderTree::Patterns, "Carcass", true);   // System → Menü-Ordner: travels with the file
    CHECK (tree.save (file).isEmpty());

    FolderTree loaded;
    CHECK (loaded.load (file).isEmpty());
    CHECK (loaded.isHidden (FolderTree::Patterns, "carcass") && ! loaded.isHidden (FolderTree::Patterns, "Exhorder"));
    loaded.setHidden (FolderTree::Patterns, "CARCASS", false);
    CHECK (! loaded.isHidden (FolderTree::Patterns, "Carcass"));

    // renamed: still hidden under the new name; a later folder with the old name is not; gone: forgotten
    loaded.setHidden (FolderTree::Patterns, "Carcass", true);
    loaded.followTopLevel (FolderTree::Patterns, { "Carcass", "Exhorder" }, { "Carcass Live", "Exhorder" });
    CHECK (loaded.isHidden (FolderTree::Patterns, "Carcass Live") && ! loaded.isHidden (FolderTree::Patterns, "Carcass"));
    loaded.followTopLevel (FolderTree::Patterns, { "Carcass Live", "Exhorder" }, { "Exhorder" });   // merged or removed
    CHECK (! loaded.isHidden (FolderTree::Patterns, "Carcass Live"));
    loaded.setHidden (FolderTree::Patterns, "Exhorder", true);
    loaded.followTopLevel (FolderTree::Patterns, { "Exhorder" }, { "Exhorder", "Neu" });   // a new folder: nothing else changes
    CHECK (loaded.isHidden (FolderTree::Patterns, "Exhorder") && ! loaded.isHidden (FolderTree::Patterns, "Neu"));
    const auto back = loaded.resolve (FolderTree::Patterns, patterns());
    CHECK (folders::find (back, path ({ "Carcass", "Übungen" })) != nullptr);
    CHECK (loaded.resolve (FolderTree::Bundled, bundled).folders.size() == 1);   // bleibt zusammengelegt
    CHECK (back.folders.size() == 2 && back.folders[1].name == "Exhorder");

    CHECK (file.replaceWithText ("{ kaputt"));
    FolderTree broken;
    CHECK (broken.load (file).isNotEmpty());
    CHECK (broken.save (file).isNotEmpty());
    CHECK (file.loadFileAsString() == "{ kaputt");
    file.deleteFile();
}

// Kurzname: was die Ordner schon sagen, entfällt
static void testShortName()
{
    const auto name = juce::String::fromUTF8 ("Carcass – CJQ – 01 Intro");
    CHECK (folders::shortName (name, path ({ "Carcass", "CJQ" })) == "01 Intro");
    CHECK (folders::shortName (name, path ({ "Metal", "Carcass" })) == juce::String::fromUTF8 ("CJQ – 01 Intro"));
    CHECK (folders::shortName (name, path ({ "Thrash" })) == name);
    CHECK (folders::shortName (name, {}) == name);
    // deeper trees: every folder of the path may go, in any order
    const auto deep = juce::String::fromUTF8 ("R.P. Bardet – Rock – 4 Takte – Pattern 1 + Break 1");
    CHECK (folders::shortName (deep, path ({ "R.P. Bardet", "Rock, Pop", "Rock", "4 Takte" })) == "Pattern 1 + Break 1");
    CHECK (folders::shortName (deep, path ({ "R.P. Bardet", "Rock, Pop" })) == juce::String::fromUTF8 ("Rock – 4 Takte – Pattern 1 + Break 1"));
}

// Blättern wie im Menü: Halte sind Ordner mit eigenen Einträgen, Unterordner vor den eigenen Einträgen,
// reine Sortier-Ordner führen in den ersten Unterordner, ausgeblendete werden übersprungen, nichts läuft im Kreis
static void testBrowseOrder()
{
    FolderNode root;
    root.items = { "user.oben" };
    FolderNode a { "A", {}, { "user.a1", "user.a2" } };
    FolderNode hidden { "Versteckt", {}, { "user.h1" } };
    FolderNode leer { "Leer", {}, {} };
    FolderNode fusion { "Fusion", {}, { "user.f1", "user.f2", "user.f3" } };
    FolderNode jazz { "Jazz", { fusion }, {} };   // only sorts
    FolderNode groove { "Groove", { jazz }, { "user.g1" } };
    root.folders = { a, hidden, leer, groove };
    FolderNode library;
    library.folders = { FolderNode { "Thrash", {}, { "lib.t1" } } };

    const auto order = browseFolders ({ { root, [] (const juce::String& f) { return f == "Versteckt"; } },
                                        { library, nullptr } });
    CHECK (order.size() == 6);   // A, Versteckt, Groove > Jazz > Fusion, Groove, oben, Thrash
    CHECK (order[0].path == path ({ "A" }) && order[0].ids.size() == 2 && ! order[0].hidden);
    CHECK (order[1].hidden);
    CHECK (order[2].path == path ({ "Groove", "Jazz", "Fusion" }) && order[2].ids.size() == 3);
    CHECK (order[3].path == path ({ "Groove" }) && order[3].ids.front() == "user.g1");
    CHECK (order[4].path.isEmpty() && order[4].ids.front() == "user.oben");
    CHECK (order[5].area == 1 && order[5].ids.front() == "lib.t1");

    CHECK (browseFolderOf (order, "user.f2") == 2 && browseFolderOf (order, "user.x") < 0);
    CHECK (nextBrowseFolder (order, 0, 1) == 2);        // the hidden one is passed over
    CHECK (nextBrowseFolder (order, 2, -1) == 0);
    CHECK (nextBrowseFolder (order, 5, 1) == -1);       // no wrapping
    CHECK (nextBrowseFolder (order, 0, -1) == -1);
    CHECK (nextBrowseFolder (order, -1, 1) == 0);       // from outside: the first and the last one
    CHECK (nextBrowseFolder (order, (int) order.size(), -1) == 5);
}

// Sortieren: erst Ordner, dann Einträge, je alphabetisch; Zahlen nach Wert, Umlaute wie Grundbuchstabe,
// Zeichen vorn zählen nicht; ohne "mit Unterordnern" bleiben die Unterordner, wie sie sind
static void testSortFolder()
{
    FolderNode root;
    for (auto* n : { "Teil 10", "b", "Teil 2", "\xc3\x84rger", "A" })
        root.folders.push_back ({ juce::String::fromUTF8 (n), {}, {} });
    root.folders[1].items = { "z", "y" };   // in "b"
    root.items = { "one", "justice", "Battery", "battery" };
    const std::map<std::string, juce::String> names { { "one", "One" }, { "justice", "...And Justice for All" }, { "Battery", "Battery" },
                                                      { "battery", "battery" }, { "z", "Zebra" }, { "y", "Yak" } };
    const auto label = [&names] (const std::string& id, const FolderPath&) { return names.at (id); };

    CHECK (folders::sortFolder (root, {}, false, label).isEmpty());
    juce::StringArray order;
    for (auto& f : root.folders)
        order.add (f.name);
    CHECK (order.joinIntoString ("|") == juce::String::fromUTF8 ("A|\xc3\x84rger|b|Teil 2|Teil 10"));
    CHECK ((root.items == std::vector<std::string> { "justice", "Battery", "battery", "one" }));
    CHECK ((root.folders[2].items == std::vector<std::string> { "z", "y" }));   // ohne Unterordner: unverändert
    CHECK (folders::sortFolder (root, {}, true, label).isEmpty());
    CHECK ((root.folders[2].items == std::vector<std::string> { "y", "z" }));
    CHECK (folders::sortFolder (root, path ({ "fehlt" }), false, label).isNotEmpty());
}

// Menü-Ordner-Plätze: ohne Datei 8 und 9 vorbelegt, 1 und 10 fest; "alle" nimmt später importierte Ordner mit,
// eine Liste nicht; Speichern und Laden geben dasselbe zurück, ein geleerter vorbelegter Platz bleibt leer
static void testMenuFolderSlots()
{
    const auto file = tempFile ("menue-ordner.json");
    file.deleteFile();
    auto slots = menufolderslots::load (file);
    CHECK (slots[7].name == "Nur Bausteine" && slots[8].name == "Nur Arrangements");
    CHECK (slots[0].name == "Minimal" && slots[9].name == "Maximal" && slots[1].isFree());

    AreaNames folders;
    folders[FolderTree::Songs] = juce::StringArray ({ "Beispiele", "Metallica" });
    folders[FolderTree::Patterns] = juce::StringArray ({ "Metallica", "Death" });
    folders[FolderTree::Bundled] = juce::StringArray ({ "Thrash", "Jazz / Fusion" });
    auto hide = menufolderslots::hiddenBy (slots[0], folders);   // Minimal
    CHECK (hide[FolderTree::Songs] == juce::StringArray ({ "Metallica" }) && hide[FolderTree::Patterns].size() == 2
           && hide[FolderTree::Bundled].isEmpty());
    hide = menufolderslots::hiddenBy (slots[7], folders);   // Nur Bausteine
    CHECK (hide[FolderTree::Songs].size() == 2 && hide[FolderTree::Patterns].isEmpty() && hide[FolderTree::Bundled].isEmpty());
    CHECK (menufolderslots::hiddenBy (slots[9], folders)[FolderTree::Songs].isEmpty());   // Maximal

    AreaNames hidden;
    hidden[FolderTree::Songs] = juce::StringArray ({ "Beispiele" });
    hidden[FolderTree::Bundled] = juce::StringArray ({ "Thrash", "Jazz / Fusion" });
    const auto metal = menufolderslots::capture ("Metallica", folders, hidden);
    CHECK (! metal.all[FolderTree::Songs] && metal.shown[FolderTree::Songs] == juce::StringArray ({ "Metallica" }));
    CHECK (metal.all[FolderTree::Patterns] && ! metal.all[FolderTree::Bundled] && metal.shown[FolderTree::Bundled].isEmpty());
    folders[FolderTree::Songs].add ("Death");      // später importiert
    folders[FolderTree::Patterns].add ("Carcass");
    hide = menufolderslots::hiddenBy (metal, folders);
    CHECK (hide[FolderTree::Songs].contains ("Death") && ! hide[FolderTree::Songs].contains ("Metallica") && hide[FolderTree::Patterns].isEmpty());

    slots[2] = metal;
    slots[7] = {};   // vorbelegt, aber löschbar
    CHECK (menufolderslots::save (slots, file));
    const auto back = menufolderslots::load (file);
    CHECK (back[2].name == "Metallica" && back[2].shown[FolderTree::Songs] == juce::StringArray ({ "Metallica" })
           && back[2].all[FolderTree::Patterns] && ! back[2].all[FolderTree::Bundled]);
    CHECK (back[7].isFree() && back[8].name == "Nur Arrangements" && back[0].name == "Minimal" && back[9].name == "Maximal");
    file.deleteFile();

    CHECK (menufolderslots::save (back, file));   // Umbenennen in der Ordnerstruktur: die Liste zieht mit
    menufolderslots::followRename (FolderTree::Songs, "metallica", "Metallica (Live)", false, file);
    const auto renamed = menufolderslots::load (file);
    CHECK (renamed[2].shown[FolderTree::Songs] == juce::StringArray ({ "Metallica (Live)" }));
    menufolderslots::followRename (FolderTree::Songs, "Metallica (Live)", "Thrash", true, file);   // the name stays on top in the album view
    CHECK (menufolderslots::load (file)[2].shown[FolderTree::Songs] == juce::StringArray ({ "Metallica (Live)", "Thrash" }));
    file.deleteFile();

    FolderTree tree;   // ein Platz setzt die Ausblendliste ganz: ein gerade verschwundener Ordner bleibt nicht ausgeblendet
    tree.setHidden (FolderTree::Patterns, "Weg", true);
    tree.setHiddenFolders (FolderTree::Patterns, juce::StringArray ({ "Death" }));
    CHECK (! tree.isHidden (FolderTree::Patterns, "Weg") && tree.isHidden (FolderTree::Patterns, "death"));
}

// Album-Ansicht: alben.json lesen (Titelliste = Nummern, einzelne Nummern, live), Schlüssel ohne Zeichen,
// Baum Band › "jjjj - Album" › "nn. Song", Sonstige zuletzt, Band ohne Album flach, ohne Band oben;
// Kurzname streicht auch die dekorierten Ordnernamen; die Ausblendliste behält Namen der anderen Ansicht
static void testAlbumView()
{
    const auto u = [] (const char* s) { return juce::String::fromUTF8 (s); };
    const auto file = tempFile ("alben.json");
    file.replaceWithText (u(R"json({"formatVersion":1,"bands":{"Metallica":[
        {"album":"Ride the Lightning","jahr":1984,"titel":["Fight Fire with Fire","Ride the Lightning"]},
        {"album":"Kill 'Em All","jahr":1983,"titel":["Hit the Lights","The Four Horsemen"]},
        {"album":"Live Evil","jahr":1982,"live":true,"nummern":{"Paranoid (Live Evil)":7}}]}})json"));
    AlbumIndex index;
    CHECK (index.load (file, juce::File()).isEmpty());
    file.deleteFile();
    CHECK (AlbumIndex::key ("Damage, Inc.") == "damageinc" && AlbumIndex::key ("1,000 Eyes") == AlbumIndex::key ("1000 Eyes"));
    CHECK (AlbumIndex::key ("Seek & Destroy") == "seekanddestroy" && AlbumIndex::key (u("Übung ß")) == "ubungss");
    CHECK (AlbumIndex::key (u("Ärger Öl")) == "argerol" && AlbumIndex::key ("...And Justice for All") == "andjusticeforall");
    CHECK (index.hasAlbums ("metallica") && ! index.hasAlbums ("Death"));
    const auto four = index.find ("Metallica", "the four horsemen");
    CHECK (four && four->album == "Kill 'Em All" && four->year == 1983 && four->number == 2 && ! four->live);
    const auto live = index.find ("Metallica", "Paranoid (Live Evil)");
    CHECK (live && live->live && live->number == 7 && live->year == 1982);

    const std::vector<albumview::Entry> entries {
        { "song.rtl", u("Metallica – Ride the Lightning (Song)"), "Metallica", "Ride the Lightning", true },
        { "song.four", u("Metallica – The Four Horsemen (Song)"), "Metallica", "The Four Horsemen", true },
        { "user.hit", u("Metallica – Hit the Lights – 01 Anfang"), "Metallica", "Hit the Lights", false },
        { "user.one", u("Metallica – One – 01 Anfang"), "Metallica", "One", false },
        { "song.mix", u("Metallica – Medley (Song)"), "Metallica", "", true },
        { "user.death", u("Death – Zombie Ritual – 01"), "Death", "Zombie Ritual", false },
        { "user.frei", "Eigener Baustein", "", "", false },
        { "song.lib", "Beispiel (Song)", "", "", true } };
    const auto root = albumview::build (entries, index);
    CHECK ((root.items == std::vector<std::string> { "song.lib", "user.frei" }));   // without a band: on top, by name
    CHECK (root.folders.size() == 2 && root.folders[0].name == "Death" && root.folders[1].name == "Metallica");
    const auto& m = root.folders[1];
    CHECK (m.folders.size() == 3 && m.folders[0].name == "1983 - Kill 'Em All" && m.folders[1].name == "1984 - Ride the Lightning"
           && m.folders[2].name == "Sonstige");
    CHECK (m.folders[0].folders.size() == 1 && m.folders[0].folders[0].name == "01. Hit the Lights");
    CHECK ((m.folders[0].items == std::vector<std::string> { "song.four" }) && (m.folders[1].items == std::vector<std::string> { "song.rtl" }));
    CHECK (m.folders[2].folders.size() == 1 && m.folders[2].folders[0].name == "One" && (m.folders[2].items == std::vector<std::string> { "song.mix" }));
    CHECK (root.folders[0].folders.size() == 1 && root.folders[0].folders[0].name == "Zombie Ritual");   // no album: flat
    CHECK (albumview::topLevel (entries) == juce::StringArray ({ "Metallica", "Death" }));

    // an album without a year after those with one, before "Sonstige"; a song without a number after the numbered;
    // two entries of one album are one folder, so are "drei" and "Drei"; "???" names no song
    const auto years = tempFile ("alben-jahre.json");
    years.replaceWithText (u(R"json({"bands":{"!!!":[{"album":"Laut","jahr":2000,"titel":["Lied"]}],"Band":[{"album":"Ohne Jahr","titel":["Eins","???"]},
        {"album":"Kill ’Em All","jahr":1983,"titel":["Zwei","Drei"]},
        {"album":"Kill ’Em All","jahr":1983,"nummern":{"Vier":0,"Fünf":5}}]}})json"));
    AlbumIndex yearIndex;
    CHECK (yearIndex.load (years, juce::File()).isEmpty());
    years.deleteFile();
    const auto eins = yearIndex.find ("Band", "Eins");
    CHECK (! yearIndex.find ("Band", "???") && ! yearIndex.find ("Band", "") && eins && eins->year == 0 && eins->number == 1);
    CHECK (! yearIndex.hasAlbums ("!!!") && ! yearIndex.find ("???", "Lied") && yearIndex.bandCount() == 1);   // "!!!" names nothing
    const std::vector<albumview::Entry> some {
        { "user.vier", u("Band – Vier – 01"), "Band", "Vier", false },
        { "user.drei", u("Band – Drei – 01"), "Band", "drei", false },
        { "user.eins", u("Band – Eins – 01"), "Band", "Eins", false },
        { "user.fuenf", u("Band – Fünf – 01"), "Band", u("Fünf"), false },
        { "user.drei2", u("Band – Drei – 02"), "Band", "Drei", false },
        { "user.drei3", u("Band – Drei! – 01"), "Band", "Drei!", false },
        { "user.rest", u("Band – Rest – 01"), "Band", "Rest", false } };
    const auto byYear = albumview::build (some, yearIndex);
    CHECK (byYear.folders.size() == 1 && byYear.folders[0].folders.size() == 3);
    const auto& albums = byYear.folders[0].folders;
    CHECK (albums[0].name == u("1983 - Kill ’Em All") && albums[1].name == "Ohne Jahr" && albums[2].name == "Sonstige");
    CHECK (albums[0].folders.size() == 3 && albums[0].folders[0].name == "02. drei" && albums[0].folders[1].name == u("05. Fünf")
           && albums[0].folders[2].name == "Vier");                                              // "drei" and "Drei": one folder
    CHECK (albums[0].folders[0].items.size() == 3);                                       // "Drei!" too: one song, one folder
    CHECK (AlbumIndex::key (u("Queensrÿche")) == "queensryche" && AlbumIndex::key (u("São Paulo")) == "saopaulo");

    CHECK (folders::shortName (u("Metallica – Hit the Lights – 01 Anfang"), path ({ "Metallica", "1983 - Kill 'Em All", "01. Hit the Lights" })) == "01 Anfang");
    CHECK (folders::shortName (u("1983... (A Merman) – Teil"), path ({ "Hendrix", "1983... (A Merman)" })) == "Teil");   // the whole folder name
    CHECK (folders::shortName (u("Merman – Teil"), path ({ "1983... (A Merman)" })) == u("Merman – Teil"));           // "1983..." is no decoration

    // an import's albums: only bands neither list knows go into the own file, nothing is overwritten
    const auto staging = tempFile ("staging-alben.json"), own = tempFile ("eigene-alben.json");
    own.deleteFile();
    staging.replaceWithText (u(R"json({"bands":{"Metallica":[{"album":"Load","jahr":1996,"titel":["Ain't My Bitch"]}],
        "Exhorder":[{"album":"Slaughter in the Vatican","jahr":1990,"titel":["Death in Vain","Homicide","Desecrator"]}],"Kaputt":"x"}})json"));
    int added = -1;
    CHECK (index.mergeNewBands (staging, own, added).isEmpty() && added == 1);
    AlbumIndex merged;
    CHECK (merged.load (juce::File(), own).isEmpty() && merged.hasAlbums ("exhorder") && ! merged.hasAlbums ("Kaputt"));
    CHECK (merged.find ("Exhorder", "Desecrator")->number == 3 && ! merged.hasAlbums ("Metallica"));   // Metallica: the shipped list knows it
    CHECK (index.mergeNewBands (staging, own, added).isEmpty() && added == 0);                          // a second time: nothing new
    own.replaceWithText ("{\"formatVersion\":1,\"bands\":[\"kaputt\"]}");                                 // a damaged own file: left alone
    CHECK (index.mergeNewBands (staging, own, added).isNotEmpty() && added == 0 && own.loadFileAsString().contains ("kaputt"));
    staging.deleteFile();
    own.deleteFile();

    // within one file the first album that names a song keeps it (the earliest, as the tool sorts); the own file overrides
    const auto shipped2 = tempFile ("alben-shipped.json"), own2 = tempFile ("alben-own.json");
    shipped2.replaceWithText (u(R"json({"bands":{"Band":[{"album":"Erstes","jahr":1970,"titel":["Lied"]},{"album":"Spaeter","jahr":1980,"titel":["Lied","Neu"]}]}})json"));
    own2.replaceWithText (u(R"json({"bands":{"band":[{"album":"Eigenes","jahr":1999,"titel":["neu"]}]}})json"));
    AlbumIndex layered;
    CHECK (layered.load (shipped2, own2).isEmpty());
    CHECK (layered.find ("Band", "Lied")->album == "Erstes" && layered.find ("Band", "Neu")->album == "Eigenes");
    shipped2.deleteFile();
    own2.deleteFile();

    using V = albumview::Vote;
    CHECK (albumview::vote ({ V { "Metallica", "One" }, V { "Metallica", "One" }, V { "Metallica", "Battery" } }) == V ("Metallica", "One"));
    CHECK (albumview::vote ({ V { "Metallica", "One" }, V { "metallica", "Battery" } }) == V ("Metallica", ""));     // one band, no majority
    CHECK (! albumview::vote ({ V { "Metallica", "One" }, V { "Death", "Spirit Crusher" } }));                       // two bands: nothing
    CHECK (albumview::vote ({ V { "Metallica", "Seek & Destroy" }, V { "metallica", "Seek and Destroy" } })
           == V ("Metallica", "Seek & Destroy"));                                                                     // one song, two spellings
    CHECK (! albumview::vote ({}));

    FolderTree tree;
    tree.setHidden (FolderTree::Songs, "Carcass", true);
    tree.followTopLevel (FolderTree::Songs, juce::StringArray ({ "Carcass" }), juce::StringArray ({ "Carcass Live" }), juce::StringArray ({ "Carcass" }));
    CHECK (tree.isHidden (FolderTree::Songs, "Carcass") && tree.isHidden (FolderTree::Songs, "Carcass Live"));
    tree.followTopLevel (FolderTree::Songs, juce::StringArray ({ "Carcass Live" }), juce::StringArray ({ "Carcass" }), juce::StringArray ({ "Carcass" }));
    const auto treeFile = tempFile ("ordner-zurueck.json");   // renamed back: the name once in the list, not twice
    CHECK (tree.save (treeFile).isEmpty());
    const auto saved = juce::JSON::parse (treeFile);
    CHECK (saved["hidden"]["songs"].size() == 1 && saved["hidden"]["songs"][0].toString() == "Carcass");
    treeFile.deleteFile();
}

void runFolderTests()
{
    testAlbumView();
    testSortFolder();
    testMenuFolderSlots();
    testBrowseOrder();
    testDefaultPlaces();
    testStoredPlaces();
    testFolderEdits();
    testRenameMerges();
    testMoveFolders();
    testMoveItems();
    testFolderFile();
    testShortName();
}
