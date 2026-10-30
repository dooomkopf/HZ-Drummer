#include "app/SongRename.h"

int renameCopies (std::vector<SongPart>& parts, const std::string& sourceId, const std::string& oldName, const std::string& newName)
{
    int renamed = 0;
    for (auto& part : parts)
        if (part.kind == PartKind::Pattern && part.sourceId == sourceId && part.copy != nullptr && part.copy->name == oldName)
        {
            auto copy = std::make_shared<hz::Pattern> (*part.copy);
            copy->name = newName;
            part.copy = std::move (copy);
            ++renamed;
        }
    return renamed;
}

juce::String renameCopiesInFiles (const juce::File& songFolder, const juce::File& stateFile, const juce::File& scratchFile,
                                  const std::string& sourceId, const std::string& oldName, const std::string& newName)
{
    juce::StringArray failed;   // one file that cannot be written does not stop the others
    auto songFiles = songFolder.findChildFiles (juce::File::findFiles, false, "*.json");
    songFiles.add (stateFile);
    for (auto& f : songFiles)
    {
        Song song;
        juce::String error;
        if (! f.existsAsFile() || ! loadSong (f, song, error))
            continue;
        if (renameCopies (song.parts, sourceId, oldName, newName) > 0 && ! saveSong (song, f, error))
            failed.add (f.getFileName() + ": " + error);
    }

    std::vector<SongPart> list;
    juce::String error;
    if (scratchFile.existsAsFile() && loadPartList (scratchFile, list, error)
        && renameCopies (list, sourceId, oldName, newName) > 0 && ! savePartList (list, scratchFile, error))
        failed.add (scratchFile.getFileName() + ": " + error);
    return failed.joinIntoString ("\n");
}
