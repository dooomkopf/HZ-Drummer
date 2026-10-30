// MainComponent, part 2: keys, menus, quick search, help, mixer and audio settings windows.

#include "app/MainComponent.h"
#include "app/SystemDataViews.h"

#include "app/views/SourceBadge.h"

#include <map>

#include "Version.h"
#include "app/AudioSettingsPanel.h"
#include "app/FolderPanel.h"
#include "app/MixerPanel.h"
#include "app/NavigationMenus.h"
#include "app/Text.h"
#include "app/views/Theme.h"

void MainComponent::setupKeys()
{
    KeyActions a;
    a.togglePlay        = [this] { togglePlay(); };
    a.rewind            = [this] { seekToBar (0); };
    a.tapTempo          = [this] { tapTempo(); };
    a.toggleFavorite    = [this] { toggleFavorite(); };
    a.nextSound         = [this] { selectSound (((int) sound + 1) % 4); };
    a.toggleBeatDisplay = [this] { beatDisplay.setShowHits (! beatDisplay.getShowHits()); saveSettings(); };
    a.changeTempo       = [this] (double d) { changeTempo (d); };
    a.stepPattern       = [this] (int dir, bool fav) { stepPattern (dir, fav); };
    a.stepCurrent       = [this] (int dir, bool fav) { stepCurrent (dir, fav); };
    a.stepStyle         = [this] (int dir) { stepStyle (dir); };
    a.seekPart          = [this] (int dir) { seekPart (dir); };
    a.patternMenu       = [this] { showPatternMenu(); };
    a.meterMenu         = [this] { showMeterMenu(); };
    a.editor            = [this] { openEditor(); };
    a.mixer             = [this] { showMixer(); };
    a.search            = [this] { openSearch(); };
    a.help              = [this] { showHelp (true); };
    keyBindings = makeKeyBindings (a);

    std::vector<HelpOverlay::Entry> entries;
    for (auto& b : keyBindings)
        if (b.keys.isNotEmpty())
            entries.push_back ({ b.keys, b.description });
    helpOverlay.setEntries (std::move (entries));
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    return handleGlobalKey (key);
}

bool MainComponent::handleGlobalKey (const juce::KeyPress& key)
{
    if (quickSearch.isVisible())
        return false;   // typing belongs to the search field

    if (helpOverlay.isVisible())
    {
        showHelp (false);   // any key closes the help
        return true;
    }

    for (auto& b : keyBindings)
        if (b.matches (key))
        {
            b.action();
            return true;
        }
    return false;
}

void MainComponent::showHelp (bool show)
{
    helpOverlay.setVisible (show);
    if (show)
        helpOverlay.toFront (false);
}

void MainComponent::showPatternMenu()
{
    PatternMenuActions actions;
    actions.select = [this] (int i) { selectPattern (i); };
    actions.preview = [this] (int i) { previewPattern (i); };
    actions.isPreviewing = [this] (int i) { return audition == Audition::Menu && menuPreviewIndex == i && control.play; };   // the mark means "sounds"
    actions.toggleFavorite = [this] { toggleFavorite(); };
    actions.rename = [this] (const std::string& id) { askRenameEntry (id); };
    actions.renameFolder = [this] (FolderTree::Area area, const FolderPath& path) { askRenameFolder (area, path); };
    actions.showErrors = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Fehlerhafte Pattern-Dateien",
                                                library.errors().joinIntoString ("\n"));
    };
    buildPatternMenu (library, currentPattern, actions).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternButton),
        [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
        {
            if (safe != nullptr && result == 0)
                safe->cancelPreview();   // dismissed without a choice: back to the previous state
        });
}

void MainComponent::refreshFolders()
{
    if (foldersWindow == nullptr)
        return;
    auto* frame = dynamic_cast<GlowFrame*> (foldersWindow->getContentComponent());
    if (auto* panel = dynamic_cast<FolderPanel*> (frame != nullptr ? frame->content() : foldersWindow->getContentComponent()))
        panel->refresh();
}

void MainComponent::confirmDeleteLibraryEntry (const std::string& id)
{
    const int index = library.indexOf (id);
    if (index < 0 || (! library.isUserPattern (index) && ! library.isSong (index)))
        return;
    const bool song = library.isSong (index);
    const auto name = juce::String (library.get (index).name);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
        song ? "Arrangement löschen"_u : "Pattern löschen"_u,
        "\"" + name + "\" in den Papierkorb verschieben?\n\n"_u
            + (song ? "Die verwendeten Patterns bleiben erhalten."
                    : "Patterns, die noch in einem Ablauf oder der Ablage verwendet werden, können nicht gelöscht werden."_u),
        "In Papierkorb", "Abbrechen", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this), id] (int result)
        {
            if (safe == nullptr || result != 1) return;
            safe->cancelPreview();
            if (const auto error = safe->removeLibraryEntry (id); error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Löschen nicht möglich"_u, error);
        }));
}

juce::String MainComponent::removeLibraryEntry (const std::string& id)
{
    const int index = library.indexOf (id);
    if (index < 0)
        return "Der Eintrag ist nicht mehr in der Bibliothek."_u;
    // the open arranger may hold the pattern unsaved; saved songs, arranger.json and ablage.json are checked by the library
    if (library.isUserPattern (index) && editorWindow != nullptr && editorWindow->getArranger().usesPattern (id))
        return "Dieses Pattern ist noch im aktuellen Ablauf oder in der Ablage. Zuerst dort entfernen.";

    const auto entry = library.get (index);
    const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
    if (const auto error = library.remove (entry); error.isNotEmpty())
        return error;

    badges.release (id);   // a restored pattern gets a badge again when the library reloads
    currentPattern = library.indexOf (selectedId);
    quickSearch.close();
    sendCurrentPlan();
    updateButtons();
    saveSettings();
    if (editorWindow != nullptr)
    {
        editorWindow->getArranger().songDeleted (id);
        editorWindow->getArranger().libraryChanged();
    }
    return {};
}

void MainComponent::previewPattern (int index)
{
    if (index < 0 || index >= library.size())
        return;
    if (audition == Audition::Menu && menuPreviewIndex == index)
    {
        // P again on the same entry: stop (and P once more: go on). Closing the menu after a
        // stop leaves it quiet instead of restarting what played before.
        togglePlay();
        beforePreview.wasPlaying = false;
        return;
    }
    if (audition != Audition::Menu)
    {
        // remember everything the preview overrides: what played, where, how fast, and whether at all
        beforePreview.editing = editing;
        beforePreview.audition = audition;
        beforePreview.arranging = arranging;
        beforePreview.wasPlaying = control.play;
        beforePreview.tempoTenths = control.tempoTenths;
        beforePreview.bar = view.hasPosition() ? view.bar : -1;
        audition = Audition::Menu;
    }
    menuPreviewIndex = index;
    arranging.reset();
    editing = library.get (index);
    setTempo (library.get (index).tempo);   // the pattern's own tempo, a doom groove must not run at 180
    sendCurrentPlan();
    if (! control.play)
        togglePlay();
}

void MainComponent::cancelPreview()
{
    if (audition != Audition::Menu)
        return;
    editing = beforePreview.editing;
    audition = beforePreview.audition;
    arranging = beforePreview.arranging;
    menuPreviewIndex = -1;
    setTempo (beforePreview.tempoTenths / 10.0);
    sendCurrentPlan();

    // back to the bar it was at (the seek lands in the plan just sent, see PatternPlayer::seek)
    if (const auto* info = plans.latest(); info != nullptr && beforePreview.bar >= 0 && beforePreview.bar < (int) info->bars.size())
    {
        engine.seek (info->bars[(size_t) beforePreview.bar].start);
        pendingDisplay.clear();
    }
    if (! beforePreview.wasPlaying && control.play)
        togglePlay();
    updateButtons();
}

void MainComponent::showMeterMenu()
{
    MeterLibrary::MenuCallbacks cb;
    cb.select = [this] (int i) { selectMeter (i); };
    cb.custom = [this] { meters.askCustomMeter (this, currentMeter, [this] (int i) { selectMeter (i); }); };
    cb.toggleFavorite = [this] (int) { if (! patternMode()) toggleFavorite(); };
    if (patternMode())
        cb.hint = "Enthalten: "_u + Library::meterText (library.get (currentPattern))
                + "  \xc2\xb7  Auswahl wechselt zum Metronom"_u;
    meters.buildMenu (patternMode() ? -1 : currentMeter, cb).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&meterButton));
}

void MainComponent::openSearch()
{
    auto items = buildSearchItems (library, meters,
                                   [this] (const std::string& id) { selectPatternById (id); },
                                   [this] (int meterIndex) { selectMeter (meterIndex); },
                                   [this] { selectPattern (-1); });
    showHelp (false);
    quickSearch.open (std::move (items));
}

//==============================================================================
// Mixer and audio settings: free-floating windows, the main window stays usable

void MainComponent::setMix (int channel, mix::Setting setting)
{
    mixSettings[(size_t) channel] = setting;
    engine.setMixState (mix::pack (mixSettings));

    // volume and mute are persisted, solo only lasts for the session
    const juce::String k (mix::channelKey (channel));
    auto& props = *appProperties.getUserSettings();
    props.setValue ("mix_" + k + "_db", (double) setting.db);
    props.setValue ("mix_" + k + "_mute", setting.muted);
}

bool MainComponent::channelInUse (int channel) const
{
    // the count-in clicks before a pattern that never clicks itself
    return channelsInUse[(size_t) channel] || (channel == mix::Click && countInWillClick());
}

void MainComponent::showMixer()
{
    if (mixerWindow == nullptr)
    {
        juce::DialogWindow::LaunchOptions o;
        const juce::Component::SafePointer<MainComponent> safe (this);
        o.content.setOwned (new GlowFrame (new MixerPanel (mixSettings,
            [safe] (int ch, mix::Setting st) { if (safe != nullptr) safe->setMix (ch, st); },
            [safe] (int ch) { return safe != nullptr ? safe->engine.takeLevel (ch) : 0.0f; },
            [safe] (int ch) { return safe == nullptr || safe->channelInUse (ch); })));
        o.dialogTitle = "Mixer";
        o.dialogBackgroundColour = theme::panel;
        o.useNativeTitleBar = true;
        o.resizable = true;
        mixerWindow.reset (o.create());
    }
    mixerWindow->setVisible (true);
    mixerWindow->toFront (true);
}

void MainComponent::showAudioSettings()
{
    if (audioWindow == nullptr)
    {
        // the audio settings plus a few lines about the system and the badges in use
        struct SystemPanel : juce::Component
        {
            AudioSettingsPanel selector;
            juce::Label badgeLine, info, version;
            juce::TextButton trash { "Papierkorb" }, logs { "Logdateien" }, structure { "Ordnerstruktur" };
            explicit SystemPanel (juce::AudioDeviceManager& dm) : selector (dm)
            {
                addAndMakeVisible (selector);
                addAndMakeVisible (trash); addAndMakeVisible (logs); addAndMakeVisible (structure);
                addAndMakeVisible (badgeLine);
                badgeLine.setFont (juce::FontOptions (20.0f, juce::Font::bold));
                badgeLine.setColour (juce::Label::textColourId, theme::textMain);
                addAndMakeVisible (info);
                info.setFont (juce::FontOptions (14.0f));
                info.setColour (juce::Label::textColourId, theme::textDim);
                info.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (version);
                version.setFont (juce::FontOptions (14.0f));
                version.setColour (juce::Label::textColourId, theme::textDim);
                version.setText ("HZ-Drummer v" HZ_VERSION, juce::dontSendNotification);
                setSize (560, ActiveGlow::height + AudioSettingsPanel::preferredHeight + 4 + 42 + 44 + 50 + 28);
            }
            void resized() override
            {
                auto r = getLocalBounds();
                version.setBounds (r.removeFromBottom (28).reduced (12, 4));
                info.setBounds (r.removeFromBottom (50).reduced (12, 4));
                badgeLine.setBounds (r.removeFromBottom (44).reduced (12, 4));
                selector.setBounds (r.withTrimmedTop (ActiveGlow::height).withHeight (AudioSettingsPanel::preferredHeight));
                auto actions = juce::Rectangle<int> (0, selector.getBottom() + 4, getWidth(), 42).reduced (12, 4);
                structure.setBounds (actions.removeFromRight (160)); actions.removeFromRight (10);   // right-aligned
                logs.setBounds (actions.removeFromRight (160)); actions.removeFromRight (10);
                trash.setBounds (actions.removeFromRight (160));
            }
        };
        auto* panel = new SystemPanel (deviceManager);
        const juce::Component::SafePointer<MainComponent> safe (this);
        panel->trash.onClick = [safe] { if (safe != nullptr) safe->showDataTools (0); };
        panel->logs.onClick = [safe] { if (safe != nullptr) safe->showDataTools (1); };
        panel->structure.onClick = [safe] { if (safe != nullptr) safe->showDataTools (2); };

        // badges: how much of the colour x mark space is taken, and whether any two patterns share one
        const auto st = badges.stats (library.all());
        panel->badgeLine.setText ("Freie Abzeichen: " + juce::String (st.total - st.used) + " von " + juce::String (st.total)
                                  + (st.shared > 0 ? "   (" + juce::String (st.shared) + " Patterns teilen sich eins!)" : juce::String()),
                                  juce::dontSendNotification);
        juce::String text;
        text << "System: " << juce::SystemStats::getOperatingSystemName() << ", " << juce::SystemStats::getCpuModel()
             << ", " << juce::SystemStats::getNumCpus() << " Kerne, " << juce::SystemStats::getMemorySizeInMegabytes() << " MB\n";
        if (auto* dev = deviceManager.getCurrentAudioDevice())
            text << "Audio: " << dev->getName() << " (" << deviceManager.getCurrentAudioDeviceType() << "), " << (int) dev->getCurrentSampleRate()
                 << " Hz, Puffer " << dev->getCurrentBufferSizeSamples() << "   " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "   JUCE " << JUCE_MAJOR_VERSION << "." << JUCE_MINOR_VERSION;
        panel->info.setText (text, juce::dontSendNotification);

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new GlowFrame (panel));
        o.dialogTitle = "System: Audio-Ausgang";
        o.dialogBackgroundColour = theme::panel;
        o.useNativeTitleBar = true;
        o.resizable = false;
        audioWindow.reset (o.create());
    }
    audioWindow->setVisible (true);
    audioWindow->toFront (true);
}

void MainComponent::showDataTools (int tab)
{
    auto& window = tab == 0 ? trashWindow : tab == 1 ? logsWindow : foldersWindow;
    if (window == nullptr)
    {
        juce::Component* panel = nullptr;
        if (tab == 0)
        {
            const juce::Component::SafePointer<MainComponent> safe (this);
            panel = new TrashPanel (Library::userPatternFolder().getParentDirectory(), [safe]
            {
                if (safe == nullptr) return;
                const auto selectedId = safe->patternMode() ? safe->library.get (safe->currentPattern).id : std::string();
                safe->library.reload();
                safe->currentPattern = safe->library.indexOf (selectedId);
                safe->sendCurrentPlan(); safe->updateButtons();
                if (safe->editorWindow != nullptr) safe->editorWindow->getArranger().libraryChanged();
            });
        }
        else if (tab == 1) panel = new LogPanel();
        else panel = new FolderPanel (library);
        panel->setSize (tab == 1 ? 1000 : 820, tab == 0 ? 500 : 640);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (new GlowFrame (panel));
        options.dialogTitle = tab == 0 ? "Papierkorb" : tab == 1 ? "Logdateien" : "Ordnerstruktur";
        options.dialogBackgroundColour = theme::panel; options.useNativeTitleBar = true; options.resizable = true;
        window.reset (options.create());
    }
    auto* frame = dynamic_cast<GlowFrame*> (window->getContentComponent());
    auto* content = frame != nullptr ? frame->content() : window->getContentComponent();
    if (auto* trash = dynamic_cast<TrashPanel*> (content)) trash->refresh();
    if (auto* logs = dynamic_cast<LogPanel*> (content)) logs->refresh();
    if (auto* folderPanel = dynamic_cast<FolderPanel*> (content)) folderPanel->refresh();
    window->setVisible (true); window->toFront (true);
}

//==============================================================================
// Seeking inside the playing song/pattern

void MainComponent::seekToBar (int bar)
{
    const auto* info = view.plan;
    if (info == nullptr || bar < 0 || bar >= (int) info->bars.size())
        return;

    engine.seek (info->bars[(size_t) bar].start);
    pendingDisplay.clear();   // pulses of the old position are no longer relevant

    // show the new position at once (also while paused). Without a position the view showed the
    // latest plan (a fresh selection), so the marker belongs to that plan now, not to the one heard last.
    if (! view.hasPosition())
        shown.planTag = plans.latestTag();
    shown.bar = bar;
    shown.pulse = 0;
    shown.meter = info->bars[(size_t) bar].meterTag;
    shown.timeMs = juce::Time::getMillisecondCounterHiRes();
    hasShown = true;
    updateView();
}

void MainComponent::seekPart (int dir)
{
    if (view.spans == nullptr || view.spans->empty())
        return;

    const auto& spans = *view.spans;
    const int bar = view.live ? view.bar : 0;
    int current = 0;
    for (int i = 0; i < (int) spans.size(); ++i)
        if (bar >= spans[(size_t) i].firstBar)
            current = i;

    int target = current + (dir > 0 ? 1 : 0);
    if (dir < 0 && bar == spans[(size_t) current].firstBar)   // already at the start: previous part
        target = current - 1;
    target = juce::jlimit (0, (int) spans.size() - 1, target);
    seekToBar (spans[(size_t) target].firstBar);
}
