#include "app/MainComponent.h"

#include "app/Text.h"
#include "app/views/NeonGlow.h"
#include "app/views/Theme.h"
#include "engine/MeterCatalog.h"

#include <utility>

//==============================================================================
// Setup

MainComponent::MainComponent()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "HZ-Drummer";
    opts.filenameSuffix = ".settings";
    opts.folderName = ".config/HZ-Drummer";   // Linux: relative to ~
    opts.osxLibrarySubFolder = "Application Support";
    opts.millisecondsBeforeSaving = 1000;
    appProperties.setStorageParameters (opts);

    Library::installStartPackage();   // first start only: community sets, example arrangements, folder tree
    badges.load (Library::songFolder().getSiblingFile ("abzeichen.json"));
    BadgeRegistry::setGlobal (&badges);
    library.onReloaded = [this] (const std::vector<hz::Pattern>& all) { badges.ensure (all); refreshFolders(); };
    library.reload();
    loadSettings();
    kits = std::make_unique<KitManager> (KitManager::findDefaultKit());

    for (auto* b : { &playButton, &tapButton, &audioButton, &editButton, &mixerButton })
        addAndMakeVisible (*b);
    playButton.onClick  = [this] { togglePlay(); };
    playButton.getProperties().set ("transport", true);   // drawn by HZLook
    tapButton.onClick   = [this] { tapTempo(); };
    tapButton.getProperties().set ("tap", true);          // drawn by HZLook
    audioButton.onClick = [this] { showAudioSettings(); };
    audioButton.getProperties().set ("system", true);     // drawn by HZLook
    editButton.onClick  = [this] { openEditor(); };
    mixerButton.onClick = [this] { showMixer(); };
    mixerButton.setTooltip ("Mixer: Lautstärke der Spuren (M)");
    mixerButton.getProperties().set ("mixer", true);      // drawn by HZLook
    addAndMakeVisible (countInButton);
    countInButton.onClick = [this] { cycleCountIn(); };
    countInButton.getProperties().set ("metronome", true);   // drawn by HZLook
    countInButton.setTooltip ("Ein Takt Klicks vor dem Start: aus, nur am Anfang des Arrangements, oder vor jedem Start"_u);
    updateCountInButton();
    for (auto* b : { &playButton, &tapButton, &mixerButton, &audioButton, &countInButton })
        b->getProperties().set ("bold", true);   // only the first row is bold (HZLook::getTextButtonFont)

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (30.0, 320.0, 1.0);
    tempoSlider.setValue (control.tempoTenths / 10.0, juce::dontSendNotification);
    tempoSlider.getProperties().set ("pill", true);   // value box drawn by HZLook as a pill
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.onValueChange = [this] { setTempo (tempoSlider.getValue()); };

    addAndMakeVisible (patternButton);
    patternButton.onClick = [this] { showPatternMenu(); };
    addAndMakeVisible (searchButton);
    searchButton.onClick = [this] { openSearch(); };
    searchButton.setTooltip ("Bausteine und Arrangements suchen (Strg+K oder /)");
    addAndMakeVisible (deleteEntryButton);
    deleteEntryButton.setTooltip ("Geladenen Eintrag in den Papierkorb verschieben");
    deleteEntryButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff713a40));
    deleteEntryButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffeeee));
    deleteEntryButton.onClick = [this]
    {
        if (patternMode() && (library.isUserPattern (currentPattern) || library.isSong (currentPattern))
            && ! editing.has_value() && ! arranging.has_value())
            confirmDeleteLibraryEntry (library.get (currentPattern).id);
    };
    addAndMakeVisible (renameEntryButton);
    renameEntryButton.setTooltip ("Geladenen Eintrag umbenennen (wie Rechtsklick im Baustein-Menü)"_u);
    renameEntryButton.onClick = [this]
    {
        if (patternMode())
            askRenameEntry (library.get (currentPattern).id);
    };
    addAndMakeVisible (meterButton);
    meterButton.onClick = [this] { showMeterMenu(); };

    addAndMakeVisible (soundBox);
    soundBox.getProperties().set ("pill", true);   // drawn by HZLook like the buttons beside it
    soundBox.addItemList ({ "Klick", "Bassdrum", "Snare", "Bass + Snare" }, 1);
    soundBox.setSelectedItemIndex ((int) sound, juce::dontSendNotification);
    soundBox.onChange = [this] { selectSound (soundBox.getSelectedItemIndex()); };

    for (auto* l : { &helpLabel, &kitLabel })
    {
        addAndMakeVisible (*l);
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::centred);
    }
    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme::textMain);
    titleLabel.setInterceptsMouseClicks (false, false);

    helpLabel.setText ("Leertaste Start/Stop   |   Strg+K Suchen   |   F1 Hilfe", juce::dontSendNotification);
    helpLabel.setFont (juce::FontOptions (19.0f));
    kitLabel.setFont (juce::FontOptions (18.0f));

    for (juce::Component* v : { (juce::Component*) &statusPanels, (juce::Component*) &songStrip, (juce::Component*) &beatDisplay })
        addAndMakeVisible (v);
    statusPanels.setInterceptsMouseClicks (false, false);
    beatDisplay.setInterceptsMouseClicks (false, false);
    songStrip.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    songStrip.onSeek = [this] (int bar) { seekToBar (bar); };
    songStrip.onLoopRange = [this] (int first, int last) { setLoopRange (first, last); };
    songStrip.onLoopClear = [this] { clearLoop(); };
    for (auto* b : { &loopButton, &trainerButton })   // quiet: they sit beside the title
    {
        addAndMakeVisible (*b);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff26262c));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1f6b60));
        b->setColour (juce::TextButton::textColourOffId, theme::textDim);
        b->setColour (juce::TextButton::textColourOnId, theme::textMain);
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
    }
    loopButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd4b13a));   // yellowish, as the range in the strip
    loopButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1c1c20));
    loopButton.setTooltip ("Loop an/aus (L). Den Bereich ganzer Takte mit Shift + Ziehen in der Takt-Leiste setzen, Doppelklick darauf löscht ihn."_u);
    loopButton.onClick = [this] { toggleLoop(); };
    trainerButton.setTooltip ("Speed-Trainer (S): von, bis, Inkrement und Wiederholungen pro Inkrement einstellen"_u);
    trainerButton.onClick = [this] { showTrainer(); };
    statusPanels.setView (&view);
    songStrip.setView (&view);
    beatDisplay.setView (&view);

    setupKeys();
    addChildComponent (helpOverlay);
    addAndMakeVisible (activeGlow);
    addChildComponent (quickSearch);
    addChildComponent (ghostMenu);
    ghostMenu.onAlpha = [this] (float a)   // the ghost takes their place while it shows
    {
        for (auto* c : { (juce::Component*) &songStrip, (juce::Component*) &statusPanels, (juce::Component*) &beatDisplay,
                         (juce::Component*) &kitLabel, (juce::Component*) &helpLabel })
            c->setAlpha (1.0f - a);
    };

    // keyboard focus stays with the main window, so space & co. never end up in a widget
    for (juce::Component* c : { (juce::Component*) &playButton, (juce::Component*) &tapButton, (juce::Component*) &audioButton,
                                (juce::Component*) &tempoSlider, (juce::Component*) &meterButton,
                                (juce::Component*) &patternButton, (juce::Component*) &soundBox, (juce::Component*) &editButton,
                                (juce::Component*) &mixerButton, (juce::Component*) &searchButton, (juce::Component*) &deleteEntryButton,
                                (juce::Component*) &renameEntryButton, (juce::Component*) &countInButton })
    {
        c->setWantsKeyboardFocus (false);
        c->setMouseClickGrabsKeyboardFocus (false);
    }
    setWantsKeyboardFocus (true);

    updateButtons();
    sendCurrentPlan();
    setSize (1000, 700);

    // restore the audio device; on the very first start prefer 48 kHz (PipeWire's native rate,
    // the kits are prepared for it)
    auto savedAudio = appProperties.getUserSettings()->getXmlValue ("audioDevice");
    setAudioChannels (0, 2, savedAudio.get());
    if (savedAudio == nullptr)
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        if (std::abs (setup.sampleRate - 48000.0) > 0.5)
        {
            setup.sampleRate = 48000.0;
            deviceManager.setAudioDeviceSetup (setup, true);
        }
    }

    startTimerHz (60);
}

MainComponent::~MainComponent()
{
    BadgeRegistry::setGlobal (nullptr);
    editorWindow = nullptr;
    mixerWindow = nullptr;
    trashWindow = nullptr;
    quellenWindow = nullptr;   // stops a running import first
    logsWindow = nullptr;
    audioWindow = nullptr;
    saveSettings();
    shutdownAudio();   // afterwards the audio thread no longer touches kits or plans
    kits = nullptr;
}

void MainComponent::loadSettings()
{
    auto& props = *appProperties.getUserSettings();
    meters.load (props);
    currentMeter = juce::jmax (0, meters.fromKey (props.getValue ("meter", "4/4")));
    control.tempoTenths = (std::uint16_t) juce::jlimit (300, 3200, props.getIntValue ("tempoTenths", 1200));
    sound = (MetronomeSound) juce::jlimit (0, 3, props.getIntValue ("sound", (int) MetronomeSound::Kick));
    const auto even = [] (double bpm) { return 2.0 * std::round (bpm / 2.0); };   // Von and Bis move in steps of two
    trainer.to = juce::jlimit (32.0, 320.0, even (props.getDoubleValue ("trainerTo", 140.0)));
    trainer.from = juce::jlimit (30.0, trainer.to - 2.0, even (props.getDoubleValue ("trainerFrom", 100.0)));   // always below "Bis"
    trainer.step = juce::jlimit (1.0, 40.0, props.getDoubleValue ("trainerIncrement", 1.0));
    trainer.repeats = juce::jlimit (1, 5, props.getIntValue ("trainerRepeatsPerIncrement", 1));
    trainerSlots = trainerslots::load (trainerslots::file());
    ghostEnabled = props.getBoolValue ("ghostMenu", true);
    setTooltipsEnabled (props.getBoolValue ("tooltips", true));
    ghostSeconds = juce::jlimit (0.5, 5.0, props.getDoubleValue ("ghostSeconds", 1.0));
    ghostMenu.setHoldSeconds (ghostSeconds);
    countIn = (CountIn) juce::jlimit (0, 2, props.getIntValue ("countIn", 0));
    beatDisplay.setShowHits (props.getBoolValue ("showBeats", true));
    library.favoritesFromString (props.getValue ("patternFavorites"));
    currentPattern = library.indexOf (props.getValue ("pattern").toStdString());

    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        const juce::String k (mix::channelKey (ch));
        mixSettings[(size_t) ch] = { (float) props.getDoubleValue ("mix_" + k + "_db", mix::defaultDb (ch)), props.getBoolValue ("mix_" + k + "_mute", false), false };
    }
    engine.setMixState (mix::pack (mixSettings));
}

void MainComponent::saveSettings()
{
    auto& props = *appProperties.getUserSettings();
    props.setValue ("meter", MeterLibrary::key (meters.get (currentMeter)));
    props.setValue ("tempoTenths", (int) control.tempoTenths);
    props.setValue ("sound", (int) sound);
    props.setValue ("trainerFrom", trainer.from);
    props.setValue ("trainerTo", trainer.to);
    props.setValue ("trainerIncrement", trainer.step);
    props.setValue ("trainerRepeatsPerIncrement", trainer.repeats);
    props.removeValue ("trainerStep");      // the old keys held the old defaults (5 and 2)
    props.removeValue ("trainerRepeats");
    props.setValue ("ghostMenu", ghostEnabled);
    props.setValue ("tooltips", tooltipsEnabled);
    props.setValue ("ghostSeconds", ghostSeconds);
    props.setValue ("countIn", (int) countIn);
    props.setValue ("pattern", patternMode() ? juce::String (library.get (currentPattern).id) : juce::String());
    props.setValue ("patternFavorites", library.favoritesToString());
    props.setValue ("showBeats", beatDisplay.getShowHits());
    meters.save (props);

    if (auto xml = deviceManager.createStateXml())
        props.setValue ("audioDevice", xml.get());
    props.saveIfNeeded();
}

//==============================================================================
// Audio callback

void MainComponent::prepareToPlay (int, double sampleRate)
{
    int latency = 0;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        latency = device->getOutputLatencyInSamples();
    engine.prepare (sampleRate, latency);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    auto* buffer = info.buffer;
    engine.process (buffer->getWritePointer (0, info.startSample),
                    buffer->getNumChannels() > 1 ? buffer->getWritePointer (1, info.startSample) : nullptr,
                    info.numSamples);
}

//==============================================================================
// Transport and selection

void MainComponent::publishControl()
{
    engine.setControl (control);
    // Keep text and artwork in sync for every playback path, including count-in and previews.
    playButton.getProperties().set ("playing", control.play);
    playButton.setButtonText (control.play ? "Pause" : "Start");
}

void MainComponent::togglePlay()
{
    if (! control.play && startWithCountIn())
        return;
    const bool abandonedCountIn = control.play && countInTag >= 0;
    countInTag = -1;
    control.play = ! control.play;
    ++control.generation;
    publishControl();
    if (abandonedCountIn)
        sendCurrentPlan();   // stopped during the clicks: the selection must be what starts next time

    // pausing keeps the last shown position (marker stays); resuming continues from there
    pendingDisplay.clear();
    updateView();
}

void MainComponent::setTempo (double bpm)
{
    if (trainer.on && ! trainerSettingTempo)
        setTrainerOn (false);   // a tempo of one's own ends the speed trainer
    bpm = juce::jlimit (30.0, 320.0, std::round (bpm));
    control.tempoTenths = (std::uint16_t) (bpm * 10.0);
    publishControl();
    appProperties.getUserSettings()->setValue ("tempoTenths", (int) control.tempoTenths);
    tempoSlider.setValue (bpm, juce::dontSendNotification);
    updateView();
}

void MainComponent::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.empty() && now - tapTimes.back() > 2000.0)
        tapTimes.clear();

    tapTimes.push_back (now);
    if (tapTimes.size() > 5)
        tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() >= 2)
        setTempo (60000.0 * (double) (tapTimes.size() - 1) / (tapTimes.back() - tapTimes.front()));
}

void MainComponent::selectMeter (int index)
{
    if (index < 0 || index >= meters.size())
        return;

    // choosing a meter means: metronome in that meter; previews and the arranger's sequence end
    if (index != currentMeter || patternMode())
    {
        clearLoop();   // the range belongs to what was selected before
        activeTrainerSlot = -1;
    }
    currentMeter = index;
    currentPattern = -1;
    arranging.reset();
    if (audition == Audition::Arranger || audition == Audition::Menu)
    {
        editing.reset();
        audition = Audition::None;
        menuPreviewIndex = -1;
    }
    meters.noteUsed (index);
    sendCurrentPlan();
    updateButtons();
    saveSettings();
    recordHistory();
}

void MainComponent::selectSound (int index)
{
    sound = (MetronomeSound) juce::jlimit (0, 3, index);
    soundBox.setSelectedItemIndex ((int) sound, juce::dontSendNotification);
    if (! patternMode())
        sendCurrentPlan();
    saveSettings();
}

void MainComponent::selectPattern (int index)
{
    if (index >= library.size())
        return;

    if (index != currentPattern)
    {
        clearLoop();                            // the range belongs to what was selected before
        activeTrainerSlot = -1;                 // ... and so does the save slot in effect
    }
    currentPattern = index;
    arranging.reset();                          // a selection here ends the arranger's playback
    if (audition == Audition::Arranger || audition == Audition::Menu)
    {
        editing.reset();                        // a chosen preview simply becomes the selection (plays on seamlessly)
        audition = Audition::None;
        menuPreviewIndex = -1;
    }
    if (patternMode())
        setTempo (library.get (index).tempo);   // take over the recommended tempo
    if (editorWindow != nullptr && patternMode())
        loadIntoEditor (index);                 // the editor follows the selection
    sendCurrentPlan();
    updateButtons();
    saveSettings();
    recordHistory();
}

void MainComponent::selectPatternById (const std::string& id)
{
    if (const int i = library.indexOf (id); i >= 0)
        selectPattern (i);
}

void MainComponent::stepPattern (int dir, bool favoritesOnly)
{
    const int n = library.size();
    if (n == 0)
        return;

    int i = currentPattern < 0 ? (dir > 0 ? -1 : 0) : currentPattern;
    for (int tries = 0; tries < n; ++tries)
    {
        i = ((i + dir) % n + n) % n;
        if (! favoritesOnly || library.isFavorite (library.get (i).id))
        {
            selectPattern (i);
            return;
        }
    }
}

void MainComponent::stepCurrent (int dir, bool favoritesOnly)
{
    if (patternMode())
        stepPattern (dir, favoritesOnly);
    else
        selectMeter (favoritesOnly ? meters.stepFavorite (currentMeter, dir) : meters.step (currentMeter, dir));
}

void MainComponent::stepStyle (int dir)
{
    const int n = library.size();
    if (n == 0)
        return;

    // entries are sorted by style: jump to the first entry of the next/previous style
    auto styleOf = [this] (int i) { const auto& t = library.get (i).tags; return t.empty() ? std::string() : t.front(); };
    if (! patternMode())
    {
        selectPattern (dir > 0 ? 0 : n - 1);
        if (dir > 0)
            return;
    }

    const int i = currentPattern;
    const auto start = styleOf (i);
    if (dir > 0)
    {
        for (int k = 1; k < n; ++k)
            if (const int j = (i + k) % n; styleOf (j) != start)
                return selectPattern (j);
        return;
    }

    // backwards: to the start of the current style; if already there, to the start of the previous one
    int first = i;
    while (first > 0 && styleOf (first - 1) == start)
        --first;
    if (first == i)
    {
        first = (i - 1 + n) % n;
        const auto prev = styleOf (first);
        while (first > 0 && styleOf (first - 1) == prev)
            --first;
    }
    selectPattern (first);
}

//==============================================================================
// Browsing with the keys, as the pattern menu is ordered

std::vector<BrowseFolder> MainComponent::browseOrder() const
{
    std::vector<BrowseArea> areas;
    for (auto area : { FolderTree::Songs, FolderTree::Patterns, FolderTree::Bundled })   // as buildPatternMenu
        areas.push_back ({ library.folders (area), [this, area] (const juce::String& folder) { return library.folderHidden (area, folder); } });
    return browseFolders (areas);
}

namespace
{
    const char* areaTitle (int area) { return area == 0 ? "Arrangements" : area == 1 ? "Bausteine" : "Bibliothek"; }

    // the tree down to a folder: its folders from the top; the area's own name only where there is no folder
    juce::StringArray treeOf (int area, const FolderPath& path)
    {
        return path.isEmpty() ? juce::StringArray (areaTitle (area)) : path;
    }

    juce::String counter (int at, int total) { return juce::String (at + 1) + " / " + juce::String (total); }
}

void MainComponent::showGhost (const GhostMenu::Content& content)
{
    if (! ghostEnabled)
        return;
    // left-aligned just below the title, as far down as the window goes; what lies beneath fades (see the constructor)
    const int top = titleLabel.getBottom() + 4;
    ghostMenu.show (content, { titleLabel.getX(), top, getWidth() - titleLabel.getX() - 24, getHeight() - top });
}

void MainComponent::browseInFolder (int dir)
{
    const auto order = browseOrder();
    const auto id = library.get (currentPattern).id;
    const int f = browseFolderOf (order, id);
    if (f < 0)
        return;
    const auto& folder = order[(size_t) f];
    int at = (int) (std::find (folder.ids.begin(), folder.ids.end(), id) - folder.ids.begin());
    if (const int to = at + dir; dir != 0 && to >= 0 && to < (int) folder.ids.size())   // at an end: it stops, the ghost shows why
        if (const int i = library.indexOf (folder.ids[(size_t) to]); i >= 0)
        {
            selectPattern (i);
            at = to;
        }

    GhostMenu::Content c;
    c.path = treeOf (folder.area, folder.path);
    for (const auto& entry : folder.ids)
        if (const int i = library.indexOf (entry); i >= 0)
            c.list.add (folders::shortName (juce::String (library.get (i).name), folder.path));
    c.current = at;
    c.count = counter (at, (int) folder.ids.size());
    showGhost (c);
}

void MainComponent::browseFolder (int dir)
{
    const auto order = browseOrder();
    const int from = patternMode() ? browseFolderOf (order, library.get (currentPattern).id) : -1;
    const int to = nextBrowseFolder (order, from >= 0 ? from : (dir > 0 ? -1 : (int) order.size()), dir);
    if (to >= 0)
        if (const int i = library.indexOf (order[(size_t) to].ids.front()); i >= 0)
            selectPattern (i);
    const int shownFolder = to >= 0 ? to : from;
    if (shownFolder < 0)
        return;

    // the tree down to the parent folder, below it the folders beside the one now in hand (visible ones,
    // with the number of entries in and below them); entries in no folder appear as "ohne Ordner"
    const auto& folder = order[(size_t) shownFolder];
    auto parent = folder.path;
    if (! parent.isEmpty())
        parent.remove (parent.size() - 1);
    const int depth = parent.size();
    GhostMenu::Content c;
    c.path = treeOf (folder.area, parent);
    std::vector<int> counts;
    auto add = [&] (const juce::String& name, int entries)
    {
        int k = c.list.indexOf (name);
        if (k < 0)
        {
            c.list.add (name);
            counts.push_back (0);
            k = c.list.size() - 1;
        }
        counts[(size_t) k] += entries;
    };
    const auto noFolder = juce::String ("ohne Ordner");
    for (const auto& stop : order)
        if (stop.area == folder.area && ! stop.hidden)
        {
            if (stop.path.size() > depth && folders::startsWith (stop.path, parent))
                add (stop.path[depth], (int) stop.ids.size());
            else if (depth == 0 && stop.path.isEmpty())
                add (noFolder, (int) stop.ids.size());
        }
    c.current = c.list.indexOf (folder.path.size() > depth ? folder.path[depth] : noFolder);
    for (int k = 0; k < c.list.size(); ++k)
        c.list.set (k, c.list[k] + "  (" + juce::String (counts[(size_t) k]) + ")");
    int visible = 0, index = -1;
    for (int k = 0; k < (int) order.size(); ++k)
        if (! order[(size_t) k].hidden)
        {
            if (k == shownFolder)
                index = visible;
            ++visible;
        }
    c.count = index >= 0 ? "Ordner " + counter (index, visible) : juce::String();
    showGhost (c);
}

void MainComponent::browseFavorites (int dir)
{
    // in the order of the menu's Favoriten section
    std::vector<int> favorites;
    for (int i = 0; i < library.size(); ++i)
        if (library.isFavorite (library.get (i).id))
            favorites.push_back (i);
    if (favorites.empty())
        return;
    const auto it = std::find (favorites.begin(), favorites.end(), currentPattern);
    int at = it != favorites.end() ? (int) (it - favorites.begin()) : -1;
    const int to = at >= 0 ? at + dir : (dir > 0 ? 0 : (int) favorites.size() - 1);
    if (to >= 0 && to < (int) favorites.size())
    {
        selectPattern (favorites[(size_t) to]);
        at = to;
    }

    GhostMenu::Content c;
    c.path.add ("Favoriten");
    for (int i : favorites)
        c.list.add (juce::String (library.get (i).name));
    c.current = at;
    c.count = at >= 0 ? counter (at, (int) favorites.size()) : juce::String();
    showGhost (c);
}

void MainComponent::browseMeters (int dir, bool favoritesOnly)
{
    const int next = dir == 0 ? currentMeter : favoritesOnly ? meters.stepFavorite (currentMeter, dir) : meters.step (currentMeter, dir);
    const bool wrapped = dir == 0 || next < 0 || (dir > 0 ? next <= currentMeter : next >= currentMeter);
    if (! wrapped)   // at an end: it stops
        selectMeter (next);

    GhostMenu::Content c;
    c.path.add (favoritesOnly ? "Taktart-Favoriten" : "Taktarten");
    for (int i = 0; i < meters.size(); ++i)
        if (! favoritesOnly || meters.isFavorite (i))
        {
            if (i == currentMeter)
                c.current = c.list.size();
            c.list.add (meters.label (i));
        }
    c.count = c.current >= 0 ? counter (c.current, c.list.size()) : juce::String();
    showGhost (c);
}

//==============================================================================
// Verlauf: back and forward through the entries chosen so far

void MainComponent::recordHistory()
{
    if (historyMoving)
        return;
    const HistoryEntry entry { patternMode() ? library.get (currentPattern).id : std::string(), patternMode() ? -1 : currentMeter };
    if (historyAt >= 0 && history[(size_t) historyAt].patternId == entry.patternId && history[(size_t) historyAt].meter == entry.meter)
        return;
    history.resize ((size_t) (historyAt + 1));   // a new choice drops what lay ahead, as in a browser
    history.push_back (entry);
    if (history.size() > 50)
        history.erase (history.begin());
    historyAt = (int) history.size() - 1;
}

void MainComponent::moveHistory (int dir)
{
    for (int i = historyAt + dir; i >= 0 && i < (int) history.size(); i += dir)
    {
        const auto& entry = history[(size_t) i];
        const int index = entry.patternId.empty() ? -1 : library.indexOf (entry.patternId);
        if (! entry.patternId.empty() && index < 0)
            continue;   // deleted meanwhile: passed over
        historyAt = i;
        historyMoving = true;
        if (index >= 0)
            selectPattern (index);
        else
            selectMeter (entry.meter);
        historyMoving = false;
        if (patternMode())   // where it is, as when browsing
            browseInFolder (0);
        else
            browseMeters (0, false);
        return;
    }
}

//==============================================================================
// Loop and speed trainer

void MainComponent::setTooltipsEnabled (bool on)
{
    tooltipsEnabled = on;
    if (! on)
        tooltipWindow = nullptr;   // without a tooltip window JUCE shows no tooltips, in no window
    else if (tooltipWindow == nullptr)
        tooltipWindow = std::make_unique<juce::TooltipWindow> (nullptr, 700);
}

bool MainComponent::stripShowsSelection() const
{
    const auto* selection = plans.find (selectionTag);
    return view.plan != nullptr && selection != nullptr && view.plan->patternId == selection->patternId
        && ! workshopOpen() && audition == Audition::None;
}

void MainComponent::showLoopAndTrainer()
{
    // "Bearbeiten" glows while the workshop is open (the halo breathes: repainted with every tick)
    const bool open = editorWindow != nullptr;
    if (open != (bool) editButton.getProperties()["glow"])
    {
        editButton.getProperties().set ("glow", open);
        editButton.repaint();
        repaint (editButton.getBounds().expanded (48));
    }
    else if (open)
        repaint (editButton.getBounds().expanded (48));

    const bool workshop = workshopOpen();
    loopButton.setVisible (! workshop && view.spans != nullptr);   // a range is picked in the strip
    trainerButton.setVisible (! workshop);
    if (workshop != std::exchange (workshopWasOpen, workshop))
    {
        if (workshop)
        {
            // the workshop moves the position at will: loop and trainer go off (the range stays), so that
            // switching them on again afterwards is unambiguous (both jump to the range start, see below)
            loopOn = false;
            if (trainer.on)
                setTrainerOn (false);
        }
        sendLoop();
    }
}

bool MainComponent::stripShowsSelectionPlan() const
{
    return stripShowsSelection() && (view.hasPosition() ? shown.planTag : plans.latestTag()) == selectionTag;
}

void MainComponent::setLoopRange (int first, int last)
{
    // only in the bars of the selection's latest plan: not while the strip still shows the one before or an
    // older version of it (a switch waits for the bar end), so that range and jump come from the same bars
    const auto* selection = plans.find (selectionTag);
    if (! stripShowsSelectionPlan() || first < 0 || last < first || last >= (int) selection->bars.size())
        return;
    loopFirst = first;
    loopLast = last;
    loopOn = true;
    loopPatternId = selection->patternId;
    sendLoop();
    if (view.bar < first || view.bar > last)   // outside the range: straight to its start
        seekToBar (first);
}

void MainComponent::clearLoop()
{
    if (loopFirst < 0 && ! loopOn)
        return;
    loopFirst = loopLast = -1;
    loopOn = false;
    sendLoop();
}

void MainComponent::toggleLoop()
{
    if (loopFirst < 0)
        return;   // no range yet: the tooltip says how to pick one
    loopOn = ! loopOn;
    sendLoop();
    if (loopOn && loopFirst >= 0 && stripShowsSelectionPlan() && (view.bar < loopFirst || view.bar > loopLast))
        seekToBar (loopFirst);
}

void MainComponent::sendLoop()
{
    // The range belongs to the selection. A count-in, a preview or the workshop only suspend it; another
    // selection ends it, and so do fewer bars than it starts at (a shorter end is cut back).
    const auto* selection = plans.find (selectionTag);
    if (loopFirst >= 0 && (selection == nullptr || selection->patternId != loopPatternId
                           || loopFirst >= (int) selection->bars.size()))
    {
        loopFirst = loopLast = -1;
        loopOn = false;
    }
    if (loopFirst >= 0)
        loopLast = juce::jmin (loopLast, (int) selection->bars.size() - 1);
    if (loopFirst >= 0 && loopOn && ! workshopOpen() && audition == Audition::None)
        engine.setLoopRange (selection->bars[(size_t) loopFirst].start,
                             selection->bars[(size_t) loopLast].start + selection->bars[(size_t) loopLast].length);
    else
        engine.clearLoopRange();
    showLoop();
}

void MainComponent::showLoop()
{
    // drawn only while on, and only over the selection's own bars
    songStrip.setLoopRange (loopFirst, loopLast, loopOn && loopFirst >= 0 && stripShowsSelection());
    loopButton.setToggleState (loopOn && loopFirst >= 0, juce::dontSendNotification);
}

void MainComponent::showTrainer()
{
    auto panel = std::make_unique<TrainerPanel> (trainer);
    panel->onChange = [safe = juce::Component::SafePointer<MainComponent> (this)] (const TrainerPanel::Values& v)
    {
        if (safe == nullptr)
            return;
        const bool wasOn = safe->trainer.on;
        safe->trainer = v;
        safe->trainer.on = wasOn;
        safe->saveSettings();
        if (v.on != wasOn)
            safe->setTrainerOn (v.on);   // switched on: it starts at "Von"
    };
    wireTrainerSlots (*panel);
    juce::CallOutBox::launchAsynchronously (std::move (panel), trainerButton.getScreenBounds(), nullptr);
}

void MainComponent::setTrainerOn (bool on)
{
    trainer.on = on;
    trainerPasses = 0;
    trainerLastLoop = -1;
    if (on)
    {
        trainerTempo = trainer.from;
        trainerSettingTempo = true;
        setTempo (trainer.from);
        trainerSettingTempo = false;
        if (loopOn && loopFirst >= 0 && stripShowsSelectionPlan() && (view.bar < loopFirst || view.bar > loopLast))
            seekToBar (loopFirst);   // with a loop on, practice starts at its range, not somewhere outside
    }
    trainerButton.setToggleState (on, juce::dontSendNotification);
}

void MainComponent::trainerCount (const AudioEngine::DisplayEvent& pulse)
{
    if (! trainer.on)
        return;
    // only passes of the selection count: not the count-in, not a preview, nothing of the workshop
    if (pulse.planTag != selectionTag || workshopOpen() || audition != Audition::None)
    {
        trainerLastLoop = -1;   // counting goes on from where it comes back (passes counted so far stay)
        return;
    }
    if (trainerLastLoop < 0 || pulse.generation != trainerGeneration || pulse.loop < trainerLastLoop)
    {
        trainerLastLoop = pulse.loop;   // a new start or a new plan of the selection: count from here
        trainerGeneration = pulse.generation;
        return;
    }
    trainerPasses += pulse.loop - trainerLastLoop;
    trainerLastLoop = pulse.loop;
}

void MainComponent::trainerTick()
{
    if (! trainer.on || ! control.play)
        return;
    double next = trainerTempo;
    for (; trainerPasses >= trainer.repeats; trainerPasses -= trainer.repeats)   // several at once (a stalled display): none lost
        next = juce::jmin (trainer.to, next + trainer.step);
    if (juce::approximatelyEqual (next, trainerTempo))
        return;   // not yet, or at "Bis": it simply goes on at that tempo
    trainerTempo = next;
    trainerSettingTempo = true;
    setTempo (next);
    trainerSettingTempo = false;
}

void MainComponent::toggleFavorite()
{
    if (patternMode())
        library.toggleFavorite (library.get (currentPattern).id);
    else
        meters.toggleFavorite (currentMeter);
    updateButtons();
    saveSettings();
}

juce::String MainComponent::currentTitle() const
{
    if (editing.has_value())
        return juce::String (editing->name) + (audition == Audition::Menu ? "  (Vorhören)"_u
                                                : audition == Audition::Arranger ? "  (Arranger)" : "  (im Editor)");
    if (patternMode())
        return juce::String (library.get (currentPattern).name);
    return "Metronom";
}

void MainComponent::sendCurrentPlan()
{
    std::unique_ptr<hz::PatternPlan> plan;
    PlanInfo info;
    info.title = currentTitle();
    info.fromEditor = editing.has_value();

    if (editing.has_value())
        plan = makePatternPlan (*editing);
    else if (arranging.has_value())
    {
        // the arranger's sequence: the strip shows its parts, the title says where it comes from
        hz::Pattern whole;
        juce::String err;
        if (buildSongPattern (*arranging, library.all(), whole, info.spans, err))
            plan = makeSongPlan (*arranging, library);
        info.title = juce::String (arranging->name) + "  (Arranger)";
    }
    else if (const auto* song = library.song (currentPattern))
    {
        plan = makeSongPlan (*song, library);
        info.spans = *library.songSpans (currentPattern);
    }
    else if (patternMode())
    {
        const auto& p = library.get (currentPattern);
        plan = makePatternPlan (p);
        if (p.bars.size() > 1)   // its bars in the strip too: the loop range is picked there
        {
            SongSpan whole;
            whole.label = juce::String (p.name);
            whole.part = -1;   // no song part (see StatusPanels)
            whole.numBars = (int) p.bars.size();
            whole.colour = sourceColourIndex (p.id);
            whole.shape = sourceShapeIndex (p.id);
            info.spans.push_back (whole);
        }
    }

    if (plan == nullptr)
        plan = makeMetronomePlan (meters.get (currentMeter), sound);

    channelsInUse = mix::channelsUsedBy (*plan);
    plans.registerPlan (*plan, meters, std::move (info));
    if (! editing.has_value() && ! arranging.has_value())
        selectionTag = plans.latestTag();   // the loop range and the speed trainer belong to this one
    engine.sendPlan (std::move (plan));
    if (! control.play)
        hasShown = false;   // a new plan starts from its beginning

    control.click = ! editing.has_value() && ! patternMode() && sound == MetronomeSound::Click;
    channelsInUse[mix::Click] = channelsInUse[mix::Click] || control.click;
    publishControl();
    updateView();
    sendLoop();   // the loop applies to this plan only, and never in the workshop
}

void MainComponent::updateButtons()
{
    if (patternMode())
    {
        const auto& p = library.get (currentPattern);
        patternButton.setContent (p.name, library.isFavorite (p.id));
        meterButton.setContent (Library::meterText (p), false);
    }
    else
    {
        patternButton.setContent ("Metronom", false);
        meterButton.setContent (meters.label (currentMeter), meters.isFavorite (currentMeter));
    }
    soundBox.setEnabled (! patternMode());
    deleteEntryButton.setVisible (patternMode() && (library.isUserPattern (currentPattern) || library.isSong (currentPattern)));
    deleteEntryButton.setEnabled (! editing.has_value() && ! arranging.has_value());
    renameEntryButton.setVisible (deleteEntryButton.isVisible());   // own patterns and songs, like Löschen
}

//==============================================================================
// Display

void MainComponent::updateView()
{
    // while playing, show what is audible; when stopped, show the selection
    view.live = control.play && hasShown;
    view.paused = ! control.play && hasShown;
    view.tempo = control.tempoTenths / 10.0;
    view.plan = view.hasPosition() ? plans.find (shown.planTag) : plans.latest();
    view.title = view.hasPosition() && view.plan != nullptr ? view.plan->title : currentTitle();
    view.spans = view.plan != nullptr && ! view.plan->spans.empty() ? &view.plan->spans : nullptr;

    if (view.hasPosition())
    {
        view.meter = meters.get (shown.meter);
        view.bar = shown.bar;
        view.pulse = shown.pulse;
        view.loop = shown.loop;
        view.numBars = shown.numBars;
        view.pulseTimeMs = shown.timeMs;
    }
    else
    {
        const auto* p = editing.has_value() ? &*editing : patternMode() ? &library.get (currentPattern) : nullptr;
        view.meter = p != nullptr && ! p->bars.empty() ? p->bars.front().meter : meters.get (currentMeter);
        view.bar = view.pulse = -1;
    }

    titleLabel.setText (view.title, juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, audition == Audition::Menu ? juce::Colour (0xff3cc6b4) : theme::textMain);   // green while auditioning
    songStrip.setVisible (view.spans != nullptr);
    showLoopAndTrainer();
    showLoop();
    if (songStrip.isVisible() != (songStrip.getHeight() > 0)
        || (songStrip.isVisible() && songStrip.getHeight() != songStrip.preferredHeight (songStrip.getWidth())))
        resized();   // the strip's height follows the song (one to three rows)
    statusPanels.repaint();
    songStrip.repaint();
    beatDisplay.repaint();
}

void MainComponent::timerCallback()
{
    if (kits->poll (engine))
    {
        kitLabel.setText (kits->status(), juce::dontSendNotification);
        if (editorWindow != nullptr)
            editorWindow->getEditor().kitChanged();
    }
    engine.collectRetiredPlans();
    showLoopAndTrainer();   // also when the workshop opens or closes while nothing plays

    // take the keyboard focus back only if our window is active (not from dialogs or menus)
    if (auto* peer = getPeer(); peer != nullptr && peer->isFocused() && ! hasKeyboardFocus (true)
        && juce::Component::getCurrentlyModalComponent() == nullptr)
        grabKeyboardFocus();

    incoming.clear();
    engine.popDisplayEvents (incoming);
    pendingDisplay.insert (pendingDisplay.end(), incoming.begin(), incoming.end());

    if (! control.play)
    {
        pendingDisplay.clear();   // paused: the view keeps showing the last position
        return;
    }

    // show a pulse only once it actually comes out of the speakers; drop events of older runs
    const double now = juce::Time::getMillisecondCounterHiRes();
    while (! pendingDisplay.empty() && pendingDisplay.front().generation != control.generation)
        pendingDisplay.pop_front();

    bool changed = false;
    while (! pendingDisplay.empty() && pendingDisplay.front().timeMs <= now)
    {
        shown = pendingDisplay.front();
        pendingDisplay.pop_front();
        hasShown = changed = true;
        trainerCount (shown);   // every pulse, also when several come due at once
    }

    if (changed)
    {
        followCountIn();
        trainerTick();
        updateView();
    }
    else if (hasShown)
    {
        // keep the moving markers smooth between pulses
        beatDisplay.repaint();
        songStrip.repaint();
    }
}

//==============================================================================
// Layout

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    watermark.draw (g, getLocalBounds());
    if (editorWindow != nullptr)   // the workshop is open: "Bearbeiten" glows, so one notices (Loop and Trainer rest)
        neonglow::draw (g, editButton.getBounds().toFloat().reduced (1.0f, 2.0f), theme::workshopGlow, neonglow::breathing());
}

void MainComponent::resized()
{
    helpOverlay.setBounds (getLocalBounds());
    quickSearch.setBounds (getLocalBounds());
    activeGlow.setBounds (getLocalBounds().withHeight (ActiveGlow::height));
    activeGlow.toFront (false);

    auto area = getLocalBounds().reduced (24);

    // row 1: transport
    // row 1: the four buttons left, same width; the tempo slider takes the rest (help: F1, see hint line)
    auto row1 = area.removeFromTop (44);
    for (auto* b : { &playButton, &tapButton, &mixerButton, &audioButton, &countInButton })
    {
        b->setBounds (row1.removeFromLeft (104));
        row1.removeFromLeft (8);
    }
    tempoSlider.setBounds (row1);

    // row 2: selection
    area.removeFromTop (8);
    auto row2 = area.removeFromTop (40);
    const bool compactSelection = row2.getWidth() < 750;
    patternButton.setBounds (row2.removeFromLeft (compactSelection ? juce::jmax (160, row2.getWidth() / 3)
                                                                : juce::jmax (220, row2.getWidth() * 2 / 5)));
    row2.removeFromLeft (8);
    searchButton.setBounds (row2.removeFromLeft (compactSelection ? 72 : 86));
    row2.removeFromLeft (8);
    editButton.setBounds (row2.removeFromLeft (compactSelection ? 90 : 110));
    row2.removeFromLeft (8);
    soundBox.setBounds (row2.removeFromRight (compactSelection ? 100 : 140));
    row2.removeFromRight (8);
    meterButton.setBounds (row2);

    // footer: kit status and key hint
    helpLabel.setBounds (area.removeFromBottom (32));
    kitLabel.setBounds (area.removeFromBottom (28));

    area.removeFromTop (8);
    auto titleRow = area.removeFromTop (40);
    deleteEntryButton.setBounds (titleRow.removeFromRight (94).reduced (0, 3));
    titleRow.removeFromRight (8);
    renameEntryButton.setBounds (titleRow.removeFromRight (120).reduced (0, 3));
    titleRow.removeFromRight (8);
    trainerButton.setBounds (titleRow.removeFromRight (94).reduced (0, 3));   // as big as "Löschen"
    titleRow.removeFromRight (8);
    loopButton.setBounds (titleRow.removeFromRight (94).reduced (0, 3));
    titleRow.removeFromRight (8);
    titleLabel.setBounds (titleRow);
    songStrip.setBounds (songStrip.isVisible() ? area.removeFromTop (songStrip.preferredHeight (area.getWidth())) : juce::Rectangle<int>());
    statusPanels.setBounds (area.removeFromTop (area.getHeight() / 2));
    beatDisplay.setBounds (area);
}
