// Tests für die Ansicht Band › Album › Song: alben.json (AlbumIndex), der Baum (AlbumView), Stimmen der
// Arrangement-Teile, Ergänzen der eigenen alben.json; aus folder_tests.cpp ausgelagert (Zeilengrenze), zählt mit dort
#include "app/AlbumIndex.h"
#include "app/AlbumView.h"
#include "app/FolderTree.h"

#include <cstdio>

extern int folderTestFailures;

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

    juce::File tempFile (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hz-album-test-" + name);
    }
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

    // a known band: an imported song without an album gets its own, alone; an album the list knows keeps its name and
    // year, off its track list the number is 0; what has a place does not move
    const auto songsIn = tempFile ("staging-songs.json");
    songsIn.replaceWithText (u(R"json({"bands":{"metallica":[{"album":"Kill 'Em All","jahr":1988,"nummern":{"Am I Evil":11}},
        {"album":"Metallica","jahr":1991,"titel":["Enter Sandman","Sad but True"]},
        {"album":"Ride the Lightning","jahr":1984,"titel":["Fight Fire with Fire","Ride the Lightning","For Whom the Bell Tolls"]}]}})json"));
    int bandsAdded = -1, songsAdded = -1;
    const AlbumIndex::Songs imported { { "Metallica", "Sad but True" }, { "Metallica", "Am I Evil" }, { "Metallica", "Ride the Lightning" },
                                       { "Metallica", "For Whom the Bell Tolls" }, { "Metallica", "Gibt es nicht" } };
    CHECK (index.mergeAlbums (songsIn, own, imported, bandsAdded, songsAdded).isEmpty() && bandsAdded == 0 && songsAdded == 3);
    AlbumIndex withSongs;
    CHECK (withSongs.load (file.getSiblingFile ("leer-gibt-es-nicht.json"), own).isEmpty());
    const auto sad = withSongs.find ("Metallica", "sad but true"), evil = withSongs.find ("Metallica", "Am I Evil");
    const auto bell = withSongs.find ("Metallica", "For Whom the Bell Tolls");
    CHECK (sad && sad->album == "Metallica" && sad->year == 1991 && sad->number == 2);
    CHECK (evil && evil->album == "Kill 'Em All" && evil->year == 1983 && evil->number == 0);       // the list's album, no number
    CHECK (bell && bell->album == "Ride the Lightning" && bell->year == 1984 && bell->number == 0);
    CHECK (! withSongs.find ("Metallica", "Ride the Lightning") && ! withSongs.find ("Metallica", "Gibt es nicht"));   // placed / nowhere
    CHECK (index.mergeAlbums (songsIn, own, imported, bandsAdded, songsAdded).isEmpty() && songsAdded == 0);    // own file holds them now
    songsIn.deleteFile();
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

// die (Band, Song) eines Imports aus den Tags seiner Bausteine: je Paar einmal, ohne Band oder Song nichts
static void testImportedSongs()
{
    const auto dir = tempFile ("patterns");
    dir.deleteRecursively();
    dir.createDirectory();
    const auto write = [&dir] (const char* name, const char* tags)
    {
        dir.getChildFile (name).replaceWithText (juce::String::fromUTF8 ("{\"id\":\"x\",\"tags\":[") + juce::String::fromUTF8 (tags) + "]}");
    };
    write ("a.json", "\"song\",\"band:Deep Purple\",\"song:Anyone's Daughter\"");
    write ("b.json", "\"band:deep purple\",\"song:Anyone’s Daughter\"");   // the same pair, other spelling
    write ("c.json", "\"band:Slayer\",\"song:South of Heaven\"");
    write ("d.json", "\"band:Slayer\"");                                      // no song
    write ("e.json", "\"song:Ohne Band\"");
    const auto songs = AlbumIndex::importedSongs (dir);
    CHECK (songs.size() == 2);
    bool purple = false, slayer = false;
    for (const auto& [band, song] : songs)
    {
        purple = purple || (AlbumIndex::key (band) == "deeppurple" && AlbumIndex::key (song) == "anyonesdaughter");
        slayer = slayer || (band == "Slayer" && song == "South of Heaven");
    }
    CHECK (purple && slayer);
    dir.deleteRecursively();
}

void runAlbumTests()
{
    testAlbumView();
    testImportedSongs();
}
