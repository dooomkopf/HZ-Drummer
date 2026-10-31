#pragma once

#include "app/views/HelpLine.h"
#include "app/MeterLibrary.h"
#include "app/Text.h"
#include "engine/Pattern.h"
#include "engine/SamplePlayer.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <vector>

// What the editor needs from the main window (audition, saving, playhead, transport keys)
class EditorGrid;

class EditorHost
{
public:
    virtual ~EditorHost() = default;

    struct Playhead
    {
        bool playing = false;   // the edited pattern is playing right now
        int bar = -1, pulse = -1;
    };

    virtual void auditionPattern (const hz::Pattern&) = 0;          // make the edited version audible right away
    virtual void auditionStopped() = 0;                              // the editor is empty again: back to the main selection
    virtual juce::String savePattern (hz::Pattern&, bool asNew) = 0; // "" = OK, else an error text; sets id/revision
    virtual juce::String deletePattern (const hz::Pattern&) = 0;
    virtual Playhead getPlayhead() const = 0;
    virtual bool handleTransportKey (const juce::KeyPress&) = 0;     // space, tempo, tap
    virtual double getCurrentTempo() const = 0;
    virtual void setPlaybackTempo (double bpm) = 0;                 // the raster's tempo field: the main tempo follows
    virtual bool patternNameTaken (const juce::String&) const = 0;  // a pattern of the library has this name
    virtual void previewVoice (hz::Voice) = 0;                      // one hit of this instrument right now (a click on a lane name)
    virtual const hz::DrumKit* currentKit() const = 0;              // which sounds exist (nullptr while no kit is loaded)
    virtual MeterLibrary& getMeters() = 0;
    virtual void editorClosed() = 0;
};

// Raster editor for one pattern: lanes x steps; click sets/removes a hit, dragging changes the velocity
class PatternEditor : public juce::Component,
                      private juce::Timer
{
public:
    explicit PatternEditor (EditorHost&);
    ~PatternEditor() override;

    // isUserPattern: own pattern (may be overwritten/deleted), otherwise bundled ("save as" only)
    void setPattern (const hz::Pattern&, bool isUserPattern);

    // A block of the arranger's sequence: "Speichern" becomes "In Ablauf übernehmen" and hands the
    // pattern to 'apply' instead of the library ("Speichern unter" still creates a library pattern)
    void setBlockPattern (const hz::Pattern&, const juce::String& blockTitle, std::function<bool (const hz::Pattern&)> apply);
    void blockApplied (const hz::Pattern&);   // the arranger took this version over later (after its question)
    // A card of the Ablage: like a block ("In Ablage übernehmen" writes it back); "Speichern unter" also hands
    // the new own pattern to savedAsNew (it becomes a card next to this one) and the raster then edits that pattern.
    void setCardPattern (const hz::Pattern&, const juce::String& cardTitle, std::function<bool (const hz::Pattern&)> apply,
                         std::function<void (const hz::Pattern&)> savedAsNew);
    void kitChanged()                         { updateLanes(); }   // other sounds: lanes and their names follow
    void updateWindowTitle();                 // "Werkstatt – <pattern> *" (the arranger tab shows the song instead)

    // Empties the editor ("Schließen"): nothing can be edited by accident until a pattern is loaded again
    void clearPattern();

    // The loaded pattern is audible only while the raster tab is in front: the workshop window
    // releases it when another tab shows and resumes it when the raster returns
    void releaseAudition()  { if (! pattern.bars.empty()) host.auditionStopped(); }
    void resumeAudition()   { if (! pattern.bars.empty()) host.auditionPattern (audible()); }
    const hz::Pattern& getPattern() const { return pattern; }
    bool hasUnsavedChanges() const;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;   // the M / S switches of the lane column
    void mouseMove (const juce::MouseEvent&) override;   // a lane name under the mouse turns green: a click plays it
    void mouseExit (const juce::MouseEvent&) override;

private:
    void load (const hz::Pattern&, bool isUserPattern);   // setPattern without touching the main tempo (blocks)
    juce::String defaultHelp() const;                      // the built-in help line (Strg+S depends on block mode)
    juce::String variantName() const;                      // "Speichern unter" suggests "<name> modN"
    friend class EditorGrid;

    struct Lane
    {
        hz::Voice voice;
        juce::String name;
        bool core;   // always visible
    };

    void timerCallback() override;
    void changed (const hz::Pattern& before);   // after every change: undo step, audition, display
    void restoreContent (const hz::Pattern& snapshot);
    void refreshDirty();
    void refreshAll();
    void editBar (int bar, std::function<void (hz::Pattern&)> edit);
    void undo();
    void redo();
    void save (bool asNew);
    void askName (std::function<void (juce::String)> onOk);
    void askRename();
    void closeRequested();
    void updateLanes();
    void updateControls();
    void showBarMenu (int bar);
    void showMeterMenuForBar (int bar);
    void showGridMenuForBar (int bar);
    int laneIndex (hz::Voice) const;

    // Lane mute / solo: listening aids while editing, never part of the pattern
    hz::Pattern audible() const;                             // the pattern as heard
    bool isMuted (hz::Voice) const;
    bool isSolo (hz::Voice) const;
    juce::Rectangle<int> laneSwitch (int lane, bool solo) const;   // the M or S box of a lane row
    juce::Rectangle<int> laneSwitchReset() const;                  // above the M / S column: all off
    juce::String laneLabel (const Lane&) const;                    // what the kit really plays ("Zil-Bel" instead of "Kuhglocke")
    bool laneSilent (const Lane&) const;                           // the kit has no sound for it
    void showLaneMenu (int lane);                                  // right click on a lane name: "Übertragen auf"
    void setStretch (double percent, int anchorX);                 // bar width in %, the time at anchorX (viewport) stays put

    EditorHost& host;
    EditorHost::Playhead lastPlayhead;
    hz::Pattern humanizeBefore;
    hz::Pattern pattern;
    hz::Pattern savedContent;      // content at the last save (for "unsaved")
    bool nameUndoPending = false;
    bool humanizeDragging = false;
    hz::Pattern tempoBefore;
    bool tempoDragging = false;
    int rowH = 26;
    int hoveredLane = -1;          // its name is green (see mouseMove)
    float stretch = 1.0f;          // how much wider than usual the bars are drawn; only for the pattern in hand
    bool userPattern = false;
    bool dirty = false;
    bool blockMode = false;        // see setBlockPattern
    bool cardMode = false;         // blockMode for a card of the Ablage (see setCardPattern)
    std::function<void (const hz::Pattern&)> cardSavedAsNew;
    juce::String blockTitle;
    bool swingHint = false;        // swing is set, but finds no place here: said in the bar line (see updateControls)
    std::function<bool (const hz::Pattern&)> applyToBlock;
    std::vector<hz::Pattern> undoStack, redoStack;
    std::vector<Lane> allLanes, lanes;
    std::vector<hz::Voice> mutedVoices, soloVoices;   // reset whenever a pattern is loaded

    juce::Label nameLabel;   // the name is shown, renaming goes through a dialog (a text field was mistaken for a search box)
    juce::TextButton undoButton { "Rückgängig"_u }, redoButton { "Wiederholen" }, addBarButton { "+ Takt" },
                     saveButton { "Speichern" }, saveAsButton { "Speichern unter" },
                     deleteButton { "Löschen"_u }, renameButton { "Umbenennen" }, closeButton { "Schließen"_u };
    juce::ToggleButton allLanesToggle { "Alle Spuren" };
    juce::ComboBox swingBox;
    juce::Slider humanizeSlider;
    juce::Slider tempoSlider;   // the pattern's own tempo (what it starts with when selected)
    juce::Slider stretchSlider; // "Breite": the view only, back to 100 % when another pattern comes in
    juce::Label humanizeLabel { {}, "Menschlich" }, stretchLabel { {}, "Breite" }, infoLabel;
    HelpLine help;   // the two lines at the foot; own text: hilfezeile-raster.txt in the data folder
    std::unique_ptr<EditorGrid> grid;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternEditor)
};
