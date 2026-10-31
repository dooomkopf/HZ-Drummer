// MainComponent, part 7: System › Quellen. Building blocks HZ-Drummer does not ship are loaded on a click from
// their original sources and converted on this computer (QuellenPanel, tools/import/quellen.py); here they go
// into the data folder, never overwriting anything, the arrangements into a folder per band.

#include "app/MainComponent.h"

#include "app/KitLoader.h"   // findAppFolder
#include "app/QuellenPanel.h"
#include "app/SystemDataViews.h"
#include "app/Text.h"
#include "app/views/ActiveGlow.h"
#include "app/views/Theme.h"

void MainComponent::showQuellen()
{
    if (quellenWindow != nullptr && quellenWindow->isVisible())
    {
        quellenWindow->toFront (true);
        return;
    }
    // a fresh panel each time: what is there already may have changed
    auto* panel = new QuellenPanel (findAppFolder ("tools").getChildFile ("import/quellen.py"),
                                    Library::userPatternFolder().getParentDirectory());
    const juce::Component::SafePointer<MainComponent> safe (this);
    panel->onInstall = [safe] (const juce::File& staging) { return safe != nullptr ? safe->installFromSource (staging) : juce::String(); };
    panel->onShowLog = [safe]
    {
        if (safe == nullptr)
            return;
        safe->showDataTools (1);
        if (auto* frame = dynamic_cast<GlowFrame*> (safe->logsWindow->getContentComponent()))
            if (auto* logs = dynamic_cast<LogPanel*> (frame->content()))
                logs->showLog (3);
    };
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new GlowFrame (panel));
    options.dialogTitle = "Quellen importieren";
    options.dialogBackgroundColour = theme::panel;
    options.useNativeTitleBar = true;
    options.resizable = false;
    quellenWindow.reset (options.create());
    quellenWindow->setVisible (true);
    quellenWindow->toFront (true);
}

juce::String MainComponent::installFromSource (const juce::File& staging)
{
    int patterns = 0, songs = 0, skipped = 0;
    juce::StringArray newSongs;   // only what this import put there is filed into folders
    auto copyAll = [&skipped] (const juce::File& from, const juce::File& to, int& count, juce::StringArray* copied)
    {
        to.createDirectory();
        for (const auto& f : from.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*.json"))
        {
            const auto target = to.getChildFile (f.getFileName());
            if (target.exists())
                ++skipped;   // never overwrite: what the user made or an older install left stays as it is
            else if (f.copyFileTo (target))
            {
                ++count;
                if (copied != nullptr)
                    copied->add ("song." + juce::JSON::parse (target)["id"].toString());
            }
        }
    };
    copyAll (staging.getChildFile ("patterns"), Library::userPatternFolder(), patterns, nullptr);
    copyAll (staging.getChildFile ("songs"), Library::songFolder(), songs, &newSongs);

    // the library knows them now; the main window keeps its selection by id
    const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
    library.reload();
    currentPattern = selectedId.empty() ? -1 : library.indexOf (selectedId);

    // the arrangements into a folder per band, as the import says
    const auto info = juce::JSON::parse (staging.getChildFile ("ordner.json"));   // kept: the object below lives inside it
    if (auto* bands = info["arrangements"].getDynamicObject())
        for (const auto& band : bands->getProperties())
        {
            juce::StringArray ids;
            if (const auto* list = band.value.getArray())
                for (const auto& id : *list)
                    if (newSongs.contains (id.toString()))   // an arrangement that was there before stays where it is
                        ids.add (id.toString());
            if (ids.isEmpty())
                continue;
            library.editFolders (FolderTree::Songs, [&band, &ids] (FolderNode& root)
            {
                const auto name = band.name.toString();
                const FolderPath path (name);
                if (folders::find (root, path) == nullptr)
                    if (const auto error = folders::addFolder (root, {}, name); error.isNotEmpty())
                        return error;
                for (const auto& id : ids)
                    folders::moveItem (root, id.toStdString(), path);
                return juce::String();
            });
        }
    refreshFolders();
    updateButtons();
    if (editorWindow != nullptr)
        editorWindow->getArranger().libraryChanged();

    juce::String report = juce::String (patterns) + " Bausteine, " + juce::String (songs) + " Arrangements eingespielt";
    if (skipped > 0)
        report << " (" << skipped << " schon da)";
    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/import.log")
        .appendText ("Eingespielt: " + report + "\n");
    return report;
}
