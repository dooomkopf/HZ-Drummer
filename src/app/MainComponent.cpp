#include "app/MainComponent.h"

#include "app/Text.h"
#include "app/views/Theme.h"
#include "engine/MeterCatalog.h"

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

    badges.load (Library::songFolder().getSiblingFile ("abzeichen.json"));
    BadgeRegistry::setGlobal (&badges);
    library.onReloaded = [this] (const std::vector<hz::Pattern>& all) { badges.ensure (all); };
    library.reload();
    loadSettings();
    kits = std::make_unique<KitManager> (KitManager::findDefaultKit());

    for (auto* b : { &playButton, &tapButton, &audioButton, &editButton, &mixerButton })
        addAndMakeVisible (*b);
    playButton.onClick  = [this] { togglePlay(); };
    tapButton.onClick   = [this] { tapTempo(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    editButton.onClick  = [this] { openEditor(); };
    mixerButton.onClick = [this] { showMixer(); };
    addAndMakeVisible (countInButton);
    countInButton.onClick = [this] { cycleCountIn(); };
    countInButton.setTooltip ("Ein Takt Klicks vor dem Start: aus, nur am Anfang des Songs, oder vor jedem Start"_u);
    updateCountInButton();

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (30.0, 320.0, 1.0);
    tempoSlider.setValue (control.tempoTenths / 10.0, juce::dontSendNotification);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.onValueChange = [this] { setTempo (tempoSlider.getValue()); };

    addAndMakeVisible (patternButton);
    patternButton.onClick = [this] { showPatternMenu(); };
    addAndMakeVisible (searchButton);
    searchButton.onClick = [this] { openSearch(); };
    searchButton.setTooltip ("Patterns und Songs suchen (Strg+K oder /)");
    addAndMakeVisible (deleteEntryButton);
    deleteEntryButton.setTooltip ("Geladenen eigenen Eintrag in den Papierkorb verschieben");
    deleteEntryButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff713a40));
    deleteEntryButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffeeee));
    deleteEntryButton.onClick = [this]
    {
        if (patternMode() && (library.isUserPattern (currentPattern) || library.isSong (currentPattern))
            && ! editing.has_value() && ! arranging.has_value())
            confirmDeleteLibraryEntry (library.get (currentPattern).id);
    };
    addAndMakeVisible (meterButton);
    meterButton.onClick = [this] { showMeterMenu(); };

    addAndMakeVisible (soundBox);
    soundBox.addItemList ({ "Klick", "Bassdrum", "Snare", "Kick + Snare" }, 1);
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
    statusPanels.setView (&view);
    songStrip.setView (&view);
    beatDisplay.setView (&view);

    setupKeys();
    addChildComponent (helpOverlay);
    addAndMakeVisible (activeGlow);
    addChildComponent (quickSearch);

    // keyboard focus stays with the main window, so space & co. never end up in a widget
    for (juce::Component* c : { (juce::Component*) &playButton, (juce::Component*) &tapButton, (juce::Component*) &audioButton,
                                (juce::Component*) &tempoSlider, (juce::Component*) &meterButton,
                                (juce::Component*) &patternButton, (juce::Component*) &soundBox, (juce::Component*) &editButton,
                                (juce::Component*) &mixerButton, (juce::Component*) &searchButton, (juce::Component*) &deleteEntryButton,
                                (juce::Component*) &countInButton })
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
    countIn = (CountIn) juce::jlimit (0, 2, props.getIntValue ("countIn", 0));
    beatDisplay.setShowHits (props.getBoolValue ("showBeats", true));
    library.favoritesFromString (props.getValue ("patternFavorites"));
    currentPattern = library.indexOf (props.getValue ("pattern").toStdString());

    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        const juce::String k (mix::channelKey (ch));
        mixSettings[(size_t) ch] = { (float) props.getDoubleValue ("mix_" + k + "_db", 0.0), props.getBoolValue ("mix_" + k + "_mute", false), false };
    }
    engine.setMixState (mix::pack (mixSettings));
}

void MainComponent::saveSettings()
{
    auto& props = *appProperties.getUserSettings();
    props.setValue ("meter", MeterLibrary::key (meters.get (currentMeter)));
    props.setValue ("tempoTenths", (int) control.tempoTenths);
    props.setValue ("sound", (int) sound);
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
    playButton.setButtonText (control.play ? "Pause" : "Start");
    updateView();
}

void MainComponent::setTempo (double bpm)
{
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
        plan = makePatternPlan (library.get (currentPattern));

    if (plan == nullptr)
        plan = makeMetronomePlan (meters.get (currentMeter), sound);

    channelsInUse = mix::channelsUsedBy (*plan);
    plans.registerPlan (*plan, meters, std::move (info));
    engine.sendPlan (std::move (plan));
    if (! control.play)
        hasShown = false;   // a new plan starts from its beginning

    control.click = ! editing.has_value() && ! patternMode() && sound == MetronomeSound::Click;
    channelsInUse[mix::Click] = channelsInUse[mix::Click] || control.click;
    publishControl();
    updateView();
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
        kitLabel.setText (kits->status(), juce::dontSendNotification);
    engine.collectRetiredPlans();

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
    }

    if (changed)
    {
        followCountIn();
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
    row1.removeFromLeft (12);
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
    row2.removeFromLeft (16);
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
    titleLabel.setBounds (titleRow);
    songStrip.setBounds (songStrip.isVisible() ? area.removeFromTop (songStrip.preferredHeight (area.getWidth())) : juce::Rectangle<int>());
    statusPanels.setBounds (area.removeFromTop (area.getHeight() / 2));
    beatDisplay.setBounds (area);
}
