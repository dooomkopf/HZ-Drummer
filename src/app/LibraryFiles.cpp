#include "app/Library.h"
#include "app/Text.h"
#include <algorithm>
#include <optional>

namespace
{
    bool referencesPattern (const juce::var& value, const juce::String& id)
    {
        // JUCE arrays also report isObject(); they are not DynamicObjects.
        if (const auto* array = value.getArray())
        {
            for (const auto& child : *array)
                if (referencesPattern (child, id))
                    return true;
        }
        else if (const auto* object = value.getDynamicObject())
        {
            if (value["source"].toString() == id || value["pattern"].toString() == id)
                return true;
            for (const auto& property : object->getProperties())
                if (referencesPattern (property.value, id))
                    return true;
        }
        return false;
    }

    // into dataFolder/trash/<uuid> as data.json with metadata.json, the way the trash window lists and restores it
    juce::String moveToTrash (const juce::File& file, const juce::String& id, bool song, const juce::File& dataFolder)
    {
        const auto entry = dataFolder.getChildFile ("trash").getChildFile (juce::Uuid().toString());
        if (! entry.createDirectory())
            return "Papierkorb nicht anlegbar.";
        auto* metadata = new juce::DynamicObject();
        metadata->setProperty ("libraryId", id);
        metadata->setProperty ("originalPath", file.getRelativePathFrom (dataFolder));
        metadata->setProperty ("name", juce::JSON::parse (file.loadFileAsString())["name"].toString());
        metadata->setProperty ("deletedAt", juce::Time::getCurrentTime().toISO8601 (true));
        metadata->setProperty ("song", song);
        if (! entry.getChildFile ("metadata.json").replaceWithText (juce::JSON::toString (juce::var (metadata))))
            return "Papierkorb-Metadaten nicht schreibbar. Original unverändert."_u;
        if (! file.moveFileTo (entry.getChildFile ("data.json")))
            return "Datei nicht in Papierkorb verschiebbar: " + file.getFullPathName();
        juce::Logger::writeToLog (juce::Time::getCurrentTime().toISO8601 (true) + " Papierkorb: " + id);
        return {};
    }

    // the files of a folder by the id inside them (files may live in subfolders or have names unrelated to their ids)
    std::map<juce::String, juce::Array<juce::File>> filesById (const juce::File& folder)
    {
        std::map<juce::String, juce::Array<juce::File>> found;
        for (const auto& file : folder.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json"))
        {
            const auto json = juce::JSON::parse (file.loadFileAsString());
            if (json.isObject())
                found[json["id"].toString()].add (file);
        }
        return found;
    }

    // what may still use a pattern: the saved songs, the arranger's state and the shelf; "" or the file not readable
    juce::String usersOfPatterns (const juce::File& dataFolder, std::vector<std::pair<juce::File, juce::var>>& users)
    {
        auto files = dataFolder.getChildFile ("songs").findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json");
        for (const auto* name : { "arranger.json", "ablage.json" })
            if (const auto file = dataFolder.getChildFile (name); file.existsAsFile())
                files.add (file);
        for (const auto& file : files)
        {
            juce::var json;
            if (juce::JSON::parse (file.loadFileAsString(), json).failed() || (! json.isObject() && ! json.isArray()))
                return file.getFileName();
            users.push_back ({ file, json });
        }
        return {};
    }
}

juce::String removeOwnLibraryFile (const std::string& libraryId, const juce::File& dataFolder)
{
    const juce::String id (libraryId);
    const bool song = id.startsWith ("song.");
    if (! song && ! id.startsWith ("user."))
        return "Bibliothek-Bausteine können nicht gelöscht werden."_u;
    const auto diskId = song ? id.substring (5) : id;
    if (diskId.isEmpty())
        return "Ungültige Kennung."_u;

    // Files may live in subfolders or have names unrelated to their ids.
    juce::Array<juce::File> matches;
    for (const auto& file : dataFolder.getChildFile (song ? "songs" : "patterns")
                                   .findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json"))
    {
        const auto json = juce::JSON::parse (file.loadFileAsString());
        if (json.isObject() && json["id"].toString() == diskId)
            matches.add (file);
    }
    if (matches.size() != 1)
        return matches.isEmpty() ? "Eintrag nicht mehr auf dem Datenträger gefunden."_u
                                 : "Mehrere Dateien mit derselben Kennung gefunden. Nichts gelöscht."_u;

    if (! song)
    {
        auto dependencies = dataFolder.getChildFile ("songs").findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json");
        for (const auto* name : { "arranger.json", "ablage.json" })
        {
            const auto file = dataFolder.getChildFile (name);
            if (file.existsAsFile()) dependencies.add (file);
        }
        for (const auto& file : dependencies)
        {
            juce::var json;
            const auto result = juce::JSON::parse (file.loadFileAsString(), json);
            if (result.failed() || (! json.isObject() && ! json.isArray()))
                return "Referenzen nicht prüfbar: "_u + file.getFileName() + ". Nichts gelöscht."_u;
            if (referencesPattern (json, id))
                return "Dieser Baustein wird in " + file.getFileName()
                     + " verwendet. Zuerst dort aus dem Ablauf bzw. der Ablage entfernen.";
        }
    }
    return moveToTrash (matches[0], id, song, dataFolder);
}

std::map<std::string, juce::String> removeOwnLibraryFiles (const std::vector<std::string>& libraryIds, const juce::File& dataFolder)
{
    std::map<std::string, juce::String> result;
    auto songs = filesById (dataFolder.getChildFile ("songs")), patterns = filesById (dataFolder.getChildFile ("patterns"));
    std::vector<std::string> ordered;   // the songs first: a pattern only they used is free afterwards
    for (const bool songsNow : { true, false })
        for (const auto& id : libraryIds)
            if (juce::String (id).startsWith ("song.") == songsNow)
                ordered.push_back (id);
    std::vector<std::pair<juce::File, juce::var>> users;
    std::optional<juce::String> unreadable;   // read once, after the songs of this call went
    for (const auto& libraryId : ordered)
    {
        const juce::String id (libraryId);
        const bool song = id.startsWith ("song.");
        if (! song && ! id.startsWith ("user."))
        {
            result[libraryId] = "Bibliothek-Bausteine können nicht gelöscht werden."_u;
            continue;
        }
        const auto& files = (song ? songs : patterns)[song ? id.substring (5) : id];
        if (files.size() != 1)
        {
            result[libraryId] = files.isEmpty() ? "Eintrag nicht mehr auf dem Datenträger gefunden."_u
                                                : "Mehrere Dateien mit derselben Kennung gefunden. Nichts gelöscht."_u;
            continue;
        }
        if (! song)
        {
            if (! unreadable)
                unreadable = usersOfPatterns (dataFolder, users);
            if (unreadable->isNotEmpty())
            {
                result[libraryId] = "Referenzen nicht prüfbar: "_u + *unreadable + ". Nichts gelöscht."_u;
                continue;
            }
            const auto user = std::find_if (users.begin(), users.end(), [&id] (const auto& u) { return referencesPattern (u.second, id); });
            if (user != users.end())
            {
                result[libraryId] = "wird in " + user->first.getFileName() + " verwendet";
                continue;
            }
        }
        result[libraryId] = moveToTrash (files[0], id, song, dataFolder);
    }
    return result;
}

std::vector<LibraryTrashEntry> listOwnTrash (const juce::File& dataFolder)
{
    std::vector<LibraryTrashEntry> entries;
    for (const auto& folder : dataFolder.getChildFile ("trash").findChildFiles (juce::File::findDirectories, false, "*"))
    {
        if (! folder.getChildFile ("data.json").existsAsFile()) continue;
        const auto metadata = juce::JSON::parse (folder.getChildFile ("metadata.json").loadFileAsString());
        if (metadata.getDynamicObject() == nullptr) continue;
        entries.push_back ({ folder.getFileName(), metadata["libraryId"].toString(), metadata["name"].toString(),
                             metadata["deletedAt"].toString(), (bool) metadata["song"] });
    }
    std::sort (entries.begin(), entries.end(), [] (const auto& a, const auto& b) { return a.deletedAt > b.deletedAt; });
    return entries;
}

bool emptyOwnTrash (const juce::File& dataFolder)
{
    const auto trash = dataFolder.getChildFile ("trash");   // created again by the next deletion
    return ! trash.exists() || trash.deleteRecursively();
}

juce::String restoreOwnTrash (const juce::String& key, const juce::File& dataFolder)
{
    if (key.isEmpty() || ! key.containsOnly ("0123456789abcdef-"))
        return "Ungültiger Papierkorb-Eintrag."_u;
    const auto folder = dataFolder.getChildFile ("trash").getChildFile (key);
    const auto metadata = juce::JSON::parse (folder.getChildFile ("metadata.json").loadFileAsString());
    if (metadata.getDynamicObject() == nullptr || ! folder.getChildFile ("data.json").existsAsFile())
        return "Papierkorb-Eintrag unvollständig."_u;
    const auto relative = metadata["originalPath"].toString();
    const bool song = (bool) metadata["song"];
    const auto libraryId = metadata["libraryId"].toString();
    if (relative.isEmpty() || juce::File::isAbsolutePath (relative))
        return "Ungültiger ursprünglicher Pfad."_u;
    const auto target = dataFolder.getChildFile (relative);
    const auto originals = dataFolder.getChildFile (song ? "songs" : "patterns");
    if (! target.isAChildOf (originals)) return "Ungültiger Wiederherstellungspfad."_u;
    const auto content = juce::JSON::parse (folder.getChildFile ("data.json").loadFileAsString());
    if (content.getDynamicObject() == nullptr || content["id"].toString().isEmpty())
        return "Gesicherte Datei ist ungültig."_u;
    for (const auto& file : originals.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.json"))
        if (juce::JSON::parse (file.loadFileAsString())["id"].toString() == content["id"].toString())
            return "Ein Eintrag mit dieser Kennung existiert bereits. Nichts überschrieben."_u;
    if (target.exists()) return "Die ursprüngliche Datei existiert bereits. Nichts überschrieben."_u;
    if (! target.getParentDirectory().createDirectory()
        || ! folder.getChildFile ("data.json").moveFileTo (target))
        return "Datei konnte nicht wiederhergestellt werden.";
    folder.getChildFile ("metadata.json").deleteFile();
    folder.deleteFile();
    juce::Logger::writeToLog (juce::Time::getCurrentTime().toISO8601 (true) + " Wiederhergestellt: " + libraryId);
    return {};
}
