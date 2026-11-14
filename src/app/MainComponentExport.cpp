// MainComponent, part: the export button right of "Löschen" and its menu. The loaded pattern or song (a loop range
// that is on: only that) goes as MIDI, MP3, WAV or FLAC into ~/HZ-Drummer-Export, rendered on a thread (AudioExport.h)
// at the tempo and with the mixer as set. Behind MP3, WAV and FLAC the level, A or B (AudioExport.h). The menu also holds
// how often a pattern or range repeats, click and count-in, and opens the folder. A green notice says what came out.

#include "app/MainComponent.h"

#include "app/Mp3Encoder.h"
#include "app/Text.h"

namespace
{
    enum MenuId
    {
        midiId = 1, mp3Id, wavId, flacId,   // in the order of ExportJob::Format, from 1
        clickId = 10, countInId, folderId, infoId,
        repeatsId = 100,                    // + the number of repeats
        levelId = 1000                      // + 10 * format, + 1 for B
    };

    // the two choices behind MP3, WAV and FLAC
    juce::String levelText (ExportJob::Level level)
    {
        return level == ExportJob::Level::Loudness ? "B: −14 LUFS (lauter mit Limiter)"_u
                                                   : "A: Spitze −1 dBFS (leiser und dynamischer)"_u;
    }
}

juce::File MainComponent::exportFolder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("HZ-Drummer-Export");
}

void MainComponent::showExportMenu()
{
    if (! patternMode() || exportJob != nullptr)
        return;
    const bool mp3 = Mp3Encoder::available();
    juce::PopupMenu menu, repeats;
    for (const int n : { 1, 2, 4, 8, 16 })
        repeats.addItem (repeatsId + n, juce::String (n), true, exportRepeats == n);
    menu.addItem (midiId, "MIDI");
    for (const auto f : { ExportJob::Format::Mp3, ExportJob::Format::Wav, ExportJob::Format::Flac })
    {
        // a click exports; the arrow behind holds the level, A or B
        const auto level = exportLevel[(size_t) f];
        juce::PopupMenu levels;
        for (const auto l : { ExportJob::Level::Peak, ExportJob::Level::Loudness })
            levels.addItem (levelId + 10 * (int) f + (int) l, levelText (l), true, level == l);
        juce::PopupMenu::Item item (f == ExportJob::Format::Mp3 ? "MP3" : f == ExportJob::Format::Wav ? "WAV" : "FLAC");
        item.itemID = midiId + (int) f;
        item.isEnabled = f != ExportJob::Format::Mp3 || mp3;
        item.subMenu = std::make_unique<juce::PopupMenu> (std::move (levels));
        item.shortcutKeyDescription = levelText (level);
        menu.addItem (std::move (item));
    }
    if (! mp3)
        menu.addItem (infoId, "MP3 braucht das Paket libmp3lame0", false);
    menu.addSeparator();
    menu.addSubMenu ("Wiederholungen (Baustein): " + juce::String (exportRepeats), repeats);
    menu.addItem (clickId, "mit Klick", true, exportClick);
    menu.addItem (countInId, "mit Einzählen"_u, true, exportCountIn);
    menu.addSeparator();
    menu.addItem (folderId, "Export-Ordner öffnen"_u);
    const juce::Component::SafePointer<MainComponent> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton),
                        [safe] (int result) { if (safe != nullptr) safe->exportMenuChosen (result); });
}

void MainComponent::exportMenuChosen (int result)
{
    const juce::Component::SafePointer<MainComponent> safe (this);
    const auto reopen = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showExportMenu(); }); };
    if (result > repeatsId || result == clickId || result == countInId)
    {
        // a setting: changed, saved, and the menu comes back to pick the format
        if (result >= levelId)
            exportLevel[(size_t) juce::jlimit (0, 3, (result - levelId) / 10)] = (result - levelId) % 10 == 1 ? ExportJob::Level::Loudness
                                                                                                            : ExportJob::Level::Peak;
        else if (result > repeatsId)
            exportRepeats = result - repeatsId;
        else if (result == clickId)
            exportClick = ! exportClick;
        else
            exportCountIn = ! exportCountIn;
        saveSettings();
        reopen();
    }
    else if (result == folderId)
    {
        exportFolder().createDirectory();
        exportFolder().startAsProcess();
    }
    else if (result >= midiId && result <= flacId)
        startExport ((ExportJob::Format) (result - midiId));
}

void MainComponent::startExport (ExportJob::Format format)
{
    if (! patternMode() || exportJob != nullptr || editing.has_value() || arranging.has_value())
        return;
    const auto fail = [] (const juce::String& why)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export nicht möglich"_u, why);
    };
    const auto& entry = library.get (currentPattern);
    const auto* song = library.song (currentPattern);
    const auto source = song != nullptr ? makeSongPlan (*song, library) : makePatternPlan (entry);
    if (source == nullptr || source->bars.empty())
        return fail ("Das Arrangement verweist auf einen Baustein, den es nicht mehr gibt.");

    ExportSpan span;
    span.repeats = song != nullptr ? 1 : exportRepeats;
    if (loopOn && loopFirst >= 0 && loopPatternId == source->patternId && loopFirst < (int) source->bars.size())
    {
        // the loop range that is on: only those bars, as often as a pattern
        const auto& last = source->bars[(size_t) juce::jlimit (loopFirst, (int) source->bars.size() - 1, loopLast)];
        span.from = source->bars[(size_t) loopFirst].start;
        span.to = last.start + last.length;
        span.repeats = exportRepeats;
    }
    span.click = exportClick;
    span.countIn = exportCountIn;

    ExportJob::Request request;
    request.format = format;
    request.level = exportLevel[(size_t) format];
    request.tempo = control.tempoTenths / 10.0;
    request.timeline = makeExportTimeline (*source, span, request.tempo, 4.0);   // 4 s of silent bars: room for 3 s ringing out
    request.mix = mix::pack (mixSettings);
    request.kit = kits->kit();
    request.title = juce::String (entry.name);
    if (request.timeline.plan == nullptr)
        return fail ("Im gewählten Bereich liegen keine Takte."_u);
    if (format != ExportJob::Format::Midi && request.kit == nullptr)
        return fail ("Es ist kein Drum-Kit geladen (siehe README, Drum-Kit einrichten).");
    if (! exportFolder().createDirectory())
        return fail ("Der Ordner " + exportFolder().getFullPathName() + " ließ sich nicht anlegen."_u);
    request.file = ExportJob::uniqueFile (exportFolder(), request.title, format);
    exportJob = std::make_unique<ExportJob> (std::move (request));   // the kit manager waits meanwhile (timerCallback)
    updateButtons();
}

void MainComponent::pollExport()
{
    if (exportJob == nullptr)
        return;
    if (! exportJob->finished())
    {
        exportButton.setButtonText (juce::String (juce::roundToInt (exportJob->progress() * 100.0f)) + " %");
        return;
    }
    const auto error = exportJob->error();
    const auto file = exportJob->file();
    exportJob = nullptr;
    exportButton.setButtonText ("Export");
    updateButtons();
    if (error.isNotEmpty())
    {
        juce::Logger::writeToLog ("Export " + file.getFileName() + ": " + error);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export nicht möglich"_u, error);
        return;
    }
    juce::Logger::writeToLog ("Export: " + file.getFullPathName());
    GhostMenu::Content notice;   // green, as the ghost menu
    notice.path.add ("Exportiert");
    notice.list.add (file.getFileName());
    showGhost (notice);
}
