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
#include "app/views/GhostMenu.h"
#include "app/TrainerSlots.h"
#include "app/views/TrainerPanel.h"
#include "app/BrowseOrder.h"

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

    // main window X or a quit from the system: asks first if the raster holds unsaved changes
    void requestQuit();

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
    void setPlaybackTempo (double bpm) override { setTempo (bpm); }
    bool patternNameTaken (const juce::String&) const override;
    void previewVoice (hz::Voice v) override { engine.previewVoice (v, 0.75f); }
    const hz::DrumKit* currentKit() const override { return kits != nullptr ? kits->kit() : nullptr; }
    MeterLibrary& getMeters() override { return meters; }
    void editorClosed() override;

    // ArrangerHost (MainComponentArranger.cpp)
    const Library& getLibrary() const override { return library; }
    void arrangerAudition (const SongPart&) override;
    void arrangerPlay (const Song&, int fromPart) override;
    void arrangerUpdate (const Song&) override;
    void arrangerSeekToPart (int part) override;
    void arrangerSeekToBar (int bar) override     { seekToBar (bar); }   // the plan shown is the arrangement
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
    // browsing as in the pattern menu (BrowseOrder.h); every key shows where one is (GhostMenu)
    void browseInFolder (int dir);                    // left/right: within the current entry's folder, stops at its ends
    void browseFolder (int dir);                      // Ctrl+left/right: the first entry of the next visible folder
    void browseFavorites (int dir);                   // Shift+left/right: the Favoriten section, stops at its ends
    void browseMeters (int dir, bool favoritesOnly);  // the metronome: the meters, stops at the ends
    std::vector<BrowseFolder> browseOrder() const;
    void showGhost (const GhostMenu::Content&);

    // Verlauf (Alt + arrow): the entries chosen so far, back and forward as in a browser; empty after a restart
    struct HistoryEntry { std::string patternId; int meter = -1; };   // a pattern, or the metronome in a meter
    std::vector<HistoryEntry> history;
    int historyAt = -1;
    bool historyMoving = false;        // a step through the history is not recorded again
    void recordHistory();
    void moveHistory (int dir);

    // Loop (a range of whole bars, picked with Shift + drag in the bar strip) and speed trainer
    void setLoopRange (int firstBar, int lastBar);
    void clearLoop();
    void toggleLoop();
    void sendLoop();                   // the range to the engine: only while on and the plan it belongs to plays
    void showLoop();                   // the range in the strip and the Loop button
    bool workshopOpen() const          { return editorWindow != nullptr || editing.has_value() || arranging.has_value(); }
    void showLoopAndTrainer();         // both belong to the main player: hidden and suspended while the workshop is open
    bool workshopWasOpen = false;
    bool stripShowsSelection() const;  // the strip shows the selection's own plan (not a count-in, preview or workshop)
    bool stripShowsSelectionPlan() const;   // ... and its latest plan, so that its bars are the ones sent
    void showTrainer();
    void setTrainerOn (bool on);
    void trainerCount (const AudioEngine::DisplayEvent&);   // every pulse heard: count the passes
    void trainerTick();                // after new pulses: raise the tempo for every due increment
    int loopFirst = -1, loopLast = -1; // bars of the plan below
    bool loopOn = false;
    std::string loopPatternId;         // the plan (pattern or song) the range belongs to
    int selectionTag = -1;             // the plan of the selection (sendCurrentPlan outside workshop and arranger)
    TrainerPanel::Values trainer;      // settings remembered; 'on' is not
    trainerslots::Slots trainerSlots;  // the trainer's save slots (trainer.json), see MainComponentTrainer.cpp
    int activeTrainerSlot = -1;        // loaded or stored last, until another selection
    void wireTrainerSlots (TrainerPanel&);
    TrainerPanel::SlotView trainerSlotView (int slot) const;
    TrainerPanel::SlotView storeTrainerSlot (int slot);                 // the present setup into the slot
    std::optional<TrainerPanel::Values> loadTrainerSlot (int slot);     // the slot's setup back
    bool slotIsSelection (int slot) const;   // the slot holds what is selected now
    void saveTrainerSlots();                 // to trainer.json; says so if that fails
    double trainerTempo = 0.0;
    int trainerPasses = 0, trainerLastLoop = -1;
    std::uint16_t trainerGeneration = 0;   // the run (start) whose passes are counted
    bool trainerSettingTempo = false;  // a tempo change of the trainer's own (any other ends it)
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
    // the order of the Baustein menu: own folders or Band › Album › Song (Library::View); the disc in the menu
    // button, the first menu line and O inside the open menu switch it
    void setMenuView (Library::View);
    void updateViewDisc();
    int patternMenuSession = 0;          // counts the openings of the Baustein menu: O belongs to the current one
    bool menuViewSwitchWanted = false;   // O was pressed: switch and reopen once the menu has closed
    bool menuKeyHeld = false;            // O still down when the menu opened again: its repeats switch nothing
    void showMeterMenu();
    void openSearch();
    void confirmDeleteLibraryEntry (const std::string&);
    juce::String removeLibraryEntry (const std::string&);   // to the trash; "" = OK, else why not
    void showHelp (bool show);
    void showMixer();
    void toggleMixer();              // key M: open, or close if it is showing
    void setMix (int channel, mix::Setting);
    bool channelInUse (int channel) const;   // mixer: does this channel sound in what is selected?
    void showAudioSettings();
    void showDataTools (int tab);    // 0 trash, 1 log files, 2 folders
    void refreshFolders();           // the folder window, if open, after the library changed
    void refreshMenuFolders();       // the Menü-Ordner window, if open: its ticks and counts
    void redistributeBadges();       // System: every pattern a new badge by the chosen rule (old assignment backed up)

    // --- renaming (MainComponentRename.cpp): right click in the pattern menu
    void askRenameEntry (const std::string& id);
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
    bool quitQuestionOpen = false;   // the "unsaved changes" question of requestQuit is showing
    PlaybackView view;

    // windows and widgets
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;   // shows the setTooltip texts in every window of the app; none while off
    bool tooltipsEnabled = true;                           // System: "Tooltips"
    void setTooltipsEnabled (bool on);
    // M inside the mixer window closes it again (declared before the windows, so it outlives them)
    struct WindowKey : juce::KeyListener
    {
        std::function<bool (const juce::KeyPress&)> onKey;
        std::function<void()> onKeyState;   // a key went down or up
        bool keyPressed (const juce::KeyPress& k, juce::Component*) override { return onKey != nullptr && onKey (k); }
        bool keyStateChanged (bool, juce::Component*) override { if (onKeyState) onKeyState(); return false; }
    } mixerKeys, menuKeys;   // menuKeys: O inside the open Baustein menu
    std::unique_ptr<WorkshopWindow> editorWindow;
    std::unique_ptr<juce::DialogWindow> mixerWindow, audioWindow, trashWindow, logsWindow, foldersWindow, menuFoldersWindow, quellenWindow;
    void showQuellen();                                       // System › Quellen (MainComponentQuellen.cpp)
    juce::String installFromSource (const juce::File& staging);   // a loaded source into the data folder
    juce::TextButton playButton { "Start" }, tapButton { "Tap" }, audioButton { "System" },
                     editButton { "Bearbeiten" }, mixerButton { "Mixer" }, searchButton { "Suchen" }, deleteEntryButton { "Löschen"_u },
                     renameEntryButton { "Umbenennen" }, countInButton,
                     loopButton { "Loop (L)" }, trainerButton { "Trainer (S)" };   // left of "Umbenennen", as big as "Löschen"
    StarButton patternButton, meterButton;
    juce::Slider tempoSlider;
    juce::ComboBox soundBox;
    juce::Label helpLabel, kitLabel, titleLabel;
    StatusPanels statusPanels;
    SongStrip songStrip;
    BeatDisplay beatDisplay;
    HelpOverlay helpOverlay;
    GhostMenu ghostMenu;
    bool ghostEnabled = true;     // System > Menü-Ordner: "Grüne Einblend-Menüs aktiv"
    double ghostSeconds = 1.0;    // how long it stays
    QuickSearch quickSearch;
    ActiveGlow activeGlow;
    std::vector<KeyBinding> keyBindings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
