// Tests für die Ordnerstruktur (System → Ordnerstruktur): Standardplätze, gespeicherte Plätze,
// Ordner anlegen/umbenennen/löschen, Verschieben, Reihenfolge, Datei lesen und schreiben
#include "app/FolderTree.h"

#include <cstdio>

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
    CHECK (tree.save (file).isEmpty());

    FolderTree loaded;
    CHECK (loaded.load (file).isEmpty());
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

void runFolderTests()
{
    testDefaultPlaces();
    testStoredPlaces();
    testFolderEdits();
    testRenameMerges();
    testMoveFolders();
    testMoveItems();
    testFolderFile();
    testShortName();
}
