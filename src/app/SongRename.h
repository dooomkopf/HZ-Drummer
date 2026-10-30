#pragma once

#include "app/Song.h"

// Renaming an own pattern renames the copies songs keep of it (format v2) too, as long as they
// still carry its old name. A block's own label, set in the arranger, is separate and stays.
int renameCopies (std::vector<SongPart>&, const std::string& sourceId, const std::string& oldName, const std::string& newName);

// The same in the saved files: every song in songFolder, the arranger's working state and the
// Ablage. Files that cannot be read are left alone; one that cannot be written does not stop the
// others. Returns "" or every file that could not be written, one per line.
juce::String renameCopiesInFiles (const juce::File& songFolder, const juce::File& stateFile, const juce::File& scratchFile,
                                  const std::string& sourceId, const std::string& oldName, const std::string& newName);
