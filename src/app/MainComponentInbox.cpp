// MainComponent, part 5: the import folder ~/HZ-Drummer-Import (ImportInbox, tools/import/eingang.py). A prepared
// file is installed like a source (installFromSource), the file moves to "erledigt" or "Fehler", a short green notice
// tells of success, everything else goes to the import log. No window ever opens for it.

#include "app/MainComponent.h"
#include "app/KitLoader.h"
#include "app/SystemDataViews.h"

namespace
{
    juce::File importLog()
    {
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/import.log");
    }

    // into the folder's "erledigt" or "Fehler"; a name that is there already gets " (2)", " (3)" …
    bool moveTo (const juce::File& file, const juce::String& sub)
    {
        const auto dir = ImportInbox::folder().getChildFile (sub);
        dir.createDirectory();
        auto target = dir.getChildFile (file.getFileName());
        for (int n = 2; target.exists(); ++n)
            target = dir.getChildFile (file.getFileNameWithoutExtension() + " (" + juce::String (n) + ")" + file.getFileExtension());
        return file.moveFileTo (target);
    }

    juce::String reasonIn (const juce::String& output)   // the last "HINWEIS:" of eingang.py, without the file's name
    {
        juce::String reason;
        for (const auto& line : juce::StringArray::fromLines (output))
            if (line.startsWith ("HINWEIS: "))
                reason = line.substring (9).fromFirstOccurrenceOf (": ", false, false);
        return reason.isNotEmpty() ? reason : juce::String ("nicht lesbar, Grund im Import-Log");
    }

    juce::String lastLineStartingWith (const juce::String& output, const juce::String& start)   // eingang.py's last word
    {
        juce::String found;
        for (const auto& line : juce::StringArray::fromLines (output))
            if (line.startsWith (start))
                found = line.substring (start.length()).trim();
        return found;
    }
}

void MainComponent::startInbox()
{
    inbox = std::make_unique<ImportInbox> (findAppFolder ("tools").getChildFile ("import/eingang.py"),
                                           Library::userPatternFolder().getParentDirectory());
    inbox->onResult = [safe = juce::Component::SafePointer<MainComponent> (this)] (const ImportInbox::Result& r)
    {
        if (safe != nullptr)
            safe->finishInboxJob (r);
    };
    inbox->canDeliver = [] { return juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0; };
    inbox->onStart = [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::String& name)
    {
        if (safe != nullptr)
            safe->noticeDot.setBusy (name);   // the dot pulses: the file was seen
    };
    noticeDot.onOpenFolder = [] { ImportInbox::folder().startAsProcess(); };
    noticeDot.onShowLog = [safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe == nullptr)
            return;
        safe->showDataTools (1);
        if (auto* frame = dynamic_cast<GlowFrame*> (safe->logsWindow->getContentComponent()))
            if (auto* logs = dynamic_cast<LogPanel*> (frame->content()))
                logs->showLog (3);   // the import log
    };
    inbox->start();
}

void MainComponent::finishInboxJob (const ImportInbox::Result& r)
{
    juce::StringArray log;
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S");
    const auto ownAlbums = Library::userPatternFolder().getParentDirectory().getChildFile ("alben.json");
    if (r.band.isNotEmpty())   // the albums of a band that had no network at its import
    {
        if (r.exitCode == 0)
        {
            AlbumIndex::Songs songs;
            for (const auto& s : AlbumIndex::importedSongs (Library::userPatternFolder()))
                if (AlbumIndex::key (s.first) == AlbumIndex::key (r.band))
                    songs.push_back (s);
            int bands = 0, filled = 0;
            const auto error = library.albumIndex().mergeAlbums (r.staging.getChildFile ("alben.json"), ownAlbums, songs, bands, filled);
            ImportInbox::notePending (r.band, error.isNotEmpty());
            const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
            library.reload();
            currentPattern = selectedId.empty() ? -1 : library.indexOf (selectedId);
            refreshFolders();
            refreshMenuFolders();
            updateButtons();
            if (editorWindow != nullptr)
                editorWindow->getArranger().libraryChanged();
            log.add (stamp + " Eingang: Alben nachgeholt: " + r.band + (bands + filled > 0 ? "" : ", nichts Neues")
                     + (error.isNotEmpty() ? " (alben.json: " + error + ")" : juce::String()));
            noticeDot.add ("Alben: " + r.band, error.isEmpty() ? "nachgeholt" : "alben.json: " + error, error.isNotEmpty());
        }
        else
            log.add (stamp + " Eingang: Alben für "_u + r.band + " weiter offen (MusicBrainz nicht erreichbar)");
    }
    else if (r.exitCode == -1)   // eingang.py did not get to its end (python3 missing, killed): the file is not at fault
    {
        log.add (stamp + " Eingang: " + r.file.getFileName() + ": nicht bearbeitet, bleibt liegen bis zum Neustart ("
                 + r.output.fromLastOccurrenceOf ("\n", false, false).trim() + ")");
        noticeDot.add (r.file.getFileName(), "nicht bearbeitet, bleibt liegen bis zum Neustart (Grund im Import-Log)", true);
        if (inbox != nullptr)
            inbox->ignore (r.file);
    }
    else if (r.file.getSize() != r.size || r.file.getLastModificationTime().toMilliseconds() != r.modified)
        log.add (stamp + " Eingang: " + r.file.getFileName() + juce::String::fromUTF8 (": während der Umwandlung geändert, "
                 "wird neu gelesen"));   // the next look takes it again; nothing of this run is installed
    else if (r.exitCode == 4)   // neither the name nor the file names band and title: renaming it there helps
    {
        log.add (stamp + " Eingang: " + r.file.getFileName() + juce::String::fromUTF8 (": ohne Band und Titel, bleibt liegen "
                 "(im Ordner umbenennen: Band - Titel.endung)"));
        noticeDot.add (r.file.getFileName(), "bleibt liegen: ohne Band und Titel, im Ordner umbenennen", true);
        if (inbox != nullptr)
            inbox->ignore (r.file);   // until it gets another name (another path) or the next start
    }
    else if (r.exitCode == 0 || r.exitCode == 2)
    {
        const auto songs = AlbumIndex::importedSongs (r.staging.getChildFile ("patterns"));
        const auto report = installFromSource (r.staging, false);
        const bool moved = moveTo (r.file, "erledigt");
        if (r.exitCode == 2 && ! songs.empty())
            ImportInbox::notePending (songs.front().first, true);   // asked for again a minute after the start
        log.add (stamp + " Eingang: " + r.file.getFileName() + ": " + report
                 + (r.exitCode == 2 ? ", Alben später" : "") + (moved ? "" : ", Datei ließ sich nicht verschieben"_u));
        GhostMenu::Content notice;   // green, as the ghost menu: what came and where it stands
        notice.path.add ("Eingespielt");
        notice.list.add (lastLineStartingWith (r.output, "Fertig: ").upToFirstOccurrenceOf (", Alben", false, false));
        if (! songs.empty())
        {
            const auto place = library.albumIndex().find (songs.front().first, songs.front().second);
            notice.list.add (place ? songs.front().first + juce::String::fromUTF8 (" \xe2\x80\xba ") + albumview::albumFolder (*place)
                                   : juce::String (r.exitCode == 2 ? "Album folgt" : "ohne Album"));
        }
        showGhost (notice);
        noticeDot.add (r.file.getFileName(), notice.list.joinIntoString (juce::String::fromUTF8 ("  \xe2\x80\xba  ")), r.exitCode == 2);
        if (! moved && inbox != nullptr)
            inbox->ignore (r.file);   // not again and again
    }
    else
    {
        const bool moved = moveTo (r.file, "Fehler");
        log.add (stamp + " Eingang: " + r.file.getFileName() + ": nicht eingespielt"
                 + (moved ? ", liegt in Fehler" : ", Datei ließ sich nicht verschieben"_u));
        noticeDot.add (r.file.getFileName(), "in Fehler: " + reasonIn (r.output), true);
        if (! moved && inbox != nullptr)
            inbox->ignore (r.file);
    }
    for (const auto& line : juce::StringArray::fromLines (r.output.trim()))
        if (! line.startsWith ("ERGEBNIS "))
            log.add ("    " + line);
    importLog().getParentDirectory().createDirectory();
    importLog().appendText (log.joinIntoString ("\n") + "\n");
    noticeDot.setBusy ({});   // the next file, if one waits, starts the pulse again
}
