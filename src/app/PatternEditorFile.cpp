// PatternEditor, part 3: closing, renaming, saving (also "Speichern unter" with modN) and the bar menus.

#include "app/PatternEditor.h"

#include "app/EditorGrid.h"
#include "app/Text.h"
#include "app/views/InWindowDialog.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"

void PatternEditor::clearPattern()
{
    grid->cancelDrag();
    pattern = {};
    savedContent = {};
    userPattern = false;
    blockMode = false;
    cardMode = false;
    applyToBlock = nullptr;
    cardSavedAsNew = nullptr;
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
            + (cardMode ? "\" sind noch nicht in die Ablage übernommen. Trotzdem schließen?"_u
               : blockMode ? "\" sind noch nicht in den Ablauf übernommen. Trotzdem schließen?"_u : "\" sind nicht gespeichert. Trotzdem schließen?"_u),
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
    auto* w = new InWindowDialog ("Baustein umbenennen", "Neuer Name:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", juce::String (pattern.name), "Name");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PatternEditor> (this), w] (int r)
    {
        if (safe == nullptr || r != 1)
            return;   // read the dialog only when OK was pressed: on cancel it may already be gone
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty() || name == juce::String (safe->pattern.name))
            return;
        safe->undoStack.push_back (safe->pattern);   // a rename is one undo step
        safe->redoStack.clear();
        safe->pattern.name = name.toStdString();
        safe->refreshAll();
        safe->host.auditionPattern (safe->audible());
    }), true);
}

juce::String PatternEditor::variantName() const
{
    // "Name modN": the base without an earlier " modN" or the old " (eigen)", N the first number not in use
    auto base = juce::String (pattern.name).trim();
    if (base.endsWith (" (eigen)"))
        base = base.dropLastCharacters (8).trimEnd();
    if (const int m = base.lastIndexOf (" mod"); m > 0 && base.substring (m + 4).isNotEmpty()
                                                 && base.substring (m + 4).containsOnly ("0123456789"))
        base = base.substring (0, m).trimEnd();
    for (int n = 1;; ++n)
        if (const auto name = base + " mod" + juce::String (n); ! host.patternNameTaken (name))
            return name;
}

void PatternEditor::askName (std::function<void (juce::String)> onOk)
{
    auto* w = new InWindowDialog ("Speichern unter", "Name für den neuen Baustein:"_u, juce::MessageBoxIconType::NoIcon, this);
    const auto suggestion = variantName();
    w->addTextEditor ("name", suggestion, "Name");
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int r)
    {
        if (r != 1)
            return;   // read the dialog only when OK was pressed: on cancel it may already be gone
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            onOk (name);
    }), true);
}

void PatternEditor::save (bool asNew)
{
    if (grid->isDragging() || pattern.bars.empty())   // a pattern without bars is no pattern (also Ctrl+S)
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
        if (createNew && safe->cardMode)
        {
            // saved from a card as an own pattern: it goes into the Ablage next to the card,
            // and from now on the raster edits that pattern (the undo history stays)
            if (safe->cardSavedAsNew)
                safe->cardSavedAsNew (candidate);
            safe->blockMode = safe->cardMode = false;
            safe->applyToBlock = nullptr;
            safe->cardSavedAsNew = nullptr;
            safe->saveButton.setButtonText ("Speichern");
            safe->resized();   // no banner any more
            safe->repaint();
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
    auto setGrid = [safe = juce::Component::SafePointer<PatternEditor> (this), bar] (hz::Rational g)
    {
        if (safe != nullptr)
            safe->editBar (bar, [bar, g] (hz::Pattern& p) { hz::setBarGrid (p, bar, g); });
    };
    // some hits have no place in this grid (diamonds): first the coarsest grid that has one for every hit
    if (! gridHoldsHits (pattern, bar, b.grid))
    {
        if (const auto fit = fittingGrid (pattern, bar); fit.num > 0)
            m.addItem ("Passend: " + juce::String (fit.num) + "/" + juce::String (fit.den) + " (alle Schläge in Kästchen)"_u,
                       [setGrid, fit] { setGrid (fit); });
        else
            m.addItem ("Kein passendes Raster", false, false, [] {});
        m.addSeparator();
    }
    // swing set, but no place anywhere for it (the hint behind the grid of bar 1): every bar gets the coarsest
    // grid that keeps its hits in cells and has places the swing moves, all in one undo step
    if (swingHint)
    {
        juce::StringArray grids;
        int fixable = 0;
        for (int bi = 0; bi < (int) pattern.bars.size(); ++bi)
            if (const auto g = fittingGrid (pattern, bi, true); g.num > 0)
            {
                ++fixable;
                grids.addIfNotAlreadyThere (juce::String (g.num) + "/" + juce::String (g.den));
            }
        const int bars = (int) pattern.bars.size();
        if (fixable == 0)
            m.addItem ("Kein passendes Raster für Swing"_u, false, false, [] {});
        else
            m.addItem ("Passend für Swing: "_u + grids.joinIntoString (", ")
                           + (bars == 1 ? juce::String() : fixable == bars ? juce::String (" (alle Takte)")
                                                         : " (" + juce::String (fixable) + (fixable == 1 ? " Takt)" : " Takte)")),
                       [safe = juce::Component::SafePointer<PatternEditor> (this), bar]
                       {
                           if (safe != nullptr)
                               safe->editBar (bar, [] (hz::Pattern& p)
                               {
                                   for (int bi = 0; bi < (int) p.bars.size(); ++bi)
                                       if (const auto g = fittingGrid (p, bi, true); g.num > 0)
                                           hz::setBarGrid (p, bi, g);
                               });
                       });
        m.addSeparator();
    }
    for (const auto& o : gridOptions)
    {
        const bool fits = (b.lengthInQuarters() / (o.grid * hz::Rational (4))).den == 1;
        m.addItem (juce::String (juce::CharPointer_UTF8 (o.label)), fits, o.grid == b.grid, [setGrid, g = o.grid] { setGrid (g); });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}
