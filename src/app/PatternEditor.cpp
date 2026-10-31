#include "app/PatternEditor.h"

#include "app/EditorGrid.h"
#include "app/Text.h"
#include "app/views/InWindowDialog.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"

PatternEditor::PatternEditor (EditorHost& h) : host (h), help ("hilfezeile-raster.txt", [this] { return defaultHelp(); }, 15.0f, 2)
{
    using I = hz::Instrument;
    using A = hz::Articulation;
    allLanes = {
        { { I::Crash1, A::Normal },  "Crash 1", true },   { { I::Crash2, A::Normal }, "Crash 2", false },
        { { I::China, A::Normal },   "China", false },    { { I::Splash, A::Normal }, "Splash", false },
        { { I::Ride, A::Normal },    "Ride", true },      { { I::Ride, A::Bell },     "Ride-Glocke", false },
        { { I::Ride, A::Brush },     "Ride Besen", false },
        { { I::HiHat, A::Normal },   "HiHat closed", true },  { { I::HiHat, A::Half },    "HiHat halb", false },
        { { I::HiHat, A::Open },     "HiHat open", true },
        { { I::Cowbell, A::Normal }, "Kuhglocke", false },
        // the snare right under the hihat, above the toms: as in drum tabs, and the backbeat sits next to the hihat
        { { I::Snare, A::Normal },   "Snare", true },     { { I::Snare, A::Rim },     "Snare Rim", false },
        { { I::Snare, A::Side },     "Sidestick", false },
        // brushes: only with a kit that has them (Swirly Drums added to the Aasimonster), else only when used
        { { I::Snare, A::Brush },    "Snare Besen", false },  { { I::Snare, A::Sweep }, "Besen kurz", false },
        { { I::Snare, A::SweepLong }, "Besen lang", false },
        { { I::Tom1, A::Normal },    "Tom 1", true },     { { I::Tom2, A::Normal },   "Tom 2", false },
        { { I::Tom2, A::Brush },     "Tom 2 Besen", false },
        { { I::Tom3, A::Normal },    "Tom 3", true },     { { I::Tom4, A::Normal },   "Tom 4", false },
        { { I::HiHat, A::Pedal },    "HiHat-Pedal", false },
        { { I::Kick, A::Normal },    "Bass", true },
    };

    addAndMakeVisible (nameLabel);
    nameLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, textMain);
    nameLabel.setMinimumHorizontalScale (0.7f);

    for (auto* b : { &undoButton, &redoButton, &addBarButton, &saveButton, &saveAsButton, &deleteButton,
                     &renameButton, &closeButton })
    {
        addAndMakeVisible (*b);
        b->setWantsKeyboardFocus (false);
    }
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    addBarButton.onClick = [this]
    {
        const auto before = pattern;
        hz::insertBar (pattern, (int) pattern.bars.size() - 1, true);
        changed (before);
    };
    saveButton.onClick = [this] { save (false); };
    saveAsButton.onClick = [this] { save (true); };
    renameButton.onClick = [this] { askRename(); };
    closeButton.onClick = [this] { closeRequested(); };
    renameButton.setTooltip ("Den Baustein umbenennen (Dialog)");
    closeButton.setTooltip ("Editor leeren, damit nichts aus Versehen geändert wird"_u);
    deleteButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Baustein löschen"_u,
            "Baustein \"" + juce::String (pattern.name) + "\" in den Papierkorb verschieben?"_u, "In Papierkorb", "Abbrechen", this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PatternEditor> (this)] (int r)
            {
                if (safe == nullptr || r != 1)
                    return;
                if (auto err = safe->host.deletePattern (safe->pattern); err.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Löschen nicht möglich"_u, err);
                else
                    safe->clearPattern();   // the window stays open, the raster is empty again
            }));
    };

    addAndMakeVisible (allLanesToggle);
    allLanesToggle.setWantsKeyboardFocus (false);
    allLanesToggle.onClick = [this] { updateLanes(); };

    addAndMakeVisible (swingBox);
    swingBox.getProperties().set ("pill", true);   // drawn by HZLook like the buttons beside it
    swingBox.setWantsKeyboardFocus (false);
    for (int i = 0; i < (int) std::size (swingOptions); ++i)
        swingBox.addItem (juce::String (juce::CharPointer_UTF8 (swingOptions[i].label)), i + 1);
    swingBox.onChange = [this]
    {
        const int i = swingBox.getSelectedItemIndex();
        if (i < 0)
            return;
        const auto before = pattern;
        pattern.swing.subdivision = swingOptions[i].sub;
        pattern.swing.ratio = swingOptions[i].ratio;
        changed (before);
    };

    addAndMakeVisible (humanizeLabel);
    humanizeLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (humanizeSlider);
    humanizeSlider.setWantsKeyboardFocus (false);
    humanizeSlider.setRange (0.0, 100.0, 1.0);
    humanizeSlider.setTextValueSuffix (" %");
    humanizeSlider.getProperties().set ("pill", true);   // value box drawn by HZLook as a pill
    humanizeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 24);
    // dragging = one undo step; typed values / keys in the slider are a step of their own
    humanizeSlider.onDragStart = [this] { humanizeBefore = pattern; humanizeDragging = true; };
    humanizeSlider.onValueChange = [this]
    {
        const auto before = pattern;
        pattern.humanize = (float) humanizeSlider.getValue() / 100.0f;
        if (! humanizeDragging)
            changed (before);
    };
    humanizeSlider.onDragEnd = [this] { humanizeDragging = false; changed (humanizeBefore); };

    // how wide the bars are drawn (also Strg+Mausrad over the raster): the view only, never part of the pattern
    addAndMakeVisible (stretchLabel);
    stretchLabel.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (stretchSlider);
    stretchSlider.setWantsKeyboardFocus (false);
    stretchSlider.setRange (100.0, 800.0, 5.0);
    stretchSlider.setSkewFactorFromMidPoint (250.0);   // more travel for the small factors
    stretchSlider.setValue (100.0, juce::dontSendNotification);
    stretchSlider.setTextValueSuffix (" %");
    stretchSlider.getProperties().set ("pill", true);   // value box drawn by HZLook as a pill
    stretchSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
    stretchSlider.setTooltip ("Breite der Takte, nur in der Ansicht und nur für diesen Baustein. Auch mit Strg+Mausrad über dem Raster.");
    stretchSlider.onValueChange = [this] { setStretch (stretchSlider.getValue(), viewport.getMaximumVisibleWidth() / 2); };

    // the pattern's tempo, a slider like the main window's: one undo step per drag and per typed value; the main tempo follows
    addAndMakeVisible (tempoSlider);
    tempoSlider.setWantsKeyboardFocus (false);
    tempoSlider.setRange (30.0, 320.0, 1.0);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.getProperties().set ("pill", true);   // value box drawn by HZLook as a pill
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    tempoSlider.onDragStart = [this] { tempoBefore = pattern; tempoDragging = true; };
    tempoSlider.onValueChange = [this]
    {
        const auto before = pattern;
        pattern.tempo = tempoSlider.getValue();
        host.setPlaybackTempo (pattern.tempo);
        if (! tempoDragging)
            changed (before);
    };
    tempoSlider.onDragEnd = [this] { tempoDragging = false; changed (tempoBefore); };

    addAndMakeVisible (infoLabel);
    addAndMakeVisible (help);
    infoLabel.setColour (juce::Label::textColourId, textDim);
    infoLabel.setFont (juce::FontOptions (16.0f));

    grid = std::make_unique<EditorGrid> (*this);
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

PatternEditor::~PatternEditor() = default;

void PatternEditor::setBlockPattern (const hz::Pattern& p, const juce::String& title, std::function<bool (const hz::Pattern&)> apply)
{
    load (p, false);   // not setPattern: in a song the song's tempo counts, the main tempo stays
    blockMode = true;
    blockTitle = title;
    applyToBlock = std::move (apply);
    saveButton.setButtonText ("In Ablauf übernehmen"_u);
    updateControls();
    resized();
}

void PatternEditor::setCardPattern (const hz::Pattern& p, const juce::String& title, std::function<bool (const hz::Pattern&)> apply,
                                    std::function<void (const hz::Pattern&)> savedAsNew)
{
    setBlockPattern (p, title, std::move (apply));
    cardMode = true;
    cardSavedAsNew = std::move (savedAsNew);
    saveButton.setButtonText ("In Ablage übernehmen"_u);
    updateControls();
    repaint();
}

void PatternEditor::blockApplied (const hz::Pattern& applied)
{
    if (! blockMode)
        return;
    savedContent = applied;   // edits made while the question was open stay unapplied
    refreshDirty();
    updateControls();
}

void PatternEditor::setPattern (const hz::Pattern& p, bool isUserPattern)
{
    // another pattern comes in: it plays in its own tempo (a reload of the same one, e.g. after a rename, keeps it)
    const bool another = pattern.bars.empty() || p.id != pattern.id;
    load (p, isUserPattern);
    if (another && ! p.bars.empty())
        host.setPlaybackTempo (pattern.tempo);
}

void PatternEditor::load (const hz::Pattern& p, bool isUserPattern)
{
    grid->cancelDrag();   // a drag in progress belongs to the old document
    if (pattern.bars.empty() || p.id != pattern.id)   // another pattern: its bars as wide as usual again
    {
        stretch = 1.0f;
        stretchSlider.setValue (100.0, juce::dontSendNotification);
    }
    pattern = p;
    savedContent = p;
    userPattern = isUserPattern;
    blockMode = false;
    cardMode = false;
    applyToBlock = nullptr;
    cardSavedAsNew = nullptr;
    saveButton.setButtonText ("Speichern");
    dirty = false;
    nameUndoPending = false;
    undoStack.clear();
    redoStack.clear();
    mutedVoices.clear();   // a silent lane in the next pattern would only puzzle
    soloVoices.clear();
    nameLabel.setText (juce::String (p.name), juce::dontSendNotification);
    updateLanes();
    updateControls();
    host.auditionPattern (audible());
}

void PatternEditor::updateControls()
{
    int events = 0;
    for (auto& l : pattern.layers)
        events += (int) l.events.size();

    // what is being edited; a library pattern needs no word here
    const juce::String kind = cardMode ? "Karte in der Ablage: " + blockTitle
                            : blockMode ? "Block im Ablauf: " + blockTitle + " (gilt nur in diesem Arrangement)"
                            : userPattern ? juce::String ("Baustein") : juce::String();
    infoLabel.setText (juce::String ((int) pattern.tempo) + " BPM  ·  "_u + juce::String ((int) pattern.bars.size())
                           + (pattern.bars.size() == 1 ? " Takt" : " Takte") + "  ·  "_u + juce::String (events) + " Schläge"_u
                           + (kind.isNotEmpty() ? "  ·  "_u + kind : juce::String())
                           + (dirty ? (blockMode ? "  ·  noch nicht übernommen"_u : "  ·  ungespeichert"_u) : ""),
                       juce::dontSendNotification);

    undoButton.setEnabled (! undoStack.empty());
    redoButton.setEnabled (! redoStack.empty());
    saveButton.setEnabled ((userPattern || blockMode) && dirty);
    deleteButton.setVisible (userPattern && ! blockMode);
    // a block is renamed in the arranger; the tempo slider and the rest of the row move up or make room
    if (const bool showRename = ! blockMode && ! pattern.bars.empty(); renameButton.isVisible() != showRename)
    {
        renameButton.setVisible (showRename);
        resized();
    }
    // swing set, but nothing here it moves (e.g. the grid 1/12 has no straight "und"): the bar line says so
    if (const bool hint = ! pattern.bars.empty() && pattern.swing.ratio > 0.5f && ! swingFindsPlace (pattern); hint != swingHint)
    {
        swingHint = hint;
        grid->repaint();
    }
    closeButton.setEnabled (! pattern.bars.empty());
    help.repaint();   // its default text follows the block mode
    // an empty raster (nothing loaded, or closed): nothing to add to, take over, save or shape
    for (auto* c : { (juce::Component*) &addBarButton, (juce::Component*) &saveAsButton,
                     (juce::Component*) &swingBox, (juce::Component*) &humanizeSlider, (juce::Component*) &humanizeLabel,
                     (juce::Component*) &stretchSlider, (juce::Component*) &stretchLabel, (juce::Component*) &allLanesToggle })
        c->setEnabled (! pattern.bars.empty());

    int best = 0;
    float bestDist = 10.0f;
    for (int i = 0; i < (int) std::size (swingOptions); ++i)
    {
        const float d = std::abs (swingOptions[i].ratio - pattern.swing.ratio)
                      + (swingOptions[i].sub == pattern.swing.subdivision || pattern.swing.ratio <= 0.5f ? 0.0f : 1.0f);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    swingBox.setSelectedItemIndex (best, juce::dontSendNotification);
    humanizeSlider.setValue (pattern.humanize * 100.0, juce::dontSendNotification);
    tempoSlider.setValue (pattern.tempo, juce::dontSendNotification);
    // a block: in a song the song's tempo counts; a card: its tempo matters once it is saved as an own pattern
    tempoSlider.setEnabled (! pattern.bars.empty() && (! blockMode || cardMode));
    tempoSlider.setTooltip (blockMode && ! cardMode ? "Im Arrangement gilt das Arrangement-Tempo, nicht das des einzelnen Blocks."
                                      : "Tempo dieses Bausteins: Damit startet er, wenn man ihn im Hauptfenster wählt. "
                                        "Schieben oder die Zahl anklicken und eintippen. Der Regler im Hauptfenster zieht mit, "
                                        "\"Speichern\" speichert es mit dem Baustein."_u);

    updateWindowTitle();
}

void PatternEditor::updateWindowTitle()
{
    if (! isShowing())
        return;   // the arranger tab is in front: its title shows the song
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setName (pattern.bars.empty() ? juce::String ("Werkstatt")
                                         : "Werkstatt " + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + " " + juce::String (pattern.name) + (dirty ? " *" : ""));
}

bool PatternEditor::hasUnsavedChanges() const
{
    return dirty || grid->isDragging();   // a drag in progress has already changed the pattern
}

void PatternEditor::refreshDirty()
{
    dirty = contentKey (pattern) != contentKey (savedContent);
}

void PatternEditor::refreshAll()
{
    nameLabel.setText (juce::String (pattern.name), juce::dontSendNotification);
    refreshDirty();
    updateLanes();
    updateControls();
    host.auditionPattern (audible());
}

void PatternEditor::changed (const hz::Pattern& before)
{
    if (auto err = pattern.validate(); ! err.empty())
    {
        pattern = before;   // revert an invalid change completely
        refreshAll();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Änderung nicht möglich"_u, juce::String (err));
        return;
    }

    if (contentKey (before) == contentKey (pattern))
        return;   // nothing changed -> no undo step

    undoStack.push_back (before);
    if ((int) undoStack.size() > maxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    nameUndoPending = false;

    refreshDirty();
    updateLanes();
    updateControls();
    host.auditionPattern (audible());
}

void PatternEditor::restoreContent (const hz::Pattern& snapshot)
{
    // identity (id, revision, origin) belongs to the saved document, not to the history
    const auto id = pattern.id;
    const auto revision = pattern.revision;
    const auto origin = pattern.origin;
    pattern = snapshot;
    pattern.id = id;
    pattern.revision = revision;
    pattern.origin = origin;
}

void PatternEditor::undo()
{
    if (undoStack.empty() || grid->isDragging())
        return;
    const double tempoWas = pattern.tempo;
    redoStack.push_back (pattern);
    restoreContent (undoStack.back());
    undoStack.pop_back();
    nameUndoPending = false;
    refreshAll();
    if (std::abs (pattern.tempo - tempoWas) > 0.01 && (! blockMode || cardMode))
        host.setPlaybackTempo (pattern.tempo);   // a tempo change undone/redone: the main tempo follows
}

void PatternEditor::redo()
{
    if (redoStack.empty() || grid->isDragging())
        return;
    const double tempoWas = pattern.tempo;
    undoStack.push_back (pattern);
    restoreContent (redoStack.back());
    redoStack.pop_back();
    nameUndoPending = false;
    refreshAll();
    if (std::abs (pattern.tempo - tempoWas) > 0.01 && (! blockMode || cardMode))
        host.setPlaybackTempo (pattern.tempo);   // a tempo change undone/redone: the main tempo follows
}

bool PatternEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());

    // isCommandDown: Ctrl on Linux/Windows, Cmd on the Mac
    if (mods.isCommandDown() && code == 'Z' && ! mods.isShiftDown()) { undo(); return true; }
    if (mods.isCommandDown() && (code == 'Y' || (code == 'Z' && mods.isShiftDown()))) { redo(); return true; }
    if (mods.isCommandDown() && code == 'S') { save (mods.isShiftDown()); return true; }

    return host.handleTransportKey (key);
}

void PatternEditor::timerCallback()
{
    // repaint the playhead only when it moved
    const auto ph = host.getPlayhead();
    if (ph.playing != lastPlayhead.playing || ph.bar != lastPlayhead.bar || ph.pulse != lastPlayhead.pulse)
    {
        lastPlayhead = ph;
        grid->repaint();
    }

    grid->finishLostDrag();

    // take the keyboard back when the window is active and nobody else needs it
    if (auto* peer = getPeer(); peer != nullptr && peer->isFocused() && ! hasKeyboardFocus (true)
        && juce::Component::getCurrentlyModalComponent() == nullptr)
        grabKeyboardFocus();
}

void PatternEditor::setStretch (double percent, int anchorX)
{
    percent = juce::jlimit (stretchSlider.getMinimum(), stretchSlider.getMaximum(), percent);
    const auto factor = (float) (percent / 100.0);
    stretchSlider.setValue (percent, juce::dontSendNotification);
    if (std::abs (factor - stretch) < 0.0001f)
        return;
    // every column scales by the same ratio: the point of the raster under anchorX stays under it
    const double anchorInGrid = viewport.getViewPositionX() + anchorX;
    const double ratio = factor / stretch;
    stretch = factor;
    grid->layoutFor (viewport.getWidth() - 2);
    viewport.setViewPosition ((int) std::round (anchorInGrid * ratio) - anchorX, viewport.getViewPositionY());   // clamped by the viewport
}

void PatternEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto row1 = area.removeFromTop (34);
    closeButton.setBounds (row1.removeFromRight (100));
    row1.removeFromRight (10);
    nameLabel.setBounds (row1.removeFromLeft (juce::jmin (300, row1.getWidth() / 4)));
    row1.removeFromLeft (6);
    if (renameButton.isVisible())
    {
        renameButton.setBounds (row1.removeFromLeft (110));
        row1.removeFromLeft (10);
    }
    tempoSlider.setBounds (row1.removeFromLeft (200));
    row1.removeFromLeft (10);
    swingBox.setBounds (row1.removeFromLeft (190));   // room for "Achtel triolisch (67 %)"
    row1.removeFromLeft (10);
    humanizeLabel.setBounds (row1.removeFromLeft (90));
    humanizeSlider.setBounds (row1.removeFromLeft (190));   // a little shorter, room for the tempo slider
    row1.removeFromLeft (16);
    stretchLabel.setBounds (row1.removeFromLeft (juce::jmin (60, row1.getWidth())));
    stretchSlider.setBounds (row1.removeFromLeft (juce::jmin (200, row1.getWidth())));   // a narrow window: Strg+Mausrad still works

    area.removeFromTop (8);
    auto row2 = area.removeFromTop (32);
    undoButton.setBounds (row2.removeFromLeft (110));
    row2.removeFromLeft (6);
    redoButton.setBounds (row2.removeFromLeft (110));
    row2.removeFromLeft (10);
    addBarButton.setBounds (row2.removeFromLeft (80));
    row2.removeFromLeft (10);
    allLanesToggle.setBounds (row2.removeFromLeft (120));
    saveAsButton.setBounds (row2.removeFromRight (160));
    row2.removeFromRight (6);
    saveButton.setBounds (row2.removeFromRight (blockMode ? 190 : 110));
    row2.removeFromRight (6);
    deleteButton.setBounds (row2.removeFromRight (90));
    row2.removeFromRight (10);
    infoLabel.setBounds (row2);

    area.removeFromTop (10);
    help.setBounds (getLocalBounds().reduced (12, 6).removeFromBottom (40));
    area.removeFromBottom (46);   // hint line
    if (blockMode)
        area.removeFromBottom (42);   // the "local copy" banner
    area.removeFromLeft (laneNameW);

    // row height grows with the window (easier to hit), within limits
    rowH = juce::jlimit (22, 40, (area.getHeight() - headerH - 16) / juce::jmax (1, (int) lanes.size()));
    viewport.setBounds (area.withHeight (juce::jmin (area.getHeight(), headerH + rowH * (int) lanes.size() + 16)));
    grid->layoutFor (viewport.getWidth() - 2);
}
