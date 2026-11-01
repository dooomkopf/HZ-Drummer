// MainComponent, part 2: keys, menus, quick search, help, mixer and data windows (audio settings: MainComponentSystem.cpp).

#include "app/MainComponent.h"
#include "app/SystemDataViews.h"
#include "app/views/HZLook.h"

#include "app/views/SourceBadge.h"

#include <map>

#include "Version.h"
#include "app/AudioSettingsPanel.h"
#include "app/FolderPanel.h"
#include "app/MixerPanel.h"
#include "app/NavigationMenus.h"
#include "app/Text.h"
#include "app/views/Theme.h"

// deleting from outside (main window, folder view) leaves a pattern open in the editor alone: the next
// Strg+S there would write it back. The editor's own Löschen clears the editor and does not come here.
juce::String MainComponent::openInEditor (WorkshopWindow* editorWindow, const std::string& id)
{
    if (editorWindow != nullptr && editorWindow->getEditor().getPattern().id == id)
        return "Dieser Baustein ist im Editor geöffnet. Zuerst dort schließen oder dort löschen."_u;
    return {};
}

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
    a.stepCurrent       = [this] (int dir, bool fav)   // as the pattern menu is ordered (BrowseOrder.h)
    {
        if (! patternMode())
            browseMeters (dir, fav);
        else if (fav)
            browseFavorites (dir);
        else
            browseInFolder (dir);
    };
    a.stepStyle         = [this] (int dir) { browseFolder (dir); };
    a.seekPart          = [this] (int dir) { seekPart (dir); };
    a.patternMenu       = [this] { showPatternMenu(); };
    a.meterMenu         = [this] { showMeterMenu(); };
    a.editor            = [this] { openEditor(); };
    a.mixer             = [this] { toggleMixer(); };
    a.loop              = [this] { if (! workshopOpen()) toggleLoop(); };   // like their buttons: not while the workshop is open
    a.history           = [this] (int dir) { moveHistory (dir); };
    a.trainer           = [this] { if (! workshopOpen()) setTrainerOn (! trainer.on); };
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
    actions.showErrors = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Fehlerhafte Baustein-Dateien",
                                                library.errors().joinIntoString ("\n"));
    };
    const juce::Component::SafePointer<MainComponent> safe (this);
    actions.albumView = library.view() == Library::View::Album;
    actions.toggleView = [safe]   // the first line: runs after the menu has closed, then the menu opens again
    {
        if (safe == nullptr)
            return;
        safe->setMenuView (safe->library.view() == Library::View::Album ? Library::View::Own : Library::View::Album);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showPatternMenu(); });
    };

    // O inside any window of this menu: the menu closes, the completion below switches the view and opens it again.
    // The session number keeps a late key or completion of an earlier menu from acting on this one.
    const int session = ++patternMenuSession;
    menuViewSwitchWanted = false;
    menuKeyHeld = juce::KeyPress::isKeyCurrentlyDown ('o') || juce::KeyPress::isKeyCurrentlyDown ('O');   // opened by O: its repeats do nothing
    menuKeys.onKeyState = [safe]
    {
        if (safe != nullptr && ! juce::KeyPress::isKeyCurrentlyDown ('o') && ! juce::KeyPress::isKeyCurrentlyDown ('O'))
            safe->menuKeyHeld = false;   // released: the next O counts again
    };
    menuKeys.onKey = [safe, session] (const juce::KeyPress& k)
    {
        if (safe == nullptr || session != safe->patternMenuSession || k.getModifiers().isAnyModifierKeyDown()
            || juce::CharacterFunctions::toLowerCase (k.getTextCharacter()) != 'o')
            return false;
        if (! safe->menuViewSwitchWanted && ! safe->menuKeyHeld)   // once per press, not per repeat
        {
            safe->menuViewSwitchWanted = true;
            juce::MessageManager::callAsync ([] { juce::PopupMenu::dismissAllActiveMenus(); });
        }
        return true;
    };
    auto* look = dynamic_cast<HZLook*> (&getLookAndFeel());
    if (look != nullptr)
        look->onPopupMenuWindow = [safe, session] (juce::Component& window)
        {
            if (safe != nullptr && session == safe->patternMenuSession)
                window.addKeyListener (&safe->menuKeys);
        };
    buildPatternMenu (library, currentPattern, actions).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternButton),
        [safe, session] (int result)
        {
            if (safe == nullptr)
                return;
            const bool switching = session == safe->patternMenuSession && safe->menuViewSwitchWanted;
            if (result == 0 && ! switching)
                safe->cancelPreview();   // dismissed without a choice: back to the previous state (O: the menu comes back)
            if (session != safe->patternMenuSession)
                return;                  // a newer menu is open already
            if (auto* lf = dynamic_cast<HZLook*> (&safe->getLookAndFeel()))
                lf->onPopupMenuWindow = nullptr;
            if (safe->menuViewSwitchWanted)
            {
                safe->menuViewSwitchWanted = false;
                safe->setMenuView (safe->library.view() == Library::View::Album ? Library::View::Own : Library::View::Album);
                juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showPatternMenu(); });
            }
        });
}

void MainComponent::setMenuView (Library::View wanted)
{
    library.setView (wanted);
    saveSettings();
    updateViewDisc();
    refreshMenuFolders();   // its counts follow the view
}

void MainComponent::refreshMenuFolders()
{
    if (menuFoldersWindow != nullptr)
        if (auto* frame = dynamic_cast<GlowFrame*> (menuFoldersWindow->getContentComponent()))
            if (auto* panel = dynamic_cast<MenuFoldersPanel*> (frame->content()))
                panel->refresh();
}

void MainComponent::updateViewDisc()
{
    const bool album = library.view() == Library::View::Album;
    patternButton.setDisc (true, album, album ? "Ansicht: Band \xe2\x80\xba Album \xe2\x80\xba Song. Klick: eigene Ordner (im Menü: O)"_u
                                              : "Ansicht: eigene Ordner. Klick: Band \xe2\x80\xba Album \xe2\x80\xba Song (im Menü: O)"_u);
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
        song ? "Arrangement löschen"_u : "Baustein löschen"_u,
        "\"" + name + "\" in den Papierkorb verschieben?\n\n"_u
            + (song ? "Die verwendeten Bausteine bleiben erhalten."
                    : "Bausteine, die noch in einem Ablauf oder der Ablage verwendet werden, können nicht gelöscht werden."_u),
        "In Papierkorb", "Abbrechen", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this), id] (int result)
        {
            if (safe == nullptr || result != 1) return;
            auto error = openInEditor (safe->editorWindow.get(), id);
            if (error.isEmpty())
            {
                safe->cancelPreview();
                error = safe->removeLibraryEntry (id);
            }
            if (error.isNotEmpty())
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
        return "Dieser Baustein ist noch im aktuellen Ablauf oder in der Ablage. Zuerst dort entfernen.";

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
                                   [this] { selectPattern (-1); },
                                   [this] (const std::string& id) { previewPattern (library.indexOf (id)); },   // as in the menu
                                   [this] (const std::string& id) { return audition == Audition::Menu && menuPreviewIndex == library.indexOf (id) && control.play; });   // the mark means "sounds"
    quickSearch.onDismiss = [this] { cancelPreview(); };   // closed without a choice: back to what played before
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
        mixerKeys.onKey = [this] (const juce::KeyPress& k)
        {
            if (k.getKeyCode() == juce::KeyPress::spaceKey && ! k.getModifiers().isAnyModifierKeyDown())
            {
                togglePlay();   // Start / Pause, as in the main window
                return true;
            }
            if (k.getModifiers().isCommandDown() || juce::CharacterFunctions::toLowerCase (k.getTextCharacter()) != 'm')
                return false;
            toggleMixer();
            return true;
        };
        mixerWindow->addKeyListener (&mixerKeys);
    }
    mixerWindow->setVisible (true);
    mixerWindow->toFront (true);
}

void MainComponent::toggleMixer()
{
    if (mixerWindow != nullptr && mixerWindow->isVisible())
        mixerWindow->setVisible (false);   // M again: away with it
    else
        showMixer();
}

//==============================================================================
// Seeking inside the playing song/pattern

void MainComponent::seekToBar (int bar)
{
    const auto* info = view.plan;
    if (info == nullptr || bar < 0 || bar >= (int) info->bars.size())
        return;
    if (loopOn && loopFirst >= 0 && stripShowsSelectionPlan() && (bar < loopFirst || bar > loopLast))
        bar = loopFirst;   // with the loop on, playback never leaves its range: outside means its start

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
