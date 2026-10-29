#include "app/PatternEditor.h"

#include "app/EditorGrid.h"
#include "app/Text.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"


//==============================================================================
PatternEditor::PatternEditor (EditorHost& h) : host (h)
{
    using I = hz::Instrument;
    using A = hz::Articulation;
    allLanes = {
        { { I::Crash1, A::Normal },  "Crash 1", true },   { { I::Crash2, A::Normal }, "Crash 2", false },
        { { I::China, A::Normal },   "China", false },    { { I::Splash, A::Normal }, "Splash", false },
        { { I::Ride, A::Normal },    "Ride", true },      { { I::Ride, A::Bell },     "Ride-Glocke", false },
        { { I::HiHat, A::Normal },   "HiHat zu", true },  { { I::HiHat, A::Half },    "HiHat halb", false },
        { { I::HiHat, A::Open },     "HiHat offen", true },
        { { I::Cowbell, A::Normal }, "Kuhglocke", false },
        { { I::Tom1, A::Normal },    "Tom 1", true },     { { I::Tom2, A::Normal },   "Tom 2", false },
        { { I::Tom3, A::Normal },    "Tom 3", true },     { { I::Tom4, A::Normal },   "Tom 4", false },
        { { I::Snare, A::Normal },   "Snare", true },     { { I::Snare, A::Rim },     "Snare Rim", false },
        { { I::Snare, A::Side },     "Sidestick", false },
        { { I::HiHat, A::Pedal },    "HiHat-Pedal", false },
        { { I::Kick, A::Normal },    "Kick", true },
    };

    addAndMakeVisible (nameLabel);
    nameLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, textMain);
    nameLabel.setMinimumHorizontalScale (0.7f);

    for (auto* b : { &undoButton, &redoButton, &addBarButton, &tempoButton, &saveButton, &saveAsButton, &deleteButton,
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
    tempoButton.onClick = [this]
    {
        const auto before = pattern;
        pattern.tempo = host.getCurrentTempo();
        changed (before);
    };
    saveButton.onClick = [this] { save (false); };
    saveAsButton.onClick = [this] { save (true); };
    renameButton.onClick = [this] { askRename(); };
    closeButton.onClick = [this] { closeRequested(); };
    renameButton.setTooltip ("Das Pattern umbenennen (Dialog)");
    closeButton.setTooltip ("Raster leeren, damit nichts aus Versehen geändert wird"_u);
    deleteButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Pattern löschen"_u,
            "Eigenes Pattern \"" + juce::String (pattern.name) + "\" in den Papierkorb verschieben?"_u, "In Papierkorb", "Abbrechen", this,
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

    addAndMakeVisible (infoLabel);
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
    setPattern (p, false);
    blockMode = true;
    blockTitle = title;
    applyToBlock = std::move (apply);
    saveButton.setButtonText ("In Ablauf übernehmen"_u);
    updateControls();
    resized();
}

void PatternEditor::setPattern (const hz::Pattern& p, bool isUserPattern)
{
    grid->cancelDrag();   // a drag in progress belongs to the old document
    pattern = p;
    savedContent = p;
    userPattern = isUserPattern;
    blockMode = false;
    applyToBlock = nullptr;
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

int PatternEditor::laneIndex (hz::Voice v) const
{
    for (int i = 0; i < (int) lanes.size(); ++i)
        if (lanes[(size_t) i].voice == v)
            return i;
    return -1;
}

bool PatternEditor::isMuted (hz::Voice v) const
{
    return std::find (mutedVoices.begin(), mutedVoices.end(), v) != mutedVoices.end();
}

bool PatternEditor::isSolo (hz::Voice v) const
{
    return std::find (soloVoices.begin(), soloVoices.end(), v) != soloVoices.end();
}

hz::Pattern PatternEditor::audible() const
{
    if (mutedVoices.empty() && soloVoices.empty())
        return pattern;
    // solo wins: only soloed lanes sound; otherwise everything but the muted ones
    hz::Pattern p = pattern;
    for (auto& layer : p.layers)
        layer.events.erase (std::remove_if (layer.events.begin(), layer.events.end(), [this] (const hz::PatternEvent& ev)
                            { return soloVoices.empty() ? isMuted (ev.voice) : ! isSolo (ev.voice); }),
                            layer.events.end());
    return p;
}

juce::Rectangle<int> PatternEditor::laneSwitch (int lane, bool solo) const
{
    const auto row = juce::Rectangle<int> (8, viewport.getY() + headerH + lane * rowH, laneNameW - 12, rowH);
    const int size = juce::jmin (18, rowH - 4);
    return { row.getRight() - (solo ? size : 2 * size + 4), row.getCentreY() - size / 2, size, size };
}

void PatternEditor::mouseDown (const juce::MouseEvent& e)
{
    if (pattern.bars.empty())
        return;
    for (int i = 0; i < (int) lanes.size(); ++i)
        for (bool solo : { false, true })
            if (laneSwitch (i, solo).contains (e.getPosition()))
            {
                auto& list = solo ? soloVoices : mutedVoices;
                const auto v = lanes[(size_t) i].voice;
                if (auto it = std::find (list.begin(), list.end(), v); it != list.end())
                    list.erase (it);
                else
                    list.push_back (v);
                host.auditionPattern (audible());
                repaint();
                return;
            }
}

void PatternEditor::updateLanes()
{
    lanes.clear();
    for (const auto& l : allLanes)
    {
        bool used = false;
        for (const auto& layer : pattern.layers)
            for (const auto& e : layer.events)
                used = used || e.voice == l.voice;
        if (l.core || used || allLanesToggle.getToggleState())
            lanes.push_back (l);
    }
    resized();
    repaint();
}

void PatternEditor::updateControls()
{
    int events = 0;
    for (auto& l : pattern.layers)
        events += (int) l.events.size();

    infoLabel.setText (juce::String ((int) pattern.tempo) + " BPM  ·  "_u + juce::String ((int) pattern.bars.size())
                           + (pattern.bars.size() == 1 ? " Takt" : " Takte") + "  ·  "_u + juce::String (events) + " Schläge  ·  "_u
                           + (blockMode ? "Block im Ablauf: " + blockTitle + " (gilt nur in diesem Song)"
                              : userPattern ? juce::String ("eigenes Pattern") : juce::String ("Bibliothek (Speichern unter legt eine Kopie an)"))
                           + (dirty ? (blockMode ? "  ·  noch nicht übernommen"_u : "  ·  ungespeichert"_u) : ""),
                       juce::dontSendNotification);

    undoButton.setEnabled (! undoStack.empty());
    redoButton.setEnabled (! redoStack.empty());
    saveButton.setEnabled ((userPattern || blockMode) && dirty);
    deleteButton.setVisible (userPattern && ! blockMode);
    renameButton.setVisible (! blockMode && ! pattern.bars.empty());   // a block is renamed in the arranger
    closeButton.setEnabled (! pattern.bars.empty());

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

void PatternEditor::clearPattern()
{
    grid->cancelDrag();
    pattern = {};
    savedContent = {};
    userPattern = false;
    blockMode = false;
    applyToBlock = nullptr;
    dirty = false;
    nameUndoPending = false;
    undoStack.clear();
    redoStack.clear();
    saveButton.setButtonText ("Speichern");
    refreshAll();
    resized();
    host.auditionStopped();
}

void PatternEditor::closeRequested()
{
    if (pattern.bars.empty())
        return;
    if (! hasUnsavedChanges())
    {
        clearPattern();
        return;
    }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
        (blockMode ? "Die Änderungen an der Kopie \""_u : "Die Änderungen an \""_u) + juce::String (pattern.name)
            + (blockMode ? "\" sind noch nicht in den Ablauf übernommen. Trotzdem schließen?"_u : "\" sind nicht gespeichert. Trotzdem schließen?"_u),
        "Verwerfen", "Weiter bearbeiten", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PatternEditor> (this)] (int r)
        {
            if (safe != nullptr && r == 1)
                safe->clearPattern();
        }));
}

void PatternEditor::askRename()
{
    if (pattern.bars.empty() || blockMode)
        return;
    auto* w = new juce::AlertWindow ("Pattern umbenennen", "Neuer Name:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", juce::String (pattern.name), "Name");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PatternEditor> (this), w] (int r)
    {
        const auto name = w->getTextEditorContents ("name").trim();
        if (safe == nullptr || r != 1 || name.isEmpty() || name == juce::String (safe->pattern.name))
            return;
        safe->undoStack.push_back (safe->pattern);   // a rename is one undo step
        safe->redoStack.clear();
        safe->pattern.name = name.toStdString();
        safe->refreshAll();
        safe->host.auditionPattern (safe->audible());
    }), true);
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
    redoStack.push_back (pattern);
    restoreContent (undoStack.back());
    undoStack.pop_back();
    nameUndoPending = false;
    refreshAll();
}

void PatternEditor::redo()
{
    if (redoStack.empty() || grid->isDragging())
        return;
    undoStack.push_back (pattern);
    restoreContent (redoStack.back());
    redoStack.pop_back();
    nameUndoPending = false;
    refreshAll();
}

void PatternEditor::askName (std::function<void (juce::String)> onOk)
{
    auto* w = new juce::AlertWindow ("Speichern unter", "Name für das neue Pattern:"_u, juce::MessageBoxIconType::NoIcon, this);
    const auto suggestion = userPattern ? juce::String (pattern.name) : juce::String (pattern.name) + " (eigen)";
    w->addTextEditor ("name", suggestion, "Name");
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int r)
    {
        const auto name = w->getTextEditorContents ("name").trim();
        if (r == 1 && name.isNotEmpty())
            onOk (name);
    }), true);
}

void PatternEditor::save (bool asNew)
{
    if (grid->isDragging())
        return;
    if (! asNew && blockMode)
    {
        // back into the arranger's block, not into the library
        if (! applyToBlock || ! applyToBlock (pattern))
            return;   // rejected changes stay unsaved and can still be corrected in the raster
        savedContent = pattern;
        refreshDirty();
        updateControls();
        return;
    }
    if (! asNew && ! userPattern)
        asNew = true;   // bundled patterns stay unchanged

    // a copy is saved; it is only taken over (name, id, revision, origin) on success
    auto doSave = [safe = juce::Component::SafePointer<PatternEditor> (this)] (bool createNew, juce::String newName)
    {
        if (safe == nullptr)
            return;
        auto candidate = safe->pattern;
        if (newName.isNotEmpty())
            candidate.name = newName.toStdString();

        if (auto err = safe->host.savePattern (candidate, createNew); err.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Speichern fehlgeschlagen", err);
            return;
        }
        safe->pattern = candidate;
        safe->savedContent = candidate;
        safe->userPattern = true;
        safe->nameUndoPending = false;
        safe->nameLabel.setText (juce::String (candidate.name), juce::dontSendNotification);
        safe->refreshDirty();
        safe->updateControls();
    };

    if (asNew)
        askName ([doSave] (juce::String name) { doSave (true, name); });
    else
        doSave (false, {});
}

void PatternEditor::editBar (int bar, std::function<void (hz::Pattern&)> edit)
{
    if (bar < 0 || bar >= (int) pattern.bars.size() || grid->isDragging())
        return;
    const auto before = pattern;
    edit (pattern);
    changed (before);
}

void PatternEditor::showBarMenu (int bar)
{
    using Safe = juce::Component::SafePointer<PatternEditor>;
    auto onBar = [safe = Safe (this), bar] (std::function<void (hz::Pattern&)> f)
    {
        return [safe, bar, f] { if (safe != nullptr) safe->editBar (bar, f); };
    };

    juce::PopupMenu m;
    m.addSectionHeader ("Takt " + juce::String (bar + 1));
    m.addItem ("Taktart ...", [safe = Safe (this), bar] { if (safe != nullptr) safe->showMeterMenuForBar (bar); });
    m.addItem ("Raster ...", [safe = Safe (this), bar] { if (safe != nullptr) safe->showGridMenuForBar (bar); });
    m.addSeparator();
    m.addItem ("Takt dahinter einfügen (Kopie)"_u, onBar ([bar] (hz::Pattern& p) { hz::insertBar (p, bar, true); }));
    m.addItem ("Takt dahinter einfügen (leer)"_u, onBar ([bar] (hz::Pattern& p) { hz::insertBar (p, bar, false); }));
    m.addItem ("Takt löschen"_u, pattern.bars.size() > 1, false, onBar ([bar] (hz::Pattern& p) { hz::removeBar (p, bar); }));
    m.addSeparator();
    juce::PopupMenu clear;
    for (const auto& l : lanes)
        clear.addItem (l.name, onBar ([bar, v = l.voice] (hz::Pattern& p) { hz::clearLane (p, bar, v); }));
    m.addSubMenu ("Spur in diesem Takt leeren", clear);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void PatternEditor::showMeterMenuForBar (int bar)
{
    if (bar < 0 || bar >= (int) pattern.bars.size())
        return;

    using Safe = juce::Component::SafePointer<PatternEditor>;
    auto& meters = host.getMeters();
    const int current = meters.findOrAdd (pattern.bars[(size_t) bar].meter);
    auto setMeter = [safe = Safe (this), bar, &meters] (int i)
    {
        if (safe != nullptr)
            safe->editBar (bar, [&meters, i, bar] (hz::Pattern& p) { hz::setBarMeter (p, bar, meters.get (i)); });
    };

    MeterLibrary::MenuCallbacks cb;
    cb.select = setMeter;
    cb.custom = [safe = Safe (this), bar, &meters, setMeter]
    {
        if (safe != nullptr && bar < (int) safe->pattern.bars.size())
            meters.askCustomMeter (safe.getComponent(), meters.indexOf (safe->pattern.bars[(size_t) bar].meter), setMeter);
    };
    cb.toggleFavorite = [&meters] (int i) { meters.toggleFavorite (i); };
    meters.buildMenu (current, cb).showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void PatternEditor::showGridMenuForBar (int bar)
{
    if (bar < 0 || bar >= (int) pattern.bars.size())
        return;

    juce::PopupMenu m;
    const auto& b = pattern.bars[(size_t) bar];
    for (const auto& o : gridOptions)
    {
        const bool fits = (b.lengthInQuarters() / (o.grid * hz::Rational (4))).den == 1;
        m.addItem (juce::String (juce::CharPointer_UTF8 (o.label)), fits, o.grid == b.grid,
                   [safe = juce::Component::SafePointer<PatternEditor> (this), bar, g = o.grid]
                   {
                       if (safe != nullptr)
                           safe->editBar (bar, [bar, g] (hz::Pattern& p) { hz::setBarGrid (p, bar, g); });
                   });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
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

    // take the keyboard back when the window is active and nobody else needs it
    if (auto* peer = getPeer(); peer != nullptr && peer->isFocused() && ! hasKeyboardFocus (true)
        && juce::Component::getCurrentlyModalComponent() == nullptr)
        grabKeyboardFocus();
}

void PatternEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    if (pattern.bars.empty())   // nothing loaded yet: say how to get a pattern in here
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (17.0f));
        g.drawFittedText ("Kein Pattern geladen.\n\nIm Hauptfenster ein Pattern wählen und E drücken (oder \"Bearbeiten\"),\n"
                          "oder im Arranger einen Block wählen und \"Bearbeiten\" klicken (auch per Rechtsklick).\n"
                          "Dann erscheint es hier im Raster."_u,
                          viewport.getBounds().reduced (20), juce::Justification::centred, 5);
        return;
    }

    // lane names on the left, aligned with the grid rows, with the M / S switches at their end
    const int top = viewport.getY() + headerH;
    for (int i = 0; i < (int) lanes.size(); ++i)
    {
        const auto voice = lanes[(size_t) i].voice;
        const bool muted = isMuted (voice), solo = isSolo (voice);
        const bool silent = soloVoices.empty() ? muted : ! solo;
        auto r = juce::Rectangle<int> (8, top + i * rowH, laneNameW - 12, rowH);
        g.setColour (theme::voiceColour (voice).withAlpha (silent ? 0.35f : 1.0f));
        g.fillRoundedRectangle (r.removeFromLeft (5).reduced (0, 5).toFloat(), 2.0f);
        g.setColour (silent ? textDim : textMain);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (lanes[(size_t) i].name, r.withTrimmedLeft (6).withTrimmedRight (2 * laneSwitch (i, false).getWidth() + 10),
                    juce::Justification::centredLeft);
        for (bool s : { false, true })
        {
            const auto box = laneSwitch (i, s);
            const bool on = s ? solo : muted;
            g.setColour (on ? (s ? juce::Colour (0xff2e7a4a) : juce::Colour (0xffb0552a)) : juce::Colour (0xff3a3a44));
            g.fillRoundedRectangle (box.toFloat(), 3.0f);
            g.setColour (on ? textMain : textDim);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (s ? "S" : "M", box, juce::Justification::centred);
        }
    }

    if (blockMode)   // unmistakable: this is the song's own copy, not the library pattern
    {
        auto banner = getLocalBounds().reduced (12, 6).withTrimmedBottom (46).removeFromBottom (36);
        g.setColour (juce::Colour (0xff2a3f5a));
        g.fillRoundedRectangle (banner.toFloat(), 6.0f);
        g.setColour (textMain);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText ("Lokale Kopie: Block \"" + blockTitle + "\" aus dem Ablauf. Änderungen gelten nur in diesem Song. "
                          "\"In Ablauf übernehmen\" schreibt sie in den Block, \"Speichern unter\" macht daraus ein eigenes Pattern."_u,
                          banner.reduced (12, 0), juce::Justification::centredLeft, 2);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("Klick: Schlag setzen/löschen  ·  Shift+Klick: Akzent  ·  Alt+Klick: Ghost  ·  hoch/runter ziehen: Lautstärke\n"
                      "Rechtsklick: Ghost/Normal/Akzent  ·  M/S an der Spur: stumm/solo  ·  Kopfzeile: Taktart, Raster, Takt-Menü  ·  Strg+Z/Y  ·  Strg+S "_u
                      + (blockMode ? juce::String ("in Ablauf übernehmen"_u) : juce::String ("speichern")),
                      getLocalBounds().reduced (12, 6).removeFromBottom (40), juce::Justification::centredLeft, 2);
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
    tempoButton.setBounds (row1.removeFromLeft (150));
    row1.removeFromLeft (10);
    swingBox.setBounds (row1.removeFromLeft (160));
    row1.removeFromLeft (10);
    humanizeLabel.setBounds (row1.removeFromLeft (90));
    humanizeSlider.setBounds (row1.removeFromLeft (220));

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
    area.removeFromBottom (46);   // hint line
    if (blockMode)
        area.removeFromBottom (42);   // the "local copy" banner
    area.removeFromLeft (laneNameW);

    // row height grows with the window (easier to hit), within limits
    rowH = juce::jlimit (22, 40, (area.getHeight() - headerH - 16) / juce::jmax (1, (int) lanes.size()));
    viewport.setBounds (area.withHeight (juce::jmin (area.getHeight(), headerH + rowH * (int) lanes.size() + 16)));
    grid->layoutFor (viewport.getWidth() - 2);
}
