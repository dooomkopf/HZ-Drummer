// MainComponent, part 4: the System windows (audio settings, badges anew, trash, log files, folders, menu folders)
// and deleting many entries at once from the folder view. Moved out of MainComponentMenus.cpp as they were.

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

void MainComponent::showAudioSettings()
{
    if (audioWindow == nullptr)
    {
        // "Abzeichen (a, b, c, …): …" with three real badges for a, b, c, so that one sees what is meant
        struct BadgeLine : juce::Component
        {
            juce::String rest;   // after the colon: how many are free
            void paint (juce::Graphics& g) override
            {
                const juce::Font font (juce::FontOptions (20.0f, juce::Font::bold));
                g.setFont (font);
                g.setColour (theme::textMain);
                auto r = getLocalBounds().toFloat();
                auto text = [&] (const juce::String& s)
                {
                    g.setColour (theme::textMain);   // a badge leaves its own colour behind
                    g.drawText (s, r.removeFromLeft (juce::GlyphArrangement::getStringWidth (font, s) + 1.0f), juce::Justification::centredLeft, false);
                };
                auto badge = [&] (int colour, int shape)
                {
                    theme::drawSourceBadge (g, r.removeFromLeft (24.0f).withSizeKeepingCentre (18.0f, 18.0f), colour, shape);
                };
                text ("Abzeichen (");
                badge (0, 0);   // a: one colour
                text (", ");
                badge (5, 7);   // b: a horizontal bar, like a flag
                text (", ");
                badge (9, 9);   // c: a dot
                text (", …): "_u);
                g.setColour (theme::textMain);
                g.setFont (juce::FontOptions (15.0f));   // the numbers on two lines, so nothing is cut off
                g.drawFittedText (rest, r.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
            }
        };

        // the audio settings plus a few lines about the system and the badges in use
        struct SystemPanel : juce::Component
        {
            AudioSettingsPanel selector;
            BadgeLine badgeLine;
            juce::Label info, version;
            juce::TextButton trash { "Papierkorb" }, logs { "Log-Dateien" }, structure { "Ordnerstruktur" }, menuFolders { "Menü-Ordner"_u },
                             quellen { "Quellen" };
            juce::Label badgeRuleLabel { {}, "Verteilung:" };
            juce::ComboBox badgeRule;
            juce::TextButton redistribute { "Neu verteilen" };
            juce::ToggleButton tooltips { "Tooltips" };
            explicit SystemPanel (juce::AudioDeviceManager& dm) : selector (dm)
            {
                addAndMakeVisible (tooltips);
                tooltips.setColour (juce::ToggleButton::textColourId, theme::textMain);
                tooltips.setColour (juce::ToggleButton::tickColourId, theme::textMain);
                addAndMakeVisible (selector);
                addAndMakeVisible (trash); addAndMakeVisible (logs); addAndMakeVisible (structure); addAndMakeVisible (menuFolders);
                addAndMakeVisible (quellen);
                quellen.setTooltip ("Weitere Bausteine von ihren Originalquellen laden und umwandeln, nur für den eigenen Gebrauch"_u);
                addAndMakeVisible (badgeLine);
                addAndMakeVisible (badgeRuleLabel); addAndMakeVisible (badgeRule); addAndMakeVisible (redistribute);
                badgeRuleLabel.setFont (juce::FontOptions (15.0f));
                badgeRule.addItem ("Erst eigene, dann Google", 1);
                badgeRule.addItem ("Gemischt", 2);
                addAndMakeVisible (info);
                info.setFont (juce::FontOptions (14.0f));
                info.setColour (juce::Label::textColourId, theme::textDim);
                info.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (version);
                version.setFont (juce::FontOptions (14.0f));
                version.setColour (juce::Label::textColourId, theme::textDim);
                version.setText ("HZ-Drummer v" HZ_VERSION, juce::dontSendNotification);
                setSize (640, ActiveGlow::height + AudioSettingsPanel::preferredHeight + 4 + 42 + 64 + 38 + 50 + 28 + 28);
            }
            void resized() override
            {
                auto r = getLocalBounds();
                auto last = r.removeFromBottom (28).reduced (12, 2);   // under everything, flush right
                tooltips.setSize (10, last.getHeight());
                tooltips.changeWidthToFitText();
                tooltips.setBounds (last.removeFromRight (tooltips.getWidth()));
                version.setBounds (r.removeFromBottom (28).reduced (12, 4));
                info.setBounds (r.removeFromBottom (50).reduced (12, 4));
                auto rule = r.removeFromBottom (38).reduced (12, 4);   // below the badge line: how new badges are given
                badgeRuleLabel.setBounds (rule.removeFromLeft (90));
                redistribute.setBounds (rule.removeFromRight (140)); rule.removeFromRight (8);
                badgeRule.setBounds (rule);
                badgeLine.setBounds (r.removeFromBottom (64).reduced (12, 4));
                selector.setBounds (r.withTrimmedTop (ActiveGlow::height).withHeight (AudioSettingsPanel::preferredHeight));
                auto actions = juce::Rectangle<int> (0, selector.getBottom() + 4, getWidth(), 42).reduced (12, 4);
                const int w = (actions.getWidth() - 4 * 8) / 5;   // five buttons across
                quellen.setBounds (actions.removeFromRight (w)); actions.removeFromRight (8);   // right-aligned
                menuFolders.setBounds (actions.removeFromRight (w)); actions.removeFromRight (8);
                structure.setBounds (actions.removeFromRight (w)); actions.removeFromRight (8);
                logs.setBounds (actions.removeFromRight (w)); actions.removeFromRight (8);
                trash.setBounds (actions.removeFromRight (w));
            }
        };
        auto* panel = new SystemPanel (deviceManager);
        const juce::Component::SafePointer<MainComponent> safe (this);
        panel->trash.onClick = [safe] { if (safe != nullptr) safe->showDataTools (0); };
        panel->logs.onClick = [safe] { if (safe != nullptr) safe->showDataTools (1); };
        panel->structure.onClick = [safe] { if (safe != nullptr) safe->showDataTools (2); };
        panel->menuFolders.onClick = [safe] { if (safe != nullptr) safe->showDataTools (3); };
        panel->quellen.onClick = [safe] { if (safe != nullptr) safe->showQuellen(); };
        panel->tooltips.setToggleState (tooltipsEnabled, juce::dontSendNotification);
        panel->tooltips.onClick = [safe, panel]
        {
            if (safe == nullptr)
                return;
            safe->setTooltipsEnabled (panel->tooltips.getToggleState());
            safe->saveSettings();
        };

        // badges: how much of the colour x mark space is taken, and whether any two patterns share one
        auto badgeText = [safe]
        {
            const auto st = safe->badges.stats (safe->library.all());
            return juce::String (st.total - st.used) + " frei von " + juce::String (st.total)
                   + (st.shared > 0 ? "   (" + juce::String (st.shared) + " Bausteine teilen sich eins!)" : juce::String());
        };
        panel->badgeLine.rest = badgeText();
        panel->badgeLine.repaint();
        panel->badgeRule.setSelectedId (badges.rule() == BadgeRegistry::Rule::Mixed ? 2 : 1, juce::dontSendNotification);
        panel->badgeRule.onChange = [safe, panel]
        {
            if (safe != nullptr)
                safe->badges.setRule (panel->badgeRule.getSelectedId() == 2 ? BadgeRegistry::Rule::Mixed : BadgeRegistry::Rule::OwnFirst);
        };
        panel->redistribute.onClick = [safe, panel, badgeText]
        {
            if (safe == nullptr)
                return;
            safe->redistributeBadges();
            panel->badgeLine.rest = badgeText();
            panel->badgeLine.repaint();
        };
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

void MainComponent::redistributeBadges()
{
    // the old assignment first into the cache folder (the last ten stay), then every pattern by the rule;
    // without a copy nothing changes
    const auto file = Library::songFolder().getSiblingFile ("abzeichen.json");
    const auto backups = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/abzeichen");
    if (file.existsAsFile())
    {
        const auto now = juce::Time::getCurrentTime();
        const auto copy = backups.getNonexistentChildFile ("abzeichen-" + now.formatted ("%Y%m%d-%H%M%S-")
                                                           + juce::String (now.getMilliseconds()).paddedLeft ('0', 3), ".json", true);
        if (! backups.createDirectory() || ! file.copyFileTo (copy))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Neu verteilen",
                "Die bisherige Zuordnung ließ sich nicht sichern, deshalb bleibt alles, wie es ist.\n\n"_u + backups.getFullPathName());
            return;
        }
        auto old = backups.findChildFiles (juce::File::findFiles, false, "abzeichen-*.json");
        old.sort();   // the names carry the time: oldest first
        for (int i = 0; i < old.size() - 10; ++i)
            old.getReference (i).deleteFile();
    }
    badges.redistribute (library.all(), (std::uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);   // rolled anew each time

    // everything that shows a badge: song strips (built with the library), Finden, sequence, Ablage
    const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
    library.reload();
    currentPattern = library.indexOf (selectedId);
    sendCurrentPlan();
    updateButtons();
    if (editorWindow != nullptr)
        editorWindow->getArranger().badgesChanged();
    repaint();
}

void MainComponent::showDataTools (int tab)
{
    auto& window = tab == 0 ? trashWindow : tab == 1 ? logsWindow : tab == 2 ? foldersWindow : menuFoldersWindow;
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
        else if (tab == 3)
        {
            auto* menuFolders = new MenuFoldersPanel (library);
            menuFolders->ghostToggle.setToggleState (ghostEnabled, juce::dontSendNotification);
            menuFolders->ghostSeconds.setValue (ghostSeconds, juce::dontSendNotification);
            menuFolders->ghostSeconds.setEnabled (ghostEnabled);
            menuFolders->onGhostChanged = [safe = juce::Component::SafePointer<MainComponent> (this), menuFolders]
            {
                if (safe == nullptr)
                    return;
                safe->ghostEnabled = menuFolders->ghostToggle.getToggleState();
                safe->ghostSeconds = menuFolders->ghostSeconds.getValue();
                safe->ghostMenu.setHoldSeconds (safe->ghostSeconds);
                safe->saveSettings();
            };
            panel = menuFolders;
        }
        else
        {
            auto* folders = new FolderPanel (library);
            folders->onDeleteEntries = [safe = juce::Component::SafePointer<MainComponent> (this)] (const std::vector<std::string>& ids)
            {
                return safe != nullptr ? safe->removeLibraryEntries (ids) : juce::String();
            };
            panel = folders;
        }
        panel->setSize (tab == 1 ? 1000 : tab == 2 ? 980 : tab == 3 ? 560 : 820, tab == 0 ? 500 : tab == 3 ? 800 : 640);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (new GlowFrame (panel));
        options.dialogTitle = tab == 0 ? "Papierkorb" : tab == 1 ? "Log-Dateien" : tab == 3 ? "Menü-Ordner"_u : juce::String ("Ordnerstruktur");
        options.dialogBackgroundColour = theme::panel; options.useNativeTitleBar = true; options.resizable = true;
        window.reset (options.create());
    }
    auto* frame = dynamic_cast<GlowFrame*> (window->getContentComponent());
    auto* content = frame != nullptr ? frame->content() : window->getContentComponent();
    if (auto* trash = dynamic_cast<TrashPanel*> (content)) trash->refresh();
    if (auto* logs = dynamic_cast<LogPanel*> (content)) logs->refresh();
    if (auto* folderPanel = dynamic_cast<FolderPanel*> (content)) folderPanel->refresh();
    if (auto* menuPanel = dynamic_cast<MenuFoldersPanel*> (content)) menuPanel->refresh();
    window->setVisible (true); window->toFront (true);
}

juce::String MainComponent::removeLibraryEntries (const std::vector<std::string>& ids)
{
    // as removeLibraryEntry for each, with one reload at the end: thirty entries must not mean thirty reloads
    juce::StringArray problems;
    std::map<std::string, juce::String> names;
    std::vector<std::string> go;
    for (const auto& id : ids)
    {
        const int index = library.indexOf (id);
        if (index < 0)
            continue;
        names[id] = juce::String (library.get (index).name);
        if (const auto error = openInEditor (editorWindow.get(), id); error.isNotEmpty())
            problems.add (names[id] + ": " + error);
        else if (library.isUserPattern (index) && editorWindow != nullptr && editorWindow->getArranger().usesPattern (id))
            problems.add (names[id] + ": noch im aktuellen Ablauf oder in der Ablage");
        else
            go.push_back (id);
    }
    cancelPreview();
    const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
    for (const auto& [id, error] : library.removeMany (go))
    {
        if (error.isNotEmpty())
        {
            problems.add (names[id] + ": " + error);
            continue;
        }
        badges.release (id);   // a restored pattern gets a badge again when the library reloads
        if (editorWindow != nullptr)
            editorWindow->getArranger().songDeleted (id);
    }
    currentPattern = library.indexOf (selectedId);
    quickSearch.close();
    sendCurrentPlan();
    updateButtons();
    saveSettings();
    if (editorWindow != nullptr)
        editorWindow->getArranger().libraryChanged();
    if (problems.isEmpty())
        return {};
    const int shown = juce::jmin (8, problems.size());
    return juce::String (problems.size()) + juce::String::fromUTF8 (" nicht gelöscht:\n") + problems.joinIntoString ("\n", 0, shown)
           + (problems.size() > shown ? "\n…"_u : juce::String());
}
