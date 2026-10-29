// patternc: compiles drum tab files (*.tab) into the JSON pattern library.
//
// Usage: patternc <tab-folder> <json-folder>
// Each .tab file may contain several patterns (separated by ---). Output: <json-folder>/<id>.json
// On an error nothing is written.

#include "app/PatternIO.h"
#include "engine/TabParser.h"

#include <set>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::printf ("Aufruf: patternc <tab-ordner> <json-ordner>\n");
        return 1;
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto inDir = cwd.getChildFile (argv[1]);
    const auto outDir = cwd.getChildFile (argv[2]);

    auto files = inDir.findChildFiles (juce::File::findFiles, true, "*.tab");
    files.sort();

    std::vector<hz::Pattern> all;
    std::set<std::string> ids;

    for (auto& f : files)
    {
        auto result = hz::parseTabs (f.loadFileAsString().toStdString());
        if (! result.error.empty())
        {
            std::printf ("FEHLER %s: %s\n", f.getFileName().toRawUTF8(), result.error.c_str());
            return 1;
        }

        // Folder name as style tag if not specified (e.g. library-src/thrash/*.tab)
        const auto style = f.getParentDirectory() == inDir ? juce::String() : f.getParentDirectory().getFileName();

        for (auto& p : result.patterns)
        {
            if (! ids.insert (p.id).second)
            {
                std::printf ("FEHLER %s: doppelte id %s\n", f.getFileName().toRawUTF8(), p.id.c_str());
                return 1;
            }
            if (style.isNotEmpty() && (p.tags.empty() || p.tags.front() != style.toStdString()))
                p.tags.insert (p.tags.begin(), style.toStdString());

            // Seed from the id: stable across recompiles
            p.humanizeSeed = (std::uint32_t) juce::String (p.id).hashCode() | 1u;
            all.push_back (std::move (p));
        }
    }

    // Only the bundled library ("lib.*" ids) is ever written. Every target file must be one of ours:
    // any other file means a user folder, and then nothing is touched (this once wiped user patterns).
    if (all.empty())
    {
        std::printf ("ABBRUCH: keine Patterns in %s gefunden, nichts geändert\n", inDir.getFullPathName().toRawUTF8());
        return 1;
    }
    for (auto& p : all)
    {
        const juce::String id (p.id);
        if (! id.startsWith ("lib.") || ! id.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789.-"))
        {
            std::printf ("ABBRUCH: ungültige id %s (erlaubt: lib.[a-z0-9.-]), nichts geändert\n", p.id.c_str());
            return 1;
        }
    }

    outDir.createDirectory();
    for (auto& f : outDir.findChildFiles (juce::File::findFiles, false, "*"))
    {
        if (! (f.getFileName().startsWith ("lib.") && f.hasFileExtension ("json")))
        {
            std::printf ("ABBRUCH: %s enthält fremde Dateien (%s), nichts geändert\n",
                         outDir.getFullPathName().toRawUTF8(), f.getFileName().toRawUTF8());
            return 1;
        }
    }

    // write everything to a staging folder first; the old library is replaced only if all succeeded
    const auto staging = outDir.getSiblingFile ("." + outDir.getFileName() + ".staging");
    staging.deleteRecursively();
    staging.createDirectory();
    for (auto& p : all)
    {
        juce::String err;
        if (! savePattern (p, staging.getChildFile (juce::String (p.id) + ".json"), err))
        {
            std::printf ("FEHLER: %s (alte Bibliothek unverändert)\n", err.toRawUTF8());
            staging.deleteRecursively();
            return 1;
        }
    }

    for (auto& old : outDir.findChildFiles (juce::File::findFiles, false, "lib.*.json"))
        old.deleteFile();
    for (auto& f : staging.findChildFiles (juce::File::findFiles, false, "*.json"))
        if (! f.moveFileTo (outDir.getChildFile (f.getFileName())))
        {
            std::printf ("FEHLER: kann %s nicht verschieben\n", f.getFileName().toRawUTF8());
            return 1;
        }
    staging.deleteRecursively();

    std::printf ("%d Patterns aus %d Dateien -> %s\n", (int) all.size(), files.size(), outDir.getFullPathName().toRawUTF8());
    return 0;
}
