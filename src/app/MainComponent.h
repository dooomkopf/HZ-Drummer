#pragma once

#include "app/AudioEngine.h"
#include "app/Text.h"
#include "app/BadgeRegistry.h"
#include "app/HelpOverlay.h"
#include "app/arranger/ArrangerHost.h"
#include "app/views/ActiveGlow.h"
#include "app/KeyBindings.h"
#include "app/KitManager.h"
#include "app/Library.h"
#include "app/MeterLibrary.h"
#include "app/MixState.h"
#include "app/WorkshopWindow.h"
#include "app/PlanFactory.h"
#include "app/QuickSearch.h"
#include "app/StarButton.h"
#include "app/views/BeatDisplay.h"
#include "app/views/PlaybackView.h"
#include "app/views/SongStrip.h"
#include "app/views/StatusPanels.h"
#include "app/views/Watermark.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <deque>
#include <memory>
#include <optional>
#include <vector>

// Main window ("play view"). Wires the modules together:
//  - AudioEngine (real-time playback), KitManager (kits), Library (patterns/songs), PlanFactory (plans)
//  - views: StatusPanels, SongStrip, BeatDisplay; overlays: help, quick search
//  - the raster editor window (as its EditorHost) and the mixer window
// Implementation is split: MainComponent.cpp (setup, transport, selection, timer, layout),
// MainComponentMenus.cpp (menus, search, dialogs, keys), MainComponentEditor.cpp (editor host).
class MainComponent : public juce::AudioAppComponent,
                      public EditorHost,
                      public ArrangerHost,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // audio callback -> AudioEngine
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override {}

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // test hooks (command line: --editor, --play, --mixer)
    void openEditorForTesting() { openEditor(); }
    void openArrangerForTesting();
    void startForTesting()      { if (! control.play) togglePlay(); }
    void openMixerForTesting()  { showMixer(); }

    // EditorHost
    void auditionPattern (const hz::Pattern&) override;
    void auditionStopped() override;
    juce::String savePattern (hz::Pattern&, bool asNew) override;
    juce::String deletePattern (const hz::Pattern&) override;
    Playhead getPlayhead() const override;
    bool handleTransportKey (const juce::KeyPress&) override;
    double getCurrentTempo() const override { return control.tempoTenths / 10.0; }
    MeterLibrary& getMeters() override { return meters; }
    void editorClosed() override;

    // ArrangerHost (MainComponentArranger.cpp)
    const Library& getLibrary() const override { return library; }
    void arrangerAudition (const SongPart&) override;
    void arrangerPlay (const Song&, int fromPart) override;
    void arrangerUpdate (const Song&) override;
    void arrangerSeekToPart (int part) override;
    void arrangerStop() override;
    bool arrangerAuditionActive() const override { return audition == Audition::Arranger && control.play; }
    bool arrangerPlayActive() const override     { return arranging.has_value() && control.play; }
    bool arrangerPlanLoaded() const override     { return arranging.has_value() && ! editing.has_value(); }   // an editor audition overrides
    std::string currentPlanId() const override   { return view.plan != nullptr ? view.plan->patternId : std::string(); }
    const PlaybackView& getPlaybackView() const override { return view; }
    juce::String arrangerSaveSong (Song&) override;

private:
    // --- transport and selection (MainComponent.cpp)
    void timerCallback() override;
    void publishControl();
    void togglePlay();
    void setTempo (double bpm);
    void changeTempo (double delta) { setTempo (control.tempoTenths / 10.0 + delta); }
    void tapTempo();
    void selectMeter (int index);
    void selectSound (int index);
    void selectPattern (int index);   // -1 = metronome
    void selectPatternById (const std::string& id);
    void stepPattern (int dir, bool favoritesOnly);
    void stepCurrent (int dir, bool favoritesOnly);
    void stepStyle (int dir);
    void toggleFavorite();
    void sendCurrentPlan();
    void updateButtons();
    void updateView();
    juce::String currentTitle() const;
    bool patternMode() const { return currentPattern >= 0; }
    void saveSettings();
    void loadSettings();

    // --- menus, dialogs and keys (MainComponentMenus.cpp)
    void setupKeys();
    bool handleGlobalKey (const juce::KeyPress&);
    void showPatternMenu();
    void showMeterMenu();
    void openSearch();
    void confirmDeleteLibraryEntry (const std::string&);
    juce::String removeLibraryEntry (const std::string&);   // to the trash; "" = OK, else why not
    void showHelp (bool show);
    void showMixer();
    void setMix (int channel, mix::Setting);
    bool channelInUse (int channel) const;   // mixer: does this channel sound in what is selected?
    void showAudioSettings();
    void showDataTools (int tab);    // 0 trash, 1 log files, 2 folders
    void refreshFolders();           // the folder window, if open, after the library changed
    void seekToBar (int bar);        // within the playing song/pattern
    void seekPart (int dir);         // songs: next / previous part (N / B)
    void previewPattern (int index); // pattern menu: audition an entry (toggle), menu stays open
    void cancelPreview();            // menu dismissed: back to what played before

    // --- editor (MainComponentEditor.cpp)
    void openEditor();
    void loadIntoEditor (int index);

    // modules
    juce::ApplicationProperties appProperties;   // ~/.config/HZ-Drummer/
    MeterLibrary meters;
    BadgeRegistry badges;                        // fixed badge per pattern id, see BadgeRegistry
    Library library;
    AudioEngine engine;
    std::unique_ptr<KitManager> kits;
    PlanRegistry plans;

    // selection and transport (UI side)
    AudioEngine::Control control;
    int currentMeter = 0;
    int currentPattern = -1;
    MetronomeSound sound = MetronomeSound::Kick;

    // --- count-in (MainComponentCountIn.cpp): a bar of clicks before the selection starts,
    // main player only; "AtStart" counts in only when starting from the first bar
    enum class CountIn { Off, AtStart, Always };
    void cycleCountIn();
    void updateCountInButton();
    bool countInWillClick() const;
    bool startWithCountIn();     // true: the clicks run instead of the plan (togglePlay returns)
    void followCountIn();        // timer: queues the selection once the clicks sound
    CountIn countIn = CountIn::Off;
    int countInTag = -1;         // plan tag of the running count-in, -1 = none
    bool countInQueued = false;  // the selection was sent to follow the clicks
    double countInResume = 0.0;  // quarters where the selection continues afterwards
    // What overrides the main selection: 'editing' (a pattern played instead) is owned by exactly
    // one audition source, 'arranging' is the arranger's sequence. A menu preview remembers
    // everything it overrides and puts it back on cancel.
    enum class Audition { None, Raster, Arranger, Menu };
    std::optional<hz::Pattern> editing;          // plays instead of the selection while audition != None
    Audition audition = Audition::None;
    std::optional<Song> arranging;               // the arranger's sequence while it plays
    int menuPreviewIndex = -1;
    struct PreviewRestore
    {
        std::optional<hz::Pattern> editing;
        Audition audition = Audition::None;
        std::optional<Song> arranging;
        bool wasPlaying = false;
        int tempoTenths = 1200;
        int bar = -1;
    } beforePreview;
    Watermark watermark;
    mix::Settings mixSettings {};
    std::array<bool, mix::NumChannels> channelsInUse {};   // by the plan sent last (count-in aside)
    std::vector<double> tapTimes;

    // display state (filled from the engine's display events)
    std::deque<AudioEngine::DisplayEvent> pendingDisplay;
    std::vector<AudioEngine::DisplayEvent> incoming;
    AudioEngine::DisplayEvent shown;
    bool hasShown = false;
    PlaybackView view;

    // windows and widgets
    std::unique_ptr<WorkshopWindow> editorWindow;
    std::unique_ptr<juce::DialogWindow> mixerWindow, audioWindow, trashWindow, logsWindow, foldersWindow;
    juce::TextButton playButton { "Start" }, tapButton { "Tap" }, audioButton { "System" },
                     editButton { "Bearbeiten" }, mixerButton { "Mixer" }, searchButton { "Suchen" }, deleteEntryButton { "Löschen"_u },
                     countInButton;
    StarButton patternButton, meterButton;
    juce::Slider tempoSlider;
    juce::ComboBox soundBox;
    juce::Label helpLabel, kitLabel, titleLabel;
    StatusPanels statusPanels;
    SongStrip songStrip;
    BeatDisplay beatDisplay;
    HelpOverlay helpOverlay;
    QuickSearch quickSearch;
    ActiveGlow activeGlow;
    std::vector<KeyBinding> keyBindings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
